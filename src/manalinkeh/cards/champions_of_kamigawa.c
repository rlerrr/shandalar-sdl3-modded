#include "manalink.h"

// Global functions

int bushido(int player, int card, event_t event, int pump){

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL && pump > 0 ){
		instance->targets[8].player = pump;
	}

	if( instance->targets[16].card < 0 ){
		instance->targets[16].card = 0;
	}

	if( ! (instance->targets[16].card & SP_KEYWORD_BUSHIDO) ){
		instance->targets[16].card |= SP_KEYWORD_BUSHIDO;
	}

	if( instance->targets[8].player > 0 && !is_humiliated(player, card) ){
		if( event == EVENT_DECLARE_BLOCKERS ){
			if( (instance->state & STATE_ATTACKING) && ! is_unblocked(player, card) ){
				pump_until_eot(player, card, player, card, instance->targets[8].player, instance->targets[8].player);
			}
			else if( current_turn != player && blocking(player, card, event) ){
				pump_until_eot(player, card, player, card, instance->targets[8].player, instance->targets[8].player);
			}
		}
		if( affect_me(player, card) && event == EVENT_CHECK_PUMP ){
			pumpable_power[player]+=instance->targets[8].player;
			pumpable_toughness[player]+=instance->targets[8].player;
		}
	}

	return 0;
}

static int get_zubera_count(void){
	int amount = get_trap_condition(HUMAN, TRAP_DEAD_ZUBERA_COUNT) + get_trap_condition(AI, TRAP_DEAD_ZUBERA_COUNT);
	return amount;
}

void splice_onto_arcane(int player, int card)
{
  // When cast, check the hand for splice spells.
  /* Storage allocation for all arcane spells:
   * targets[0]: Target of the original spell.
   * targets[1], [2], [3], [4], [5], [6]: Targets of spliced spell #1, #2, #3, #4, #5, and #6.
   * targets[7].player, [7].card, [8].player, [8].card, [9].player, [9].card: Low 3 bytes are the csvids of spliced-on spells #1, #2, #3, #4, #5, and #6
   *            respectively; high byte is the card indices of those spells.
   * targets[10]: unused
   * targets[11]-[19]: Used for global effects.
   * eot_toughness: Number of spliced spells.  (So splicing two spells results in eot_toughness == 2.)
   * info_slot: Reserved for spliced-onto spell's use.  In practice, can only be used with Arcane spells that can't be spliced (or else they'd overwrite the
   *            info_slot of the card they're spliced onto). */

  card_instance_t* instance = get_card_instance(player, card);
  if (IS_AI(player))
	return;

  struct
  {
	int csvid;
	uint8_t card;
  } options[151];

  int i;
  int opt_count = 1;
  card_instance_t* inst;
  for (i = 0; i < active_cards_count[player]; ++i)
	if ((inst = in_hand(player, i)))
	  {
		CardFunction fn = cards_data[inst->internal_card_id].code_pointer;
		if (call_card_fn(fn, instance, player, card, EVENT_CAN_SPLICE))
		  {
			options[opt_count].csvid = get_id(player, i);
			options[opt_count].card = i;
			++opt_count;
		  }
	  }

  instance->eot_toughness = 0;
  int* data = &instance->targets[7].player;
  ASSERT(&data[5] == &instance->targets[9].card);

  int spliced = 0;
  while (spliced < 6 && opt_count > 1)
	{
	  char buffer[600];
	  int pos = scnprintf(buffer, 600, " Done\n");
	  for (i = 1; i < opt_count; ++i)
		pos += scnprintf(buffer + pos, 600 - pos, " Splice %s\n", cards_ptr[options[i].csvid]->name);

	  int choice = do_dialog(player, player, card, -1, -1, buffer, 0);
	  if (choice == 0)
		break;

	  hack_prepend_prompt = cards_ptr[options[choice].csvid]->name;

	  CardFunction fn = cards_data[get_internal_card_id_from_csv_id(options[choice].csvid)].code_pointer;
	  call_card_fn(fn, instance, player, card, EVENT_SPLICE);

	  hack_prepend_prompt = NULL;

	  if (spell_fizzled == 1)
		{
		  spell_fizzled = 0;
		  continue;
		}

	  instance->targets[spliced + 1] = instance->targets[0];	// struct copy
	  data[spliced] = options[choice].csvid;
	  SET_BYTE3(data[spliced]) = options[choice].card;
	  ++instance->eot_toughness;
	  ++spliced;

	  for (i = choice; i < opt_count; ++i)
		options[i] = options[i + 1];	// struct copy

	  --opt_count;

	  for (i = 1; i < opt_count; ++i)
		if (!((inst = in_hand(player, options[i].card))
			  && call_card_fn(cards_data[inst->internal_card_id].code_pointer, instance, player, card, EVENT_CAN_SPLICE)))
		  {
			int j;
			for (j = i; j < opt_count; ++j)
			  options[j] = options[j + 1];	// struct copy

			--opt_count;
			--i;	// So we test the card that was just moved into this slot
		  }
	}

  instance->number_of_targets = 0;
}

void check_for_spliced_spells(int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  int* data = &instance->targets[7].player;
  int i, old_number_of_targets = instance->number_of_targets;
  for (i = 0; i < (int)instance->eot_toughness; ++i)
	if (data[i] != -1)
	  {
		// A hack to make the spliced spell see its target in targets[0].
		instance->targets[0] = instance->targets[i + 1];	// struct copy
		instance->number_of_targets = instance->targets[0].player >= 0 && instance->targets[0].card >= 0 ? 1 : 0;

		/* A truly horrible hack to make any effect cards created by the spliced spell have that spell's image and text, while still using the spliced-onto
		 * spell for target validation. */
		HackForceEffectChangeSource old_hack_force_effect_change_source = hack_force_effect_change_source;	// struct copy

		hack_force_effect_change_source.from.player = player;
		hack_force_effect_change_source.from.card = card;
		hack_force_effect_change_source.to.player = is_stolen(player, card) ? 1-player : player;
		hack_force_effect_change_source.to.card = BYTE3(data[i]);

		CardFunction fn = cards_data[get_internal_card_id_from_csv_id(data[i] & 0x00FFFFFF)].code_pointer;
		call_card_fn(fn, instance, player, card, EVENT_RESOLVE_SPLICE);

		hack_force_effect_change_source = old_hack_force_effect_change_source;	// struct copy
	  }

  instance->number_of_targets = old_number_of_targets;
}

/* A handy frontend to Arcane instants and sorceries that also have Splice onto Arcane.  fn() is almost a normal card function; the only difference is that it
 * shouldn't call kill_card(player, card, KILL_DESTROY) during EVENT_RESOLVE_SPELL.  Even handier when called via the ARCANE_WITH_SPLICE macro.
 * If x == -10, then send EVENT_CAN_SPLICE and EVENT_SPLICE to the supplied function; it should respond only by checking for and charging the splice cost
 * respectively, returning 1 on success or 0 on failure.  This differs from the normal EVENT_CAN_SPLICE/EVENT_SPLICE behavior. */
int arcane_with_splice(int player, int card, int event, int (*fn)(int,int,event_t), int x, int b, int u, int g, int r, int w)
{
  switch (event)
	{
	  case EVENT_CAN_CAST:
		return fn(player, card, event);

	  case EVENT_CAN_SPLICE:
		if (x == -10 ? fn(player, card, EVENT_CAN_SPLICE) : has_mana_multi(player, x, b, u, g, r, w))
		  return fn(player, card, EVENT_CAN_CAST);
		else
		  return 0;

	  case EVENT_CAST_SPELL:
		if (!affect_me(player, card))
		  return 0;
		splice_onto_arcane(player, card);
		if (get_card_instance(player, card)->eot_toughness > 0)
		  hack_prepend_prompt = get_card_name_by_id(get_id(player, card));
		int cast_rval = fn(player, card, event);
		hack_prepend_prompt = NULL;
		return cast_rval;

	  case EVENT_SPLICE:
		if (!(x == -10 ? fn(player, card, EVENT_SPLICE) : charge_mana_multi(player, x, b, u, g, r, w)))
		  {
			spell_fizzled = 1;
			return 0;
		  }

		// Temporary assign affected_card* so we can use the usual EVENT_CAST_SPELL && affect_me(player, card) form in client code
		int old_affected_card_controller = affected_card_controller, old_affected_card = affected_card;

		affected_card_controller = player;
		affected_card = card;

		int splice_rval = fn(player, card, EVENT_CAST_SPELL);

		affected_card_controller = old_affected_card_controller;
		affected_card = old_affected_card;

		return splice_rval;

	  case EVENT_RESOLVE_SPELL:
		fn(player, card, event);
		check_for_spliced_spells(player, card);
		kill_card(player, card, KILL_DESTROY);
		return 0;

	  case EVENT_RESOLVE_SPLICE:
		return fn(player, card, EVENT_RESOLVE_SPELL);

	  default:
		return fn(player, card, event);
	}
}

// As arcane_with_splice, but can't be spliced itself.
int arcane(int player, int card, int event, int (*fn)(int,int,event_t))
{
  switch (event)
	{
	  case EVENT_CAN_CAST:
		return fn(player, card, event);

	  case EVENT_CAST_SPELL:
		if (!affect_me(player, card))
		  return 0;
		splice_onto_arcane(player, card);
		if (get_card_instance(player, card)->eot_toughness > 0)
		  hack_prepend_prompt = get_card_name_by_id(get_id(player, card));
		int cast_rval = fn(player, card, event);
		hack_prepend_prompt = NULL;
		return cast_rval;

	  case EVENT_RESOLVE_SPELL:
		fn(player, card, event);
		check_for_spliced_spells(player, card);
		kill_card(player, card, KILL_DESTROY);
		return 0;

	  default:
		return fn(player, card, event);
	}
}

int soulshift(int player, int card, event_t event, int cmc, int num){

	if( this_dies_trigger(player, card, event, 2) ){
		char msg[100];
		scnprintf(msg, 100, "Select a Spirit card with cmc %d or less.", cmc);
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, msg);
		this_test.subtype = SUBTYPE_SPIRIT;
		this_test.subtype_flag = MATCH;
		this_test.cmc = cmc+1;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		for (; num > 0 && new_special_count_grave(player, &this_test); --num){
			if( new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test) == -1 ){
				break;
			}
		}
	}

	return 0;
}

int arcane_spirit_spell_trigger(int player, int card, event_t event, int trigger_mode){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller ){
		test_definition_t test;
		default_test_definition(&test, TYPE_NONEFFECT);
		test.subtype = SUBTYPE_ARCANE;
		test.sub2 = SUBTYPE_SPIRIT;
		test.subtype_flag = F2_MULTISUBTYPE;

		return new_specific_spell_played(player, card, event, player, trigger_mode, &test);
	}

	return 0;
}

static int myojin(int player, int card, event_t event){

	check_legend_rule(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance( player, card )->info_slot = not_played_from_hand(player, card);
	}

	if ( ! get_card_instance( player, card )->info_slot){
		enters_the_battlefield_with_counters(player, card, event, COUNTER_DIVINITY, 1);
	}

	if( count_counters(player, card, COUNTER_DIVINITY) > 0 ){
		indestructible(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTER(COUNTER_DIVINITY), NULL, NULL);
	}

	return 0;
}

// cards

int card_akki_coalflinger(int player, int card, event_t event){
/* CARD_ID_AKKI_COALFLINGER	7153
Akki Coalflinger	|1|R|R
Creature - Goblin Shaman 2/2
First strike
|R, |T: Attacking creatures gain first strike until end of turn. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.state = STATE_ATTACKING;
		pump_creatures_until_eot(instance->parent_controller, instance->parent_card, player, 0, 0, 0, KEYWORD_FIRST_STRIKE,0, &test);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_R(1), 0, NULL, NULL);
}

int card_ashen_skin_zubera(int player, int card, event_t event){
/* CARD_ID_ASHEN_SKIN_ZUBERA	7158
Ashen-Skin Zubera	|1|B
Creature - Zubera Spirit 1/2
When ~ dies, target opponent discards a card for each Zubera that died this turn. */

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;

		if( would_validate_target(player, card, &td, 0) ){
			new_multidiscard(instance->targets[0].player, get_zubera_count(), 0, player);
		}
	}

	return 0;
}

int card_azami_lady_of_scrolls(int player, int card, event_t event){
/* CARD_ID_AZAMI_LADY_OF_SCROLLS	7160
Azami, Lady of Scrolls	|2|U|U|U
Legendary Creature - Human Wizard 0/2
Tap an untapped Wizard you control: Draw a card. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_state = TARGET_STATE_TAPPED;
	td.required_subtype = SUBTYPE_WIZARD;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td) ){
		return generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( new_pick_target(&td, "Select an untapped Wizard you control.", 0, 1) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_azusa_lost_but_seeking(int player, int card, event_t event){
/* CARD_ID_AZUSA_LOST_BUT_SEEKING	7161
Azusa, Lost but Seeking	|2|G
Legendary Creature - Human Monk 1/2
You may play two additional lands on each of your turns. */

// The effect is in "total_playable_lands" function in "functions.c"

	check_playable_lands(player);
	check_legend_rule(player, card, event);
	return 0;
}

int card_battle_mad_ronin(int player, int card, event_t event){
/* CARD_ID_BATTLE_MAD_RONIN	7162
Battle-Mad Ronin	|1|R
Creature - Human Samurai 1/1
Bushido 2 (Whenever this creature blocks or becomes blocked, it gets +2/+2 until end of turn.)
~ attacks each turn if able. */

	attack_if_able(player, card, event);
	return bushido(player, card, event, 2);
}

int card_ben_ben_akki_hermit(int player, int card, event_t event){
/* CARD_ID_BEN_BEN_AKKI_HERMIT	7163
Ben-Ben, Akki Hermit	|2|R|R
Legendary Creature - Goblin Shaman 1/1
|T: ~ deals damage to target attacking creature equal to the number of untapped |H1Mountains you control. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, count_subtype(player, TYPE_LAND, SUBTYPE_MOUNTAIN));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ATTACKING_CREATURE");
}

ARCANE_WITH_SPLICE(card_blessed_breath, MANACOST_W(1)){
/* CARD_ID_BLESSED_BREATH	7164
Blessed Breath	|W
Instant - Arcane
Target creature you control gains protection from the color of your choice until end of turn.
Splice onto Arcane |W (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = td.allowed_controller = player;

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "ASHNODS_BATTLEGEAR", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				card_instance_t *instance = get_card_instance(player, card);
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0,
									   select_a_protection(player) | KEYWORD_RECALC_SET_COLOR, 0);
			}
	}
	return 0;
}

int card_blood_speaker(int player, int card, event_t event){
/* CARD_ID_BLOOD_SPEAKER	7167
Blood Speaker	|3|B
Creature - Ogre Shaman 3/2
At the beginning of your upkeep, you may sacrifice ~. If you do, search your library for a Demon card, reveal that card, put it into your hand, then shuffle your library.
Whenever a Demon enters the battlefield under your control, return ~ from your graveyard to your hand. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "Select a Demon card.");
		this_test.subtype = SUBTYPE_DEMON;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_SACRIFICE);
	}

	return 0;
}

int card_bloodthirsty_ogre(int player, int card, event_t event){
/* CARD_ID_BLOODTHIRSTY_OGRE	7168
Bloodthirsty Ogre	|2|B
Creature - Ogre Warrior Shaman 3/1
|T: Put a devotion counter on ~.
|T: Target creature gets -X/-X until end of turn, where X is the number of devotion counters on ~. Activate this ability only if you control a Demon.*/


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_WEAKEN
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		if( count_counters(player, card, COUNTER_DEVOTION) < 3 ){
			if( current_turn != player && current_phase > PHASE_DECLARE_BLOCKERS ){
				ai_modifier += 25;
			}
			else{
				if( count_counters(player, card, COUNTER_DEVOTION) == 0 ){
					ai_modifier -= 25;
				}
			}
		}
		int ab2 = can_target(&td) && check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON) ? 1 : 0;
		int priority_ab1 = current_turn != player && current_phase > PHASE_DECLARE_BLOCKERS ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add Devotion counter", 1, priority_ab1,
							"Give -X/-X", ab2, 5);

		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( choice == CHOICE_WEAKEN ){
				pick_target(&td, "TARGET_CREATURE");
			}
			if( spell_fizzled != 1 ){
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_DEVOTION);
		}
		if( instance->info_slot == CHOICE_WEAKEN && valid_target(&td) ){
			int amount = count_counters(instance->parent_controller, instance->parent_card, COUNTER_DEVOTION);
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -amount, -amount);
		}
	}

	return 0;
}

int card_brothers_yamazaki(int player, int card, event_t event){
      /* Brothers Yamazaki |2|R
	  Legendary Creature — Human Samurai 2/1
	  Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)
	  If there are exactly two permanents named Brothers Yamazaki on the battlefield, the "legend rule" doesn't apply to them.
	  Each other creature named Brothers Yamazaki gets +2/+2 and has haste.	*/

	if( event == EVENT_STATIC_EFFECTS && ! is_humiliated(player, card) ){
		if( check_battlefield_for_id(ANYBODY, CARD_ID_MIRROR_GALLERY) ){
			return 0;
		}
		int this_card = get_card_name(player, card);
		int marked[2][151] = {{0}};
		int mc[2] = {0};
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && get_card_name(p, c) == this_card ){
										marked[p][c] = 1;
										mc[p]++;
									}

								};
		);

		if( mc[0] + mc[1] > 2 ){
			APNAP(p,{
						if( mc[p] > 1 ){
							target_definition_t td;
							default_target_definition(player, card, &td, TYPE_PERMANENT);
							td.who_chooses = p;
							td.allowed_controller = td.preferred_controller = p;
							td.illegal_abilities = 0;
							td.extra = get_internal_card_id_from_csv_id(this_card);
							td.allow_cancel = 0;

							char buffer[100];
							scnprintf(buffer, 100, "Special Legend Rule: selec a %s permanent to keep.", cards_ptr[this_card]->name);
							if( new_pick_target(&td, buffer, 0, GS_LITERAL_PROMPT) ){
								card_instance_t* instance = get_card_instance(player, card);
								marked[instance->targets[0].player][instance->targets[0].card] = 0;
							}
							get_card_instance(player, card)->number_of_targets = 0;

							int c;
							for(c = 150; c > -1; c--){
								if( marked[p][c] == 1 ){
									kill_card(p, marked[p][c], KILL_SACRIFICE);
								}
							}
						}
					};
			);
		}
	}

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && in_play(player, card) &&
		! is_humiliated(player, card) )
	{
		if( ! affect_me(player, card) && get_card_name(affected_card_controller, affected_card) == get_card_name(player, card) ){
			switch( event ){
					case EVENT_POWER:
					case EVENT_TOUGHNESS:
						event_result+=2;
						break;
					case EVENT_ABILITIES:
						haste(affected_card_controller, affected_card, event);
						break;
					default:
						break;
			}
		}
	}

	return bushido(player, card, event, 1);
}

int card_budoka_gardener(int player, int card, event_t event){
/* CARD_ID_BUDOKA_GARDENER	7172
Budoka Gardener	|1|G
Creature - Human Monk 2/1
|T: You may put a land card from your hand onto the battlefield. If you control ten or more lands, flip ~.
--FLIP--
Dokai, Weaver of Life */

	double_faced_card(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to put into play.");
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		if( count_permanents_by_type(player, TYPE_LAND) >= 10 ){
			true_transform(instance->parent_controller, instance->parent_card);
			verify_legend_rule(instance->parent_controller, instance->parent_card, get_id(instance->parent_controller, instance->parent_card));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_dokai_weaver_of_life(int player, int card, event_t event){
	/* Dokai, Weaver of Life	|1|G (flipped)
	 * Legendary Creature - Human Monk 3/3
	 * |4|G|G, |T: Put an X/X |Sgreen Elemental creature token onto the battlefield, where X is the number of lands you control. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_ELEMENTAL, &token);
		token.pow = count_subtype(player, TYPE_LAND, -1);
		token.tou = count_subtype(player, TYPE_LAND, -1);
		token.color_forced = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XG(4,2), 0, NULL, NULL);
}

int card_burr_grafter(int player, int card, event_t event){
/* CARD_ID_BURR_GRAFTER	7174
Burr Grafter	|3|G
Creature - Spirit 2/2
Sacrifice ~: Target creature gets +2/+2 until end of turn.
Soulshift 3 (When this creature dies, you may return target Spirit card with converted mana cost 3 or less from your graveyard to your hand.) */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
			}
		}
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
	}

	return soulshift(player, card, event, 3, 1);
}

int card_bushi_tenderfoot(int player, int card, event_t event){
/* CARD_ID_BUSHI_TENDERFOOT	7175
Bushi Tenderfoot	|W
Creature - Human Soldier 1/1
When a creature dealt damage by ~ this turn dies, flip ~.
--FLIP--
Kenzo the Hardhearted */

	double_faced_card(player, card, event);

	if( sengir_vampire_trigger(player, card, event, 2) ){
		true_transform(player, card);
		verify_legend_rule(player, card, get_id(player, card));
	}
	return 0;
}

int card_kenzo_the_hardhearted(int player, int card, event_t event){
/* CARD_ID_KENZO_THE_HARDHEARTED	7176 flipped
Kenzo the Hardhearted	|W
Legendary Creature - Human Samurai 3/4
Double strike; bushido 2 (Whenever this creature blocks or becomes blocked, it gets +2/+2 until end of turn.) */

	check_legend_rule(player, card, event);
	return bushido(player, card, event, 2);
}

int card_cage_of_hands(int player, int card, event_t event){
/* CARD_ID_CAGE_OF_HANDS	7177
Cage of Hands	|2|W
Enchantment - Aura
Enchant creature
Enchanted creature can't attack or block.
|1|W: Return ~ to its owner's hand. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		cannot_attack(t_player, t_card, event);
		cannot_block(t_player, t_card, event);

		if( IS_GAA_EVENT(event) ){
			if( event == EVENT_RESOLVE_ACTIVATION ){
				bounce_permanent(instance->parent_controller, instance->parent_card);
			}
			return generic_activated_ability(player, card, event, 0, MANACOST_XW(1, 1), 0, NULL, NULL);
		}
	}

	return disabling_aura(player, card, event);
}

int card_call_to_glory(int player, int card, event_t event){
/* CARD_ID_CALL_TO_GLORY	7178
Call to Glory	|1|W
Instant
Untap all creatures you control. Samurai creatures you control get +1/+1 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_SAMURAI;
		new_manipulate_all(player, card, player, &test, ACT_UNTAP);
		pump_subtype_until_eot(player, card, player, SUBTYPE_SAMURAI, 1, 1, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

ARCANE_WITH_SPLICE(card_candles_glow, MANACOST_XW(1,1)){
	/* Candles' Glow	|1|W	0x2005f34
	 * Instant - Arcane
	 * Prevent the next 3 damage that would be dealt to any target this turn. You gain life equal to the damage prevented this way.
	 * Splice onto Arcane |1|W */
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.preferred_controller = player;
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			prevent_the_next_n_damage(player, card, instance->targets[0].player, instance->targets[0].card, 3, PREVENT_GAIN_LIFE, 0, 0);
		}
	}
	return 0;
}

// Cleanfall --> Tranquillity
ARCANE(card_cleanfall){

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ENCHANTMENT);
		new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_commune_with_nature(int player, int card, event_t event){
/* CARD_ID_COMMUNE_WITH_NATURE	7183
Commune with Nature	|G
Sorcery
Look at the top five cards of your library. You may reveal a creature card from among them and put it into your hand.
Put the rest on the bottom of your library in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		reveal_x_and_choose_a_card_type(player, card, 5, TYPE_CREATURE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

ARCANE_WITH_SPLICE(card_consuming_vortex, MANACOST_XU(3,1)){
/* CARD_ID_CONSUMING_VORTEX	7184
Consuming Vortex	|1|U
Instant - Arcane
Return target creature to its owner's hand.
Splice onto Arcane |3|U (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

// Cranial Extraction --> Memoricide

int card_cursed_ronin(int player, int card, event_t event){
/* CARD_ID_CURSED_RONIN	7188
Cursed Ronin	|3|B
Creature - Human Samurai 1/1
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)
|B: ~ gets +1/+1 until end of turn. */

	bushido(player, card, event, 1);

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_B(1), 1,1);
}

ARCANE_WITH_SPLICE(card_dampen_thought, MANACOST_XU(1,1)){
/* CARD_ID_DAMPEN_THOUGHT	7190
Dampen Thought	|1|U
Instant - Arcane
Target player puts the top four cards of his or her library into his or her graveyard.
Splice onto Arcane |1|U (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		return can_target(&td);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			mill(instance->targets[0].player, 4);
		}
	}
	return 0;
}

ARCANE_WITH_SPLICE(card_desperate_ritual, MANACOST_XR(1,1)){
/* CARD_ID_DESPERATE_RITUAL	7193
Desperate Ritual	|1|R
Instant - Arcane
Add |R|R|R to your mana pool.
Splice onto Arcane |1|R (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}
	if( event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_RED, 3);
	}

	return 0;
}

int card_devoted_retainer(int player, int card, event_t event){
/* CARD_ID_DEVOTED_RETAINER	7194
Devoted Retainer	|W
Creature - Human Samurai 1/1
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.) */

	return bushido(player, card, event, 1);
}

ARCANE(card_devouring_greed){
/* CARD_ID_DEVOURING_GREED	7195
Devouring Greed	|2|B|B
Sorcery - Arcane
As an additional cost to cast ~, you may sacrifice any number of Spirits.
Target player loses 2 life plus 2 life for each Spirit sacrificed this way. You gain that much life. */

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_PLAYER") ){
			if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
				instance->info_slot = 0;

				int sacrifices[100];
				int sc = 0;

				test_definition_t test;
				new_default_test_definition(&test, TYPE_PERMANENT, "Select a Spirit to sacrifice.");
				test.subtype = SUBTYPE_SPIRIT;

				while( new_can_sacrifice_as_cost(player, card, &test) ){
						int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
						if( sac ){
							sacrifices[sc] = BYTE3(sac);
							sc++;
						}
						else{
							break;
						}
				}

				int i;
				for(i=0; i<sc; i++){
					kill_card(player, sacrifices[i], KILL_SACRIFICE);
				}

				instance->info_slot = sc * 2;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, instance->info_slot);
			gain_life(player, instance->info_slot);
		}
	}

	return 0;
}

int card_dosan_the_falling_leaf(int player, int card, event_t event){
	/* Dosan the Falling Leaf	|1|G|G
	 * Legendary Creature - Human Monk 2/2
	 * Players can cast spells only during their own turns. */

	check_legend_rule(player, card, event);

	if( event == EVENT_MODIFY_COST_GLOBAL && in_play(player, card) && ! is_humiliated(player, card) ){
		if( affected_card_controller != current_turn ){
			infinite_casting_cost();
		}
	}
	return 0;
}

int card_dripping_tongue_zubera(int player, int card, event_t event){
	/* Dripping-Tongue Zubera	|1|G
	 * Creature - Zubera Spirit 1/2
	 * When ~ dies, put a 1/1 colorless Spirit creature token onto the battlefield for each Zubera that died this turn. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_tokens_by_id(player, card, CARD_ID_SPIRIT, get_zubera_count());
	}

	return 0;
}

int card_earthshaker(int player, int card, event_t event){
/* CARD_ID_EARTHSHAKER	7200
Earthshaker	|4|R|R
Creature - Spirit 4/5
Whenever you cast a Spirit or Arcane spell, ~ deals 2 damage to each creature without flying. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.keyword = KEYWORD_FLYING;
		test.keyword_flag = DOESNT_MATCH;
		new_damage_all(player, card, ANYBODY, 2, 0, &test);
	}

	return 0;
}

ARCANE(card_eerie_procession){
/* CARD_ID_EERIE_PROCESSION	7201
Eerie Procession	|2|U
Sorcery - Arcane
Search your library for an Arcane card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, 1, TYPE_ANY, 0, SUBTYPE_SPIRIT, 0, 0, 0, 0, 0, -1, 0);
	}
	return 0;
}

static const char* damage_to_legendary_creature(int who_chooses, int player, int card)
{
	card_instance_t *instance = get_card_instance(player, card);
	if(	instance->internal_card_id == damage_card ){
		if( instance->damage_target_card != -1 &&
			is_what(instance->damage_target_player, instance->damage_target_player, TYPE_CREATURE) &&
			is_legendary(instance->damage_target_player, instance->damage_target_player) )
		{
			return NULL;
		}
	}
	return "must be damage to a Legendary creature";
}

int card_eiganjo_castle(int player, int card, event_t event){
	/* Eiganjo Castle	""
	 * Legendary Land
	 * |T: Add |W to your mana pool.
	 * |W, |T: Prevent the next 2 damage that would be dealt to target legendary creature this turn. */

	 if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.extra = (int32_t)damage_to_legendary_creature;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance( player, card );

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION,
												MANACOST_W(2), 0, &td1, NULL);
		if( result ){
			return result;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION,
												MANACOST_W(2), 0, &td1, NULL);
		int ai_choice = 0;
		if( result ){
			ai_choice = 1;
		}
		int choice = 0;
		if( ai_choice == 1 ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Protect a Legend\n Cancel", ai_choice);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				charge_mana_for_activated_ability(player, card, MANACOST_W(1));
				if( spell_fizzled != 1 && new_pick_target(&td1, "Select target damage dealt to a legendary creature.", 0, GS_LITERAL_PROMPT) ){
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

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td1) ){
				prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
			}
		}
	}

	return 0;
}

int card_eight_and_a_half_tails(int player, int card, event_t event){
/*
Eight-and-a-Half-Tails |W|W
Legendary Creature — Fox Cleric 2/2
{1}{W}: Target permanent you control gains protection from white until end of turn.
{1}: Target spell or permanent becomes white until end of turn.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = player;
	td.preferred_controller = player;

	target_definition_t td2;
	counterspell_target_definition(player, card, &td2, TYPE_ANY);

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_GIVE_PROT = 1,
		CHOICE_CHANGE_COLOR_OF_PERMANENT,
		CHOICE_CHANGE_COLOR_OF_SPELL
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_X(1), 0, &td2, NULL);
		if( result ){
			return result;
		}
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td3, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE){
		instance->number_of_targets = instance->info_slot = 0;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SPELL_ON_STACK, MANACOST_X(1), 0, &td2, NULL) ){
			generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_X(1), 0, &td2, NULL);
			if( spell_fizzled != 1 ){
				instance->info_slot = 3;
			}
			return 0;
		}
		else{
		int abilities[3] =
		{
			0,
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, NULL),
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_X(1), 0, &td3, NULL)
		};

		int choice = abilities[2] ? CHOICE_CHANGE_COLOR_OF_SPELL : 0;
		if( choice ){
			int clr = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			const char* color = (clr == COLOR_BLACK ? "BLACK"
								 : clr == COLOR_BLUE ? "BLUE"
								 : clr == COLOR_GREEN ? "GREEN"
								 : clr == COLOR_RED ? "RED"
								 : "white");
			char buf[128];
			sprintf(buf, "Give Prot. from %s", color);
			char buf3[128];
			sprintf(buf3, "Turn permanent %s", color);
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_OMIT_ILLEGAL,
							buf, abilities[1], 1,
							buf3, abilities[2], 1);
		}

		if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}

		if( choice == CHOICE_GIVE_PROT ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, "TARGET_PERMANENT_YOU_CONTROL");
		}
		if( choice == CHOICE_CHANGE_COLOR_OF_PERMANENT ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td3, "TARGET_PERMANENT");
		}
		if( choice == CHOICE_CHANGE_COLOR_OF_SPELL ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td2, "TARGET_ANY");
		}
	}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_GIVE_PROT && valid_target(&td) ){
			pump_ability_until_eot( instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card,
									0, 0, get_sleighted_protection(player, card, KEYWORD_PROT_WHITE), 0 );
		}
		if( instance->info_slot == CHOICE_CHANGE_COLOR_OF_SPELL || instance->info_slot == 3){
			if( counterspell_validate(player, card, &td2, 0) ){
			change_color(player, card,
						 instance->targets[0].player, instance->targets[0].card,
						 get_sleighted_color_test(player, card, COLOR_TEST_WHITE), CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT);
			}
		}
		if( instance->info_slot == CHOICE_CHANGE_COLOR_OF_PERMANENT && valid_target(&td3) ){
			change_color(instance->parent_controller, instance->parent_card,
						 instance->targets[0].player, instance->targets[0].card,
						 get_sleighted_color_test(player, card, COLOR_TEST_WHITE), CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT);
		}
	}

	return 0;
}

int card_ember_fist_zubera(int player, int card, event_t event){
	/* Ember-Fist Zubera	|1|R	0x2005f70
	 * Creature - Zubera Spirit 1/2
	 * When ~ dies, it deals damage to any target equal to the number of Zubera that died this turn. */
	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_creature_or_player(player, card, event, get_zubera_count());
		}
	}

	return 0;
}

ARCANE(card_ethereal_haze){//UNUSEDCARD
/* CARD_ID_ETHEREAL_HAZE	7205
Ethereal Haze	|W
Instant - Arcane
Prevent all damage that would be dealt by creatures this turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		fog_special(player, card, ANYBODY, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

ARCANE(card_eye_of_nowhere){
/* CARD_ID_EYE_OF_NOWHERE	1295
Eye of Nowhere	|U|U
Sorcery - Arcane
Return target permanent to its owner's hand. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

ARCANE(card_feast_of_worms){
/* CARD_ID_FEAST_OF_WORMS	7206
Feast of Worms	|3|G|G
Sorcery - Arcane
Destroy target land. If that land was legendary, its controller sacrifices another land. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			int sac = is_legendary(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( sac == 1 ){
				player_sacrifices_a_permanent(player, card, instance->targets[0].player, TYPE_LAND, SAC_CAUSED | SAC_NO_CANCEL);
			}
		}
	}
	return 0;
}

int card_floating_dream_zubera(int player, int card, event_t event){
/* CARD_ID_FLOATING_DREAM_ZUBERA	7209
Floating-Dream Zubera	|1|U
Creature - Zubera Spirit 1/2
When ~ dies, draw a card for each Zubera that died this turn. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		draw_cards(player, get_zubera_count());
	}

	return 0;
}

int card_forbidden_orchard(int player, int card, event_t event){
	/* Forbidden Orchard	""
	 * Land
	 * |T: Add one mana of any color to your mana pool.
	 * Whenever you tap ~ for mana, target opponent puts a 1/1 colorless Spirit creature token onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIRIT, &token);
		token.t_player = 1-player;
		generate_token(&token);
	}

	return mana_producer(player, card, event);
}

int card_generals_kabuto(int player, int card, event_t event){
	/* General's Kabuto	|4
	 * Artifact - Equipment
	 * Equipped creature has shroud.
	 * Prevent all combat damage that would be dealt to equipped creature.
	 * Equip |2 */

	if( is_equipping(player, card) ){
		card_instance_t* damage = combat_damage_being_prevented(event);
		if (damage ){
			card_instance_t* instance = get_card_instance(player, card);
			if (instance->damage_target_card == damage->damage_target_card &&
				instance->damage_target_card == damage->damage_target_player)
			{
				int amount = damage->info_slot;
				prevent_damage(affected_card_controller, affected_card, amount);
			}
		}
	}

	return vanilla_equipment(player, card, event, 2, 0, 0, KEYWORD_SHROUD, 0);
}

// Ghostly Prison --> Propaganda

static int test_not_in_array4(int iid, int arrptr, int player, int card)
{
  int* arr = (int*)arrptr;
  return iid != arr[0] && iid != arr[1] && iid != arr[2] && iid != arr[3];
}

int gift_ungiven_impl(int player, int card, event_t event, test_definition_t *test, int must_target_opponent){

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL )
	{
		if( (! must_target_opponent || (must_target_opponent && valid_target(&td))) && deck_ptr[player][0] != -1 ){
			int chosen[4] = {-1, -1, -1, -1};

			test->no_shuffle = 1;
			test->value_for_special_selection_function = (int)chosen;
			test->special_selection_function = test_not_in_array4;

			int num_chosen;
			for (num_chosen = 0; num_chosen < 4; ++num_chosen)
			{
				if (ai_is_speculating != 1){
					char msg[100];
					sprintf(test->message, "Select four %s with different names: #%d", type_text(msg, 100, test->type, 0), num_chosen + 1);
				}

				int pos = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, test);
				if(pos == -1)
					break;
				chosen[num_chosen] = deck_ptr[player][pos];
				remove_card_from_deck(player, pos);
			}

			/* Shuffle library first, since Manalink triggers won't wait until this is done resolving; we don't want any weird sequence
			* of triggers caused by putting cards into the graveyard to have any chance of doing anything with the unshuffled top of
			* the library. */
			shuffle(player);

			if (num_chosen <= 2)	// All go to graveyard from library.
			{
				int to_mill = 0;
				if (chosen[1] != -1)
				{
					put_on_top_of_deck(player, add_card_to_hand(player, chosen[1]));
					++to_mill;
				}
				if (chosen[0] != -1)
				{
					put_on_top_of_deck(player, add_card_to_hand(player, chosen[0]));
					++to_mill;
				}
				mill(player, to_mill);
			}
			else
			{
				new_default_test_definition(test, 0, "Select a card to put into opponent's graveyard.");
				int i;
				for (i = 0; i < 2; ++i)
				{
					int selected = select_card_from_zone(1-player, player, chosen, num_chosen, 1, AI_MAX_VALUE, -1, test);
					put_on_top_of_deck(player, add_card_to_hand(player, chosen[selected]));
					// Move later choices down
					for (; selected < 3; ++selected)
						chosen[selected] = chosen[selected + 1];
					chosen[3] = -1;
					--num_chosen;
				}

				mill(player, 2);

				for (i = 0; i < 4; ++i)
					if (chosen[i] != -1)
						add_card_to_hand(player, chosen[i]);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if( ! must_target_opponent ){
		return basic_spell(player, card, event);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_gifts_ungiven(int player, int card, event_t event){return 0;}
int card_gifts_ungiven2(int player, int card, event_t event)
{
/*
Gifts Ungiven |3|U
Instant
Search your library for up to four cards with different names and reveal them. Target opponent chooses two of those cards.
 Put the chosen cards into your graveyard and the rest into your hand. Then shuffle your library.
 */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_ANY);

	return gift_ungiven_impl(player, card, event, &test, 1);
}

ARCANE_WITH_SPLICE(card_glacial_ray, MANACOST_XR(1,1)){
	/* Glacial Ray	|1|R	0x2005f84
	 * Instant - Arcane
	 * ~ deals 2 damage to any target.
	 * Splice onto Arcane |1|R */
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}
	return 0;
}

static int effect_glimpse_of_nature(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			draw_cards(player, 1);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}


int card_glimpse_of_nature(int player, int card, event_t event){
/* CARD_ID_GLIMPSE_OF_NATURE	7218
Glimpse of Nature	|G
Sorcery
Whenever you cast a creature spell this turn, draw a card. */

	if(event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI){
		ai_modifier += (has_mana(player, COLOR_ANY, -1)*5);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &effect_glimpse_of_nature );
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_godo_bandit_warlord(int player, int card, event_t event){
/* CARD_ID_GODO_BANDIT_WARLORD	7219
Godo, Bandit Warlord	|5|R
Legendary Creature - Human Barbarian 3/3
When ~ enters the battlefield, you may search your library for an Equipment card and put it onto the battlefield. If you do, shuffle your library.
Whenever ~ attacks for the first time each turn, untap it and all Samurai you control. After this phase, there is an additional combat phase.*/

	check_legend_rule(player, card, event);

	// When ~ enters the battlefield, you may search your library for an Equipment card and put it onto the battlefield.
	// If you do, shuffle your library.
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( do_dialog(player, player, card, -1, -1, " Search for an Equipment\n Pass", 0) == 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an Equipment card.");
			this_test.subtype = SUBTYPE_EQUIPMENT;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	// Whenever ~ attacks for the first time each turn, untap it and all Samurai you control. After this phase,
	// there is an additional combat phase.
	if (get_card_instance(player, card)->info_slot == 0 && declare_attackers_trigger(player, card, event, 0, player, card)){

		card_instance_t* instance = get_card_instance(player, card);
		instance->info_slot = 1;

		untap_card(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_SAMURAI;
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);

		create_legacy_effect(player, card, &finest_hour_legacy);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}


	return 0;
}

int card_gutwrencher_oni(int player, int card, event_t event){
/* CARD_ID_GUTWRENCHER_ONI	7222
Gutwrencher Oni	|3|B|B
Creature - Demon Spirit 5/4
Trample
At the beginning of your upkeep, discard a card if you don't control an Ogre. */

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		upkeep_trigger_ability_mode(player, card, event, player, ! check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_OGRE) ?
							RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		discard(player, 0, player);
	}

	return 0;
}

int card_hana_kami(int player, int card, event_t event){
	/* Hana Kami	|G
	 * Creature - Spirit 1/1
	 * |1|G, Sacrifice ~: Return target Arcane card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target Arcane card.");
	test.subtype = SUBTYPE_ARCANE;

	if( event == EVENT_CAN_ACTIVATE ){
		if( new_special_count_grave(player, &test) && ! graveyard_has_shroud(player) ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(1, 1), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XG(1, 1)) ){
			if( select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &test, 0) != -1 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}
	if(event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return 0;
}

int card_he_who_hungers(int player, int card, event_t event){
/* CARD_ID_HE_WHO_HUNGERS	7229
He Who Hungers	|4|B
Legendary Creature - Spirit 3/2
Flying
|1, Sacrifice a Spirit: Target opponent reveals his or her hand. You choose a card from it. That player discards that card.
Activate this ability only any time you could cast a sorcery.
Soulshift 4 (When this creature dies, you may return target Spirit card with converted mana cost 4 or less from your graveyard to your hand.) */

	check_legend_rule(player, card, event);

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		if(event == EVENT_RESOLVE_ACTIVATION){
			if( valid_target(&td) ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ANY);

				ec_definition_t this_definition;
				default_ec_definition(instance->targets[0].player, player, &this_definition);
				new_effect_coercion(&this_definition, &this_test);
			}
		}
		return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT | GAA_SACRIFICE_CREATURE, MANACOST_X(1), 0,
										&td, NULL);
	}

	return soulshift(player, card, event, 4, 1);
}

static int target_for_heart_kami(int player, int card, int mode){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	int i;
	int trg = -1;
	int par = -1;
	for(i=0; i<2; i++){
		if( i == 1-player || player == HUMAN ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_ARTIFACT) ){
						if( has_mana_for_activated_ability(player, card, MANACOST_X(get_cmc(i, count))) ){
							if( would_validate_arbitrary_target(&td, i, count) ){
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

int card_heart_kami(int player, int card, event_t event){
/* CARD_ID_HEARTH_KAMI	7230
Hearth Kami	|1|R
Creature - Spirit 2/1
|X, Sacrifice ~: Destroy target artifact with converted mana cost X. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL) ){
		return target_for_heart_kami(player, card, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( player == HUMAN ){
			if( pick_target(&td, "TARGET_ARTIFACT") ){
				int cost = get_cmc( instance->targets[0].player, instance->targets[0].card );
				if( ! has_mana_for_activated_ability(player, card, MANACOST_X(cost)) ){
					spell_fizzled = 1;
				}
			}
		}
		else if( player == AI ){
				int result = target_for_heart_kami(player, card, 1);
				if( result != -1 ){
					instance->targets[0].player = 1-player;
					instance->targets[0].card = result;
					instance->number_of_targets = 1;
				}
				else{
					spell_fizzled = 1;
				}
		}
		if( spell_fizzled != 1 ){
			int cost = get_cmc( instance->targets[0].player, instance->targets[0].card );
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(cost)) ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card( instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
		}
	}

	return 0;
}

ARCANE_WITH_SPLICE(card_hideous_laughter, MANACOST_XB(3,2)){
/* CARD_ID_HIDEOUS_LAUGHTER	7231
Hideous Laughter	|2|B|B
Instant - Arcane
All creatures get -2/-2 until end of turn.
Splice onto Arcane |3|B|B (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_creatures_until_eot(player, card, ANYBODY, 0, -2,-2, 0,0, NULL);
	}
	return 0;
}

int card_hikari_twilight_guardian(int player, int card, event_t event){
/* CARD_ID_HIKARI_TWILIGHT_GUARDIAN	7232
Hikari, Twilight Guardian	|3|W|W
Legendary Creature - Spirit 4/4
Flying
Whenever you cast a Spirit or Arcane spell, you may exile ~.
If you do, return it to the battlefield under its owner's control at the beginning of the next end step. */

	check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_AI(player)) ){
		remove_until_eot(player, card, player, card);
	}

	return 0;
}

int card_hinder(int player, int card, event_t event)
{
  /* Hinder	|1|U|U
   * Instant
   * Counter target spell. If that spell is countered this way, put that card on the top or bottom of its owner's library instead of into that player's
   * graveyard. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (counterspell_validate(player, card, NULL, 0))
		{
		  set_flags_when_spell_is_countered(player, card, instance->targets[0].player, instance->targets[0].card);
		  if (do_dialog(player, player, card, instance->targets[0].player, instance->targets[0].card, " Put on top of library\n Put on bottom of library", 1) == 0)
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		  else
			put_on_bottom_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
	  kill_card(player, card, KILL_DESTROY);
	  return 0;
	}
  else
	return counterspell(player, card, event, NULL, 0);
}

int card_hisoka_minamo_sensei(int player, int card, event_t event){
/*
Hisoka, Minamo Sensei |2|U|U
Legendary Creature — Human Wizard 1/3
{2}{U}, Discard a card: Counter target spell if it has the same converted mana cost as the discarded card.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_DISCARD, MANACOST_XU(2, 1), 0, &td, NULL);
		if( result ){
			test.cmc = get_cmc(card_on_stack_controller, card_on_stack);
			if( player == AI && ! check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
				result = 0;
			}
		}
		return result;
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XU(2, 1)) ){
			if( player == AI ){
				test.cmc = get_cmc(card_on_stack_controller, card_on_stack);
			}
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
			if( selected != -1 ){
				instance->info_slot = get_cmc(player, selected);
				new_discard_card(player, selected, 0, player);
				return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) &&
			get_cmc(instance->targets[0].player, instance->targets[0].card) == instance->info_slot )
		{
			real_counter_a_spell(instance->parent_controller, instance->parent_card,
								instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_honden_of_nights_reach (int player, int card, event_t event){
/* CARD_ID_HONDEN_OF_NIGHTS_REACH	7241
Honden of Night's Reach	|3|B
Legendary Enchantment - Shrine
At the beginning of your upkeep, target opponent discards a card for each Shrine you control. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance(player, card);

		instance->targets[0].player = 1-player;
		instance->targets[0].card = -1;
		instance->number_of_targets = 1;

		if( valid_target(&td) ){
			new_multidiscard(1-player, count_subtype(player, TYPE_ENCHANTMENT, SUBTYPE_SHRINE), 0, player);
		}
	}

	return global_enchantment(player, card, event);
}

int card_honden_of_seeing_winds (int player, int card, event_t event){
/* CARD_ID_HONDEN_OF_SEEING_WINDS	7242
Honden of Seeing Winds	|4|U
Legendary Enchantment - Shrine
At the beginning of your upkeep, draw a card for each Shrine you control. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		draw_cards(player, count_subtype(player, TYPE_ENCHANTMENT, SUBTYPE_SHRINE));
	}

	return global_enchantment(player, card, event);
}

int card_honden_of_lifes_web (int player, int card, event_t event){
	/* Honden of Life's Web	|4|G
	 * Legendary Enchantment - Shrine
	 * At the beginning of your upkeep, put a 1/1 colorless Spirit creature token onto the battlefield for each Shrine you control. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_SPIRIT, count_subtype(player, TYPE_ENCHANTMENT, SUBTYPE_SHRINE));
	}

	return global_enchantment(player, card, event);
}

int card_honden_of_infinite_rage(int player, int card, event_t event){
	/* Honden of Infinite Rage	|2|R	0x20012cc
	 * Legendary Enchantment - Shrine
	 * At the beginning of your upkeep, ~ deals damage to any target equal to the number of Shrines you control. */
	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, count_subtype(player, TYPE_ENCHANTMENT, SUBTYPE_SHRINE));
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_honden_of_cleansing_fire (int player, int card, event_t event){
/* CARD_ID_HONDEN_OF_CLEANSING_FIRE	7238
Honden of Cleansing Fire	|3|W
Legendary Enchantment - Shrine
At the beginning of your upkeep, you gain 2 life for each Shrine you control. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		gain_life(player, 2*count_subtype(player, TYPE_ENCHANTMENT, SUBTYPE_SHRINE));
	}

	return global_enchantment(player, card, event);
}

int card_honor_worn_shaku(int player, int card, event_t event){
/* CARD_ID_HONOR_WORN_SHAKU	7243
Honor-Worn Shaku	|3
Artifact
|T: Add |C to your mana pool.
Tap an untapped legendary permanent you control: Untap ~. */

	if( ! IS_GAA_EVENT(event) ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance= get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_UNTAP
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return player == AI ? (is_tapped(player, card) ? 1 : 0) : 1;
		}
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int abilities[3] = {
							0,
							mana_producer(player, card, EVENT_CAN_ACTIVATE),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL)
		};

		int choice = CHOICE_MANA;
		if( ! paying_mana() ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 					abilities[1], 5,
							"Tap Legend and untap this", 	abilities[2], is_tapped(player, card) ? 10 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_UNTAP ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
										&td, "Select an untapped Legendary permanent you control.");
			instance->number_of_targets = 0;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_UNTAP ){
			untap_card(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}


int card_horobi_deaths_wail(int player, int card, event_t event){
/* CARD_ID_HOROBI_DEATHS_WAIL	7245
Horobi, Death's Wail	|2|B|B
Legendary Creature - Spirit 4/4
Flying
Whenever a creature becomes the target of a spell or ability, destroy that creature. */

  check_legend_rule(player, card, event);

  const target_t* targets = any_creature_becomes_target_of_spell_or_effect(player, card, event, ANYBODY);
  if (targets)
	for (; targets->player != -1; ++targets)
	  kill_card(targets->player, targets->card, KILL_DESTROY);

  return 0;
}

// Humble Budoka --> vanilla

int card_iname_death_aspect(int player, int card, event_t event){
/* CARD_ID_INAME_DEATH_ASPECT	7249
Iname, Death Aspect	|4|B|B
Legendary Creature - Spirit 4/4
When ~ enters the battlefield, you may search your library for any number of Spirit cards and put them into your graveyard. If you do, shuffle your library.*/

	check_legend_rule(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a Spirit card.");
		test.qty = count_deck(player);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_GRAVE, 0, AI_MIN_VALUE, &test);
	}

	return 0;
}

int card_iname_life_aspect(int player, int card, event_t event){
/* CARD_ID_INAME_LIFE_ASPECT	7250
Iname, Life Aspect	|4|G|G
Legendary Creature - Spirit 4/4
When ~ dies, you may exile it. If you do, return any number of target Spirit cards from your graveyard to your hand. */

	check_legend_rule(player, card, event);

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player))
	  && exile_from_owners_graveyard(player, card))
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a Spirit card.");
		test.subtype = SUBTYPE_SPIRIT;
		int max = new_special_count_grave(player, &test);
		test.qty = max;

		if( ! graveyard_has_shroud(player) && max ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		}
	}

	return 0;
}

int card_indomitable_will(int player, int card, event_t event){
/* CARD_ID_INDOMITABLE_WILL	7251
Indomitable Will	|1|W
Enchantment - Aura
Flash
Enchant creature
Enchanted creature gets +1/+2. */

	return generic_aura(player, card, event, player, 1, 2, 0, 0, 0, 0, 0);
}

int card_jade_idol(int player, int card, event_t event){
/* CARD_ID_JADE_IDOL	7255
Jade Idol	|4
Artifact
Whenever you cast a Spirit or Arcane spell, ~ becomes a 4/4 Spirit artifact creature until end of turn. */

	if( arcane_spirit_spell_trigger(player, card, event, 2) ){
		add_a_subtype(player, card, SUBTYPE_SPIRIT);
		artifact_animation(player, card, player, card, 1, 4, 4, 0, 0);
	}

	return 0;
}

int card_journeyers_kite(int player, int card, event_t event){
/* CARD_ID_JOURNEYERS_KITE	7256
Journeyer's Kite	|2
Artifact
|3, |T: Search your library for a basic land card, reveal it, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		char msg[100] = "Select a basic land card.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, msg);
		this_test.subtype = SUBTYPE_BASIC;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
}

int card_jugan_rising_star(int player, int card, event_t event){
/* CARD_ID_JUGAN_THE_RISING_STAR	7258
Jugan, the Rising Star	|3|G|G|G
Legendary Creature - Dragon Spirit 5/5
Flying
When ~ dies, you may distribute five +1/+1 counters among any number of target creatures. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance= get_card_instance(player, card);

		int count = 0;
		while( can_target(&td) && count < 5 ){
				new_pick_target(&td, "TARGET_CREATURE", count, 0);
				count++;
		}

		instance->number_of_targets = 0;

		int i;
		for(i=0; i<5; i++){
			add_1_1_counter(instance->targets[i].player, instance->targets[i].card);
		}
	}

	return 0;
}

// Jukai Messenger -> vanilla

int card_junkyo_bell(int player, int card, event_t event){
/* CARD_ID_JUNKYO_BELL	7260
Junkyo Bell	|4
Artifact
At the beginning of your upkeep, you may have target creature you control get +X/+X until end of turn, where X is the number of creatures you control.
If you do, sacrifice that creature at the beginning of the next end step. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;

		card_instance_t *instance= get_card_instance(player, card);

		if( pick_target(&td, "TARGET_CREATURE") ){
			int pow = count_subtype(player, TYPE_CREATURE, -1);
			pump_ability_t pump;
			default_pump_ability_definition(player, card, &pump, pow, pow, 0, 0);
			pump.paue_flags = PAUE_END_AT_EOT | PAUE_REMOVE_TARGET_AT_EOT;
			pump.eot_removal_method = KILL_SACRIFICE;
			pump_ability(player, card, instance->targets[0].player, instance->targets[0].card, &pump);
		}
	}

	return 0;
}

int card_jushi_apprentice(int player, int card, event_t event){
/* CARD_ID_JUSHI_APPRENTICE	7261
Jushi Apprentice	|1|U
Creature - Human Wizard 1/2
|2|U, |T: Draw a card. If you have nine or more cards in hand, flip ~.
--FLIP--
Tomoya the Revealer
Legendary Creature - Human Wizard
2/3
|3|U|U, |T: Target player draws X cards, where X is the number of cards in your hand. */

	double_faced_card(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance= get_card_instance(player, card);
		draw_cards(player, 1);
		if( hand_count[player] > 8 ){
			true_transform(instance->parent_controller, instance->parent_card);
			verify_legend_rule(instance->parent_controller, instance->parent_card, get_id(instance->parent_controller, instance->parent_card));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(2, 1), 0, NULL, NULL);
}

int card_tomoya_the_revealer(int player, int card, event_t event){
/* CARD_ID_TOMOYA_THE_REVEALER	7262
Tomoya the Revealer	|1|U
Legendary Creature - Human Wizard 2/3
|3|U|U, |T: Target player draws X cards, where X is the number of cards in your hand. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, hand_count[player]);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XU(3, 2), 0, &td, "TARGET_PLAYER");
}

int card_kami_of_ancient_law(int player, int card, event_t event){
/* CARD_ID_KAMI_OF_ANCIENT_LAW	7264
Kami of Ancient Law	|1|W
Creature - Spirit 2/2
Sacrifice ~: Destroy target enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ENCHANTMENT");
}

int card_kami_of_fires_roar(int player, int card, event_t event){
/* CARD_ID_KAMI_OF_FIRES_ROAR	7265
Kami of Fire's Roar	|3|R
Creature - Spirit 2/3
Whenever you cast a Spirit or Arcane spell, target creature can't block this turn. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );

		card_instance_t *instance= get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_kami_of_the_hunt(int player, int card, event_t event){
/* CARD_ID_KAMI_OF_THE_HUNT	7268
Kami of the Hunt	|2|G
Creature - Spirit 2/2
Whenever you cast a Spirit or Arcane spell, ~ gets +1/+1 until end of turn. */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		pump_until_eot_merge_previous(player, card, player, card, 1, 1);
	}

	return 0;
}

int card_kami_of_waning_moon(int player, int card, event_t event){
/* CARD_ID_KAMI_OF_THE_WANING_MOON	7271
Kami of the Waning Moon	|2|B
Creature - Spirit 1/1
Flying
Whenever you cast a Spirit or Arcane spell, target creature gains fear until end of turn. (It can't be blocked except by artifact creatures and/or black creatures.) */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;
        td.allow_cancel = 0;

		card_instance_t *instance= get_card_instance(player, card);

		if( can_target(&td) &&  pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_FEAR);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_kashi_tribe_reaver(int player, int card, event_t event){
/* CARD_ID_KASHI_TRIBE_REAVER	7273
Kashi-Tribe Reaver	|3|G
Creature - Snake Warrior 3/2
Whenever ~ deals combat damage to a creature, tap that creature and it doesn't untap during its controller's next untap step.
|1|G: Regenerate ~. */

	freeze_when_damage(player, card, event);

	return regeneration(player, card, event, MANACOST_XG(1, 1));
}

int card_keiga(int player, int card, event_t event){
/* CARD_ID_KEIGA_THE_TIDE_STAR	7275
Keiga, the Tide Star	|5|U
Legendary Creature - Dragon Spirit 5/5
Flying
When ~ dies, gain control of target creature. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);
		instance->state |= STATE_CANNOT_TARGET;
		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			gain_control_permanently(player, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_kiki_jiki_mirror_breaker(int player, int card, event_t event){
/* CARD_ID_KIKI_JIKI_MIRROR_BREAKER	7276
Kiki-Jiki, Mirror Breaker	|2|R|R|R
Legendary Creature - Goblin Shaman 2/2
Haste
|T: Create a token that's a copy of target nonlegendary creature you control. That token has haste. Sacrifice it at the beginning of the next end step.*/

	if( ! is_unlocked(player, card, event, 17) ){ return 0; }

	haste(player, card, event);

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.required_subtype = SUBTYPE_LEGEND;
	td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			token_generation_t token;
			copy_token_definition(instance->parent_controller, instance->parent_card, &token,
								  instance->targets[0].player, instance->targets[0].card);
			token.special_code_on_generation = &haste_and_sacrifice_token_at_end_of_next_turn;
			generate_token(&token);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_LITERAL_PROMPT, MANACOST0, 0,
									 &td, "Select target nonlegendary creature.");
}

int card_kiku_night_flower(int player, int card, event_t event){
/* CARD_ID_KIKU_NIGHTS_FLOWER	7277
Kiku, Night's Flower	|B|B
Legendary Creature - Human Assassin 1/1
|2|B|B, |T: Target creature deals damage to itself equal to its power. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int dmg = get_power(instance->targets[0].player, instance->targets[0].card);
			damage_creature(instance->targets[0].player, instance->targets[0].card, dmg,
							instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XB(2, 2), 0, &td, "TARGET_CREATURE");
}

// Kitsune Blademaster --> Devoted Retainer

int card_kitsune_healer(int player, int card, event_t event){
	/* Kitsune Healer	|3|W	0x2006001
	 * Creature - Fox Cleric 2/2
	 * |T: Prevent the next 1 damage that would be dealt to any target this turn.
	 * |T: Prevent all damage that would be dealt to target legendary creature this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( can_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( target->damage_target_card != -1 &&
				is_what(target->damage_target_player, target->damage_target_card, TYPE_CREATURE) &&
				is_legendary(target->damage_target_player, target->damage_target_card)
			  ){
				int dmg = target->info_slot;
				prevent_damage(instance->targets[0].player, instance->targets[0].card, dmg);
				prevent_all_damage_to_target(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 1);
			}
			else{
				prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0, &td, "TARGET_DAMAGE");
}

int card_kitsune_mystic(int player, int card, event_t event){
/* CARD_ID_KITSUNE_MYSTIC	7281
Kitsune Mystic	|3|W
Creature - Fox Wizard 2/3
At the beginning of the end step, if ~ is enchanted by two or more Auras, flip it.
--FLIP--
Autumn-Tail, Kitsune Sage
Legendary Creature - Fox Wizard
4/5
|1: Attach target Aura attached to a creature to another creature. */

	double_faced_card(player, card, event);
	if( eot_trigger(player, card, event) && count_auras_enchanting_me(player, card) > 1 ){
		true_transform(player, card);
	}
	return 0;
}

static int check_targettable_auras_for_autumn_tail(int player, int card, int mode){
	int i;
	int par = -1;
	int result = -1;
	for(i=0; i<2; i++){
		if( player == HUMAN || (player == AI && i != player) ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && has_subtype(i, count, SUBTYPE_AURA_CREATURE) ){
						card_instance_t *targ = get_card_instance( i, count );
						CardFunction ptFunction = cards_data[targ->internal_card_id].code_pointer;
						if( ptFunction(i, count, EVENT_CAN_MOVE_AURA) ){
							if( mode == 0 ){
								return 1;
							}
							if( mode == 1 && get_cmc(i, count) > par ){
								par = get_cmc(i, count);
								result = count;
							}
						}
					}
					count++;
			}
		}
	}
	return result;
}

int card_autumn_tail_kitsune_sage(int player, int card, event_t event){
/*
Autumn-Tail, Kitsune Sage |3|W
Legendary Creature — Fox Wizard 4/5
{1}: Attach target Aura attached to a creature to another creature.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ENCHANTMENT);
	td1.required_subtype = SUBTYPE_AURA_CREATURE;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td1, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			int good = 1;
			if( player == HUMAN ){
				if( new_pick_target(&td1, "Select target Aura attached to a creature.", 0, 1 | GS_LITERAL_PROMPT) ){
					if( call_card_function(instance->targets[0].player, instance->targets[0].card, EVENT_CAN_MOVE_AURA) ){
						good = 0;
					}
				}
			}
			else{
				int result = check_targettable_auras_for_autumn_tail(player, card, 1);
				if( result > -1 ){
					instance->targets[0].player = player;
					instance->targets[0].card = result;
					instance->number_of_targets = 1;
				}
				else{
					good = 0;
				}
			}
			if( good == 1 ){
				if( call_card_function(instance->targets[0].player, instance->targets[0].card, EVENT_MOVE_AURA) ){
					spell_fizzled = 1;
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			call_card_function(instance->targets[0].player, instance->targets[0].card, EVENT_RESOLVE_MOVING_AURA);
		}
	}

	return 0;
}

int card_kodama_of_the_south_tree(int player, int card, event_t event){
/* CARD_ID_KODAMA_OF_THE_SOUTH_TREE	7285
Kodama of the South Tree	|2|G|G
Legendary Creature - Spirit 4/4
Whenever you cast a Spirit or Arcane spell, each other creature you control gets +1/+1 and gains trample until end of turn. */

    check_legend_rule(player, card, event);

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;
		pump_creatures_until_eot(player, card, player, 0, 1,1, KEYWORD_TRAMPLE,0, &test);
	}

	return 0;
}

int card_kodama_of_the_north_tree(int player, int card, event_t event){
/* CARD_ID_KODAMA_OF_THE_NORTH_TREE	7284
Kodama of the North Tree	|2|G|G|G
Legendary Creature - Spirit 6/4
Trample
Shroud (This creature can't be the target of spells or abilities.) */

	check_legend_rule(player, card, event);
	return 0;
}

ARCANE_WITH_SPLICE(card_kodamas_might, MANACOST_G(1)){
/* CARD_ID_KODAMAS_MIGHT	7286
Kodama's Might	|G
Instant - Arcane
Target creature gets +2/+2 until end of turn.
Splice onto Arcane |G (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}
	return 0;
}

ARCANE(card_kodamas_reach){
/* CARD_ID_KODAMAS_REACH	1531
Kodama's Reach	|2|G
Sorcery - Arcane
Search your library for up to two basic land cards, reveal those cards, and put one onto the battlefield tapped and the other into your hand.
Then shuffle your library. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}
	if( event == EVENT_RESOLVE_SPELL ){
		cultivate(player);
	}
	return 0;
}

int card_kokusho_the_evening_star(int player, int card, event_t event){
/* CARD_ID_KOKUSHO_THE_EVENING_STAR	1562
Kokusho, the Evening Star	|4|B|B
Legendary Creature - Dragon Spirit 5/5
Flying
When ~ dies, each opponent loses 5 life. You gain life equal to the life lost this way. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int amount = lose_life(1-player, 5);
		gain_life(player, amount);
	}

	return 0;
}

int card_kondas_banner(int player, int card, event_t event){
/*
Konda's Banner |2
Legendary Artifact — Equipment
Konda's Banner can be attached only to a legendary creature.
Creatures that share a color with equipped creature get +1/+1.
Creatures that share a creature type with equipped creature get +1/+1.
Equip {2}
*/
	check_legend_rule(player, card, event);

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int eq_player = instance->damage_target_player;
		int eq_card = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! affect_me(eq_player, eq_card) ){
			if( shares_creature_subtype(eq_player, eq_card, affected_card_controller, affected_card) ){
				event_result++;
			}
			if( get_color(eq_player, eq_card) & get_color(affected_card_controller, affected_card) ){
				event_result++;
			}
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_LEGEND;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			equip_target_creature(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_EQUIPMENT|GAA_LITERAL_PROMPT, MANACOST_X(2), 0, &td, "Select target Legendary creature you control.");
}

int card_kondas_hatamoto(int player, int card, event_t event){
/* CARD_ID_KONDAS_HATAMOTO	7289
Konda's Hatamoto	|1|W
Creature - Human Samurai 1/2
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)
As long as you control a legendary Samurai, ~ gets +1/+2 and has vigilance. (Attacking doesn't cause this creature to tap.) */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_ABILITIES) && affect_me(player, card) ){
		if( in_play(player, card) && ! is_humiliated(player, card) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			test.subtype = SUBTYPE_SAMURAI;
			test.sub2 = SUBTYPE_LEGEND;
			test.subtype_flag = F2_MULTISUBTYPE_ALL;

			if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &test) ){
				switch( event ){
						case EVENT_POWER:
							event_result++;
							break;

						case EVENT_TOUGHNESS:
							event_result+=2;
							break;

							case EVENT_ABILITIES:
							vigilance(player, card, event);
							break;

						default:
							break;
				}
			}
		}
	}

	return bushido(player, card, event, 1);
}

int card_konda_lord_of_eiganjo(int player, int card, event_t event){
/* CARD_ID_KONDA_LORD_OF_EIGANJO	7287
Konda, Lord of Eiganjo	|5|W|W
Legendary Creature - Human Samurai 3/3
Vigilance, indestructible
Bushido 5 (Whenever this creature blocks or becomes blocked, it gets +5/+5 until end of turn.) */

	check_legend_rule(player, card, event);
	vigilance(player, card, event);
	indestructible(player, card, event);
	return bushido(player, card, event, 5);
}

static int rfg_when_die_legacy(int player, int card, event_t event){

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance= get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( affect_me(p, c) ){
			card_instance_t *affected = get_card_instance(p, c);
			if( affected->kill_code >= KILL_DESTROY && affected->kill_code <= KILL_SACRIFICE ){
				affected->kill_code = KILL_EXILE;
				set_special_flags2(p, c, SF2_WILL_BE_EXILED_IF_PUTTED_IN_GRAVE);
			}
		}
	}

	if( trigger_condition == TRIGGER_LEAVE_PLAY && affect_me(player, card) ){
		card_instance_t *instance= get_card_instance(player, card);
		int p = instance->targets[0].player;
		int c = instance->targets[0].card;

		if( other_leaves_play(player, card, event, p, c) ){
			kill_card(player, card, KILL_EXILE);
		}
	}


	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

static int rfg_when_die(int player, int card, int t_player, int t_card){
	int legacy = create_targetted_legacy_effect(player, card, &rfg_when_die_legacy, t_player, t_card);
	card_instance_t *instance = get_card_instance( player, legacy );
	instance->targets[0].player = player;
	instance->targets[0].card = card;
	instance->number_of_targets = 1;
	return legacy;
}

int card_kumano_master_yamabushi(int player, int card, event_t event){
	/* Kumano, Master Yamabushi	|3|R|R	0x2006024
	 * Legendary Creature - Human Shaman 4/4
	 * |1|R: ~ deals 1 damage to any target.
	 * If a creature dealt damage by ~ this turn would die, exile it instead. */
	check_legend_rule(player, card, event);

	damage_effects(player, card, event);
	check_damage_test(player, card, event, DDBM_TRACE_DAMAGING_SOURCES | DDBM_MUST_DAMAGE_CREATURE | DDBM_TRACE_DAMAGED_CREATURES, player, card, 0, NULL);

	if( resolve_damage_trigger(player, card, event, DDBM_TRACE_DAMAGING_SOURCES | DDBM_MUST_DAMAGE_CREATURE | DDBM_TRACE_DAMAGED_CREATURES, player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		int i;
		for(i = 0; i<instance->targets[1].player; i++){
			rfg_when_die(player, card, instance->targets[i].player, instance->targets[i].card);
		}
		instance->targets[1].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_ANY");
}

int card_kuro_pit_lord(int player, int card, event_t event){
/* CARD_ID_KURO_PITLORD	7292
Kuro, Pitlord	|6|B|B|B
Legendary Creature - Demon Spirit 9/9
At the beginning of your upkeep, sacrifice ~ unless you pay |B|B|B|B.
Pay 1 life: Target creature gets -1/-1 until end of turn. */

	check_legend_rule(player, card, event);

	basic_upkeep(player, card, event, MANACOST_B(4));

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			pump_until_eot_merge_previous(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -1, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 1, &td, "TARGET_CREATURE");
}

ARCANE(card_lava_spike){
	/* Lava Spike	|R	0x200190c
	 * Sorcery - Arcane
	 * ~ deals 3 damage to target player or planeswalker. */
	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);

	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);

	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
	}

	return 0;
}

// Lantern Kami --> vanilla

int card_long_forgotten_gohei(int player, int card, event_t event){
/* CARD_ID_LONG_FORGOTTEN_GOHEI	7297
Long-Forgotten Gohei	|3
Artifact
Arcane spells you cast cost |1 less to cast.
Spirit creatures you control get +1/+1. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player && ! is_humiliated(player, card) ){
		if( has_subtype(affected_card_controller, affected_card, SUBTYPE_ARCANE) ){
			COST_COLORLESS--;
		}
	}

	return boost_creature_type(player, card, event, SUBTYPE_SPIRIT, 1, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
}

int card_marrow_gnawer(int player, int card, event_t event){
	/* Marrow-Gnawer	|3|B|B
	 * Legendary Creature - Rat Rogue 2/3
	 * Rat creatures have fear.
	 * |T, Sacrifice a Rat: Put X 1/1 |Sblack Rat creature tokens onto the battlefield, where X is the number of Rats you control. */

	check_legend_rule(player, card, event);

	if( event == EVENT_ABILITIES && in_play(player, card) && ! is_humiliated(player, card) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			has_subtype(affected_card_controller, affected_card, SUBTYPE_RAT) )
		{
			fear(affected_card_controller, affected_card, event);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Rat to sacrifice.");
	test.subtype = SUBTYPE_RAT;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_RAT, &token);
		token.qty = count_subtype(player, TYPE_PERMANENT, SUBTYPE_RAT);
		token.pow = token.tou = 1;
		token.color_forced = get_sleighted_color_test(instance->parent_controller, instance->parent_card, COLOR_TEST_BLACK);
		generate_token(&token);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL, 0, &test, 0);
}

int card_matsu_tribe_decoy(int player, int card, event_t event){
/* CARD_ID_MATSU_TRIBE_DECOY	7301
Matsu-Tribe Decoy	|2|G
Creature - Snake Warrior 1/3
|2|G: Target creature blocks ~ this turn if able.
Whenever ~ deals combat damage to a creature, tap that creature and it doesn't untap during its controller's next untap step. */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );

		card_instance_t *instance = get_card_instance( player, card );

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td)  ){
				target_must_block_me(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 1);
			}
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(2, 1), 0, &td, "TARGET_CREATURE");
	}

	return freeze_when_damage(player, card, event);
}

int card_meloku_the_clouded_mirror(int player, int card, event_t event){
/* CARD_ID_MELOKU_THE_CLOUDED_MIRROR	7302
Meloku the Clouded Mirror	|4|U
Legendary Creature - Moonfolk Wizard 2/4
Flying
|1, Return a land you control to its owner's hand: Create a 1/1 |Sblue Illusion creature token with flying. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0, &td,
								"Choose a land you control.");
		if( spell_fizzled != 1 ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card );
		}
		instance->number_of_targets = 0;
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ILLUSION_TOKEN, &token);
		token.color_forced = get_sleighted_color_test(instance->parent_controller, instance->parent_card, COLOR_TEST_BLUE);
		token.pow = token.tou = 1;
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return 0;
}

int card_minamo_school_at_waters_edge(int player, int card, event_t event){
	/* Minamo, School at Water's Edge	""
	 * Legendary Land
	 * |T: Add |U to your mana pool.
	 * |U, |T: Untap target legendary permanent. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_UNTAP_LEGEND
	};

	if( event == EVENT_ACTIVATE ){
		int ab = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0, &td, NULL);
		int choice = CHOICE_MANA;
		if( ab && ! paying_mana() ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Untap target Legend", ab, 5);

			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_UNTAP_LEGEND ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_U(2), 0,
									&td, "TARGET_LEGENDARY_PERMANENT");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_UNTAP_LEGEND ){
			if(valid_target(&td) ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_myojin_of_cleansing_fire(int player, int card, event_t event){
/* CARD_ID_MYOJIN_OF_CLEANSING_FIRE	7309
Myojin of Cleansing Fire	|5|W|W|W
Legendary Creature - Spirit 4/6
~ enters the battlefield with a divinity counter on it if you cast it from your hand.
~ has indestructible as long as it has a divinity counter on it.
Remove a divinity counter from ~: Destroy all other creatures. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		APNAP(p, {new_manipulate_all(instance->parent_controller, instance->parent_card, p, &this_test, KILL_DESTROY);};);
	}

	return myojin(player, card, event);
}

int card_myojin_of_infinite_rage(int player, int card, event_t event){
/* CARD_ID_MYOJIN_OF_INFINITE_RAGE	7310
Myojin of Infinite Rage	|7|R|R|R
Legendary Creature - Spirit 7/4
~ enters the battlefield with a divinity counter on it if you cast it from your hand.
~ has indestructible as long as it has a divinity counter on it.
Remove a divinity counter from ~: Destroy all lands. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		APNAP(p, {new_manipulate_all(instance->parent_controller, instance->parent_card, p, &this_test, KILL_DESTROY);};);
	}

	return myojin(player, card, event);
}

int card_myojin_of_lifes_web(int player, int card, event_t event){
/* CARD_ID_MYOJIN_OF_LIFES_WEB	7311
Myojin of Life's Web	|6|G|G|G
Legendary Creature - Spirit 8/8
~ enters the battlefield with a divinity counter on it if you cast it from your hand.
~ has indestructible as long as it has a divinity counter on it.
Remove a divinity counter from ~: Put any number of creature cards from your hand onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
		this_test.qty = count_deck(player);
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return myojin(player, card, event);
}

int card_myojin_of_nights_reach(int player, int card, event_t event){
/* CARD_ID_MYOJIN_OF_NIGHTS_REACH	7312
Myojin of Night's Reach	|5|B|B|B
Legendary Creature - Spirit 5/2
~ enters the battlefield with a divinity counter on it if you cast it from your hand.
~ has indestructible as long as it has a divinity counter on it.
Remove a divinity counter from ~: Each opponent discards his or her hand. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		new_discard_all(1-player, player);
	}
	return myojin(player, card, event);
}

int card_myojin_of_seeing_winds(int player, int card, event_t event){
/* CARD_ID_MYOJIN_OF_SEEING_WINDS	7313
Myojin of Seeing Winds	|7|U|U|U
Legendary Creature - Spirit 3/3
~ enters the battlefield with a divinity counter on it if you cast it from your hand.
~ has indestructible as long as it has a divinity counter on it.
Remove a divinity counter from ~: Draw a card for each permanent you control. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, count_permanents_by_type(player, TYPE_PERMANENT));
	}
	return myojin(player, card, event);
}

int card_nagao_bound_by_honor(int player, int card, event_t event){
/* CARD_ID_NAGAO_BOUND_BY_HONOR	7315
Nagao, Bound by Honor	|3|W
Legendary Creature - Human Samurai 3/3
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.)
Whenever ~ attacks, Samurai creatures you control get +1/+1 until end of turn. */

	check_legend_rule(player, card, event);
	// Whenever ~ attacks, Samurai creatures you control get +1/+1 until end of turn.
	if (declare_attackers_trigger(player, card, event, 0, player, card)){
		pump_subtype_until_eot(player, card, player, SUBTYPE_SAMURAI, 1, 1, 0, 0);
	}
	return bushido(player, card, event, 1);
}

int card_natures_will(int player, int card, event_t event){
/* CARD_ID_NATURES_WILL	7316
Nature's Will	|2|G|G
Enchantment
Whenever one or more creatures you control deal combat damage to a player, tap all lands that player controls and untap all lands you control. */

	card_instance_t* damage = combat_damage_being_dealt(event);
	if( damage &&
		damage->damage_source_player == player &&
		damage->damage_target_card == -1 &&
		!check_special_flags(damage->damage_source_player, damage->damage_source_card, SF_ATTACKING_PWALKER) &&
		(damage->targets[3].player & TYPE_CREATURE))	// probably redundant to status check
	{
		get_card_instance(player, card)->targets[8].player = 1;
	}

	if( resolve_damage_trigger(player, card, event, 0, player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	return global_enchantment(player, card, event);
}

int card_nezumi_bone_reader(int player, int card, event_t event){
/* CARD_ID_NEZUMI_BONE_READER	7317
Nezumi Bone-Reader	|1|B
Creature - Rat Shaman 1/1
|B, Sacrifice a creature: Target player discards a card. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET |GAA_SACRIFICE_CREATURE,
									MANACOST_B(1), 0, &td, "TARGET_PLAYER");
}

int card_nezumi_cutthroat(int player, int card, event_t event){
/* CARD_ID_NEZUMI_CUTTHROAT	7318
Nezumi Cutthroat	|1|B
Creature - Rat Warrior 2/1
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
~ can't block. */

	fear(player, card, event);
	cannot_block(player, card, event);
	return 0;
}

int card_nezumi_graverobber(int player, int card, event_t event ){
/*
Nezumi Graverobber |1|B
Creature — Rat Rogue 2/1
{1}{B}: Exile target card from an opponent's graveyard. If no cards are in that graveyard, flip Nezumi Graverobber.
*/
	double_faced_card(player, card, event);

	card_instance_t *instance = get_card_instance( player, card );
	test_definition_t test;
	default_test_definition(&test, TYPE_ANY);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, 1-player, 0);
		if( selected != -1 ){
			rfg_card_from_grave(1-player, selected);
			if( get_grave(1-player)[0] == -1 ){
				true_transform(player, instance->parent_card);
				verify_legend_rule(player, instance->parent_card, CARD_ID_NIGHTEYES_THE_DESECRATOR);
			}
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_XB(1, 1), 0, NULL, NULL, TYPE_ANY,
															&test, GS_GRAVE_RECYCLER_OPP_GRAVE);
}

int card_nighteyes_the_desecrator(int player, int card, event_t event){

	/* Nighteyes the Desecrator
	 * Legendary Creature - Rat Wizard
	 * 4/2
	 * |4|B: Put target creature card from a graveyard onto the battlefield under your control. */

	check_legend_rule(player, card, event);
	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );

		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			reanimate_permanent(instance->parent_controller, instance->parent_card,
								instance->targets[0].player, selected, REANIMATE_DEFAULT);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_XB(4, 1), 0, NULL, NULL, TYPE_CREATURE,
															&test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
}

int card_nezumi_shortfang(int player, int card, event_t event){
/* CARD_ID_NEZUMI_SHORTFANG	7322
Nezumi Shortfang	|1|B
Creature - Rat Rogue 1/1
|1|B, |T: Target opponent discards a card. Then if that player has no cards in hand, flip ~.
--FLIP--
Stabwhisker the Odious
Legendary Creature - Rat Shaman
3/3
At the beginning of each opponent's upkeep, that player loses 1 life for each card fewer than three in his or her hand. */

	double_faced_card(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( would_validate_arbitrary_target(&td, 1-player, -1)  ){
			discard(1-player, 0, player);
			if( hand_count[1-player] < 1 ){
				true_transform(instance->parent_controller, instance->parent_card);
				verify_legend_rule(instance->parent_controller, instance->parent_card, CARD_ID_STABWHISKER_THE_ODIOUS);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_UNTAPPED, MANACOST_XB(1, 1), 0, &td, "TARGET_OPPONENT");
}

int card_stabwhisker_the_odious(int player, int card, event_t event){
/* CARD_ID_STABWHISKER_THE_ODIOUS	7323
Stabwhisker the Odious	|1|B
Legendary Creature - Rat Shaman 3/3
At the beginning of each opponent's upkeep, that player loses 1 life for each card fewer than three in his or her hand. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, 1-player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int life_loss = 3;
		life_loss-=hand_count[1-player];
		if( life_loss > 0 ){
			lose_life(1-player, life_loss);
		}
	}

	return 0;
}

int card_night_dealings(int player, int card, event_t event){
/* CARD_ID_NIGHT_DEALINGS	7324
Night Dealings	|2|B|B
Enchantment
Whenever a source you control deals damage to another player, put that many theft counters on ~.
|2|B|B, Remove X theft counters from ~: Search your library for a nonland card with converted mana cost X, reveal it, and put it into your hand.
Then shuffle your library. */

	check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_OPPONENT, player, card, TYPE_ANY, NULL);

	int result = resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_OPPONENT, player, card);
	if( result ){
		add_counters(player, card, COUNTER_THEFT, result);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		get_card_instance(player, card)->info_slot = 0;
		generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 2), 0, NULL, NULL);
		if( spell_fizzled != 1 && count_counters(player, card, COUNTER_THEFT) ){
			int amount = internal_rand(count_counters(player, card, COUNTER_THEFT));
			if( player == HUMAN ){
				int number = choose_a_number(player, "Remove how many counters?", amount);
				if( number > 0 && number <= amount ){
					amount = number;
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			get_card_instance(player, card)->info_slot = amount;
			remove_counters(player, card, COUNTER_THEFT, amount);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = get_card_instance(player, card)->info_slot;

		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.type_flag = DOESNT_MATCH;
		test.cmc = amount;
		scnprintf(test.message, 100, "Select target nonland card with CMC %d", amount);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

	return global_enchantment(player, card, event);
}

int card_night_of_souls_betrayal(int player, int card, event_t event)
{
  /* Night of Souls' Betrayal	|2|B|B
   * Legendary Enchantment
   * All creatures get -1/-1. */

	check_legend_rule(player, card, event);
	boost_subtype(player, card, event, -1, -1,-1, 0,0, BCT_INCLUDE_SELF);
	return global_enchantment(player, card, event);
}

int card_no_dachi(int player, int card, event_t event){
/* CARD_ID_NO_DACHI	7326
No-Dachi	|2
Artifact - Equipment
Equipped creature gets +2/+0 and has first strike.
Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 3, 2, 0, KEYWORD_FIRST_STRIKE, 0);
}

static int oathkeeper_reanimation_legacy(int player, int card, event_t event){
	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		int owner = instance->targets[0].player;
		int iid = instance->targets[0].card;
		seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATE_DEFAULT);
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

static int oathkeeper_exile_legacy(int player, int card, event_t event){
	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(instance->targets[0].player, instance->targets[0].card) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_oathkeeper_takenos_daisho(int player, int card, event_t event){
/*
Oathkeeper, Takeno's Daisho |3
Legendary Artifact — Equipment
Equipped creature gets +3/+1.
Whenever equipped creature dies, return that card to the battlefield under your control if it's a Samurai card.
When Oathkeeper, Takeno's Daisho is put into a graveyard from the battlefield, exile equipped creature.
Equip {2}
*/

	check_legend_rule(player, card, event);

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_GRAVEYARD_FROM_PLAY ){
			if( affect_me(p, c) && has_subtype(p, c, SUBTYPE_SAMURAI) && is_this_dying(p, c) ){
				int legacy = create_legacy_effect(player, card, &oathkeeper_reanimation_legacy);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->targets[0].player = get_owner(p, c);
				leg->targets[0].card = get_original_internal_card_id(p, c);
				leg->targets[11].player = 1;
			}
			if( affect_me(player, card) && is_this_dying(player, card) ){
				int legacy = create_legacy_effect(player, card, &oathkeeper_exile_legacy);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->targets[0].player = p;
				leg->targets[0].card = c;
				leg->number_of_targets = 1;
				leg->targets[11].player = 1;
			}
		}
	}

	return vanilla_equipment(player, card, event, 2, 3, 1, 0, 0);
}

int card_okina_temple_to_the_grandfathers(int player, int card, event_t event){
/* CARD_ID_OKINA_TEMPLE_TO_THE_GRANDFATHERS	7329
Okina, Temple to the Grandfathers	""
Legendary Land
|T: Add |G to your mana pool.
|G, |T: Target legendary creature gets +1/+1 until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( ! paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_G(2), 0, &td, "TARGET_CREATURE") ){
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Pump a Legend\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			instance->number_of_targets = instance->info_slot = 0;
			instance->state |= STATE_TAPPED;
			if (charge_mana_for_activated_ability(player, card, MANACOST_G(1)) && pick_target(&td, "TARGET_LEGENDARY_CREATURE") ){
				instance->info_slot = 1;
				tap_card(player, card); // Now get the proper event sent
			}
			else{
				untap_card_no_event(player, card);
			}
		}

		if (choice == 2){
			cancel = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
			}
		}
	}

	return 0;
}

#pragma message "currently this only adds SUBTYPE_DEMON"
int card_oni_possession(int player, int card, event_t event ){
/*
Oni Possession |2|B
Enchantment — Aura
Enchant creature
At the beginning of your upkeep, sacrifice a creature.
Enchanted creature gets +3/+3 and has trample.
Enchanted creature is a Demon Spirit.
*/
	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player > -1 && ! is_humiliated(player, card) ){

		upkeep_trigger_ability(player, card, event, player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			player_sacrifices_a_permanent(player, card, player, TYPE_CREATURE, SAC_CAUSED | SAC_NO_CANCEL);
		}
	}

	return generic_aura(player, card, event, player, 3, 3, KEYWORD_TRAMPLE, 0, SUBTYPE_DEMON, 0, 0);
}

int card_orochi_eggwatcher(int player, int card, event_t event){
	/* Orochi Eggwatcher	|2|G
	 * Creature - Snake Shaman 1/1
	 * |2|G, |T: Put a 1/1 |Sgreen Snake creature token onto the battlefield. If you control ten or more creatures, flip ~.
	 * --FLIP--
	 * Shidako, Broodmistress
	 * Legendary Creature - Snake Shaman
	 * |G, Sacrifice a creature: Target creature gets +3/+3 until end of turn.
	 * 3/3 */

	double_faced_card(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance= get_card_instance(player, card);

		generate_token_by_id(player, card, CARD_ID_SNAKE);
		if( count_subtype(player, TYPE_CREATURE, -1) >= 10 ){
			true_transform(instance->parent_controller, instance->parent_card);
			verify_legend_rule(instance->parent_controller, instance->parent_card, get_id(instance->parent_controller, instance->parent_card));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, 2, 0, 0, 1, 0, 0, 0, 0, 0);
}

int card_petals_of_insight(int player, int card, event_t event){
/* CARD_ID_PETALS_OF_INSIGHT	7345
Petals of Insight	|4|U
Sorcery - Arcane
Look at the top three cards of your library. You may put those cards on the bottom of your library in any order.
If you do, return ~ to its owner's hand. Otherwise, draw three cards. */

	if( event == EVENT_RESOLVE_SPELL ){
		int max = MIN(count_deck(player), 3);
		int kill_me = 1;
		if( max > 0 ){
			if( player == AI && max < 3 ){
				kill_me = 0;
			}
			else{
				int *deck = deck_ptr[player];
				if( show_deck(player, deck, max, "Click on a 'cancel' to draw 3 cards", 0, 0x7375B0 ) == -1 ){
					draw_cards(player, 3);
				}
				else{
					kill_me = 0;
				}
			}
		}
		if( kill_me == 1 ){
			kill_card(player, card, KILL_DESTROY);
		}
		else{
			if( max > 1 ){
				put_top_x_on_bottom(player, player, max);
			}
			bounce_permanent(player, card);
		}
	}

	return basic_spell(player, card, event);
}

int card_shidako_broodmistress(int player, int card, event_t event){
/* CARD_ID_SHIDAKO_BROODMISTRESS	7335
Shidako, Broodmistress	|2|G
Legendary Creature - Snake Shaman 3/3
|G, Sacrifice a creature: Target creature gets +3/+3 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 3, 3);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
}

int card_orochi_hatchery(int player, int card, event_t event){
	/* Orochi Hatchery	|X|X
	 * Artifact
	 * ~ enters the battlefield with X charge counters on it.
	 * |5, |T: Put a 1/1 |Sgreen Snake creature token onto the battlefield for each charge counter on ~. */

	if( event == EVENT_MODIFY_COST ){
		if( check_battlefield_for_id(2, CARD_ID_GADDOCK_TEEG) ){
			infinite_casting_cost();
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->info_slot = 0;
		if( ! is_token(player, card) && ! played_for_free(player, card) ){
			if( player == AI ){
				if( instance->targets[1].player < 0 ){
					instance->targets[1].player = get_average_cmc(1-player);
				}
				if( ! has_mana(player, COLOR_ARTIFACT, instance->targets[1].player*2) ){
					ai_modifier-=1000;
				}
				else{
					charge_mana(player, COLOR_ARTIFACT, instance->targets[1].player*2);
					if( spell_fizzled != 1 ){
						int result = ((instance->targets[1].player*2) + true_get_updated_casting_cost(player, card, -1, event, -1))/2;
						instance->info_slot = result;
					}
				}
			}
			else{
				int result = (charge_mana_for_double_x(player, COLOR_ARTIFACT) + true_get_updated_casting_cost(player, card, -1, event, -1))/2;
				instance->info_slot = result;
			}
		}
		set_special_flags2(player, card, SF2_X_SPELL);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_CHARGE, instance->info_slot);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		generate_tokens_by_id(player, card, CARD_ID_SNAKE, count_counters(instance->parent_controller, instance->parent_card, COUNTER_CHARGE));
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(5), 0, NULL, NULL);
}

int card_orochi_leafcaller(int player, int card, event_t event){
/* CARD_ID_OROCHI_LEAFCALLER	7337
Orochi Leafcaller	|G
Creature - Snake Shaman 1/1
|G: Add one mana of any color to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			produce_mana(player, COLOR_ANY, 1);
		}
	}

	return 0;
}

int card_orochi_ranger(int player, int card, event_t event){
/* CARD_ID_OROCHI_RANGER	7338
Orochi Ranger	|1|G
Creature - Snake Warrior 2/1
Whenever ~ deals combat damage to a creature, tap that creature and it doesn't untap during its controller's next untap step. */

	return freeze_when_damage(player, card, event);
}

// orochi sustainer --> llanowar elves

int card_otherworldly_journey(int player, int card, event_t event){
/* CARD_ID_OTHERWORLDLY_JOURNEY	7340
Otherworldly Journey	|1|W
Instant - Arcane
Exile target creature. At the beginning of the next end step, return that card to the battlefield under its owner's control with a +1/+1 counter on it.*/

	// the +1/+1 effect is embedded in "remove_until_eot" function

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = player;
	td2.required_state = TARGET_STATE_DYING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( player == HUMAN ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td2, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( player == HUMAN ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature you control.", 1, NULL);
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_REGENERATION, &td2, "Select target creature you control.", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int valid = (player == HUMAN && valid_target(&td)) || (player == AI && valid_target(&td2)) ? 1 : 0;
		if( valid ){
			remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
			kill_card(player, card, KILL_DESTROY );
		}
	}

	return 0;
}

int card_pain_kami(int player, int card, event_t event){
/* CARD_ID_PAIN_KAMI	7341
Pain Kami	|2|R
Creature - Spirit 2/2
|X|R, Sacrifice ~: ~ deals X damage to target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_X(-1), 0, &td, "TARGET_CREATURE");
}

int card_painwracker_oni(int player, int card, event_t event){
/* CARD_ID_PAINWRACKER_ONI	7342
Painwracker Oni	|3|B|B
Creature - Demon Spirit 5/4
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
At the beginning of your upkeep, sacrifice a creature if you don't control an Ogre. */

	fear(player, card, event);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		player_sacrifices_a_permanent(player, card, player, TYPE_CREATURE, SAC_CAUSED | SAC_NO_CANCEL);
	}

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		upkeep_trigger_ability_mode(player, card, event, player, check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_OGRE) ?
									0 : RESOLVE_TRIGGER_MANDATORY);
	}

	return 0;
}

ARCANE(card_part_the_veil)
{
  /* Part the Veil	|3|U
   * Instant - Arcane
   * Return all creatures you control to their owner's hand. */

  if (event == EVENT_CAN_CAST)
	return basic_spell(player, card, event);

  if (event == EVENT_RESOLVE_SPELL)
	manipulate_type(player, card, player, TYPE_CREATURE, ACT_BOUNCE);

  return 0;
}

ARCANE(card_peer_through_depths){
	/* Look at the top five cards of your library. You may reveal an instant or sorcery card from among them and put it into your hand. Put the rest on the
	 * bottom of your library in any order. */
	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}
	if( event == EVENT_RESOLVE_SPELL ){
		reveal_x_and_choose_a_card_type(player, card, 5, TYPE_SORCERY | TYPE_INSTANT);
	}
	return 0;
}

int card_pious_kitsune(int player, int card, event_t event){

	/* At the beginning of your upkeep, put a devotion counter on ~. Then if a creature named Eight-and-a-Half-Tails is on the battlefield, you gain 1 life for
	 * each devotion counter on ~.
	 * |T, Remove a devotion counter from ~: You gain 1 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counter(player, card, COUNTER_DEVOTION);
		if( check_battlefield_for_id(ANYBODY, CARD_ID_EIGHT_AND_A_HALF_TAILS) ){
			gain_life(player, count_counters(player, card, COUNTER_DEVOTION));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTER(COUNTER_DEVOTION), NULL, NULL);
}

ARCANE_WITH_SPLICE(card_psychic_puppetry, MANACOST_U(1))
{
/* CARD_ID_PSYCHIC_PUPPETRY	7348
Psychic Puppetry	|1|U
Instant - Arcane
You may tap or untap target permanent.
Splice onto Arcane |U (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT );

	if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);

	if (event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PERMANENT", 1, NULL);
		ai_modifier_twiddle(player, card, 0);
	}

	if (event == EVENT_RESOLVE_SPELL && valid_target(&td))
		twiddle(player, card, 0);

	return 0;
}

ARCANE(card_quiet_purity){
/* CARD_ID_QUIET_PURITY	7350
Quiet Purity	|W
Instant - Arcane
Destroy target enchantment. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

ARCANE(card_reach_through_mists){
/* CARD_ID_REACH_THROUGH_MISTS	7353
Reach Through Mists	|U
Instant - Arcane
Draw a card. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 1);
	}
	return 0;
}

ARCANE(card_rend_flesh){
/* CARD_ID_REND_FLESH	7356
Rend Flesh	|2|B
Instant - Arcane
Destroy target non-Spirit creature. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_SPIRIT;
	td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target non-Spirit creature.", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_ronin_houndmaster(int player, int card, event_t event){
/* CARD_ID_RONIN_HOUNDMASTER	7361
Ronin Houndmaster	|2|R
Creature - Human Samurai 2/2
Haste
Bushido 1 (Whenever this creature blocks or becomes blocked, it gets +1/+1 until end of turn.) */

	haste(player, card, event);
	return bushido(player, card, event, 1);
}

int card_rootrunner(int player, int card, event_t event){
/* CARD_ID_ROOTRUNNER	7362
Rootrunner	|2|G|G
Creature - Spirit 3/3
|G|G, Sacrifice ~: Put target land on top of its owner's library.
Soulshift 3 (When this creature dies, you may return target Spirit card with converted mana cost 3 or less from your graveyard to your hand.) */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND );

		card_instance_t *instance= get_card_instance(player, card);
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
			}
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_G(2), 0, &td, "TARGET_LAND");
	}

	return soulshift(player, card, event, 3, 1);
}

int card_ryusei(int player, int card, event_t event){
/* CARD_ID_RYUSEI_THE_FALLING_STAR	7363
Ryusei, the Falling Star	|5|R
Legendary Creature - Dragon Spirit 5/5
Flying
When ~ dies, it deals 5 damage to each creature without flying. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = DOESNT_MATCH;
		APNAP(p, {new_damage_all(player, card, p, 5, 0, &this_test);};);
	}

	return 0;
}

int card_sachi_daughter_of_seshiro(int player, int card, event_t event){
/* CARD_ID_SACHI_DAUGHTER_OF_SESHIRO	7364
Sachi, Daughter of Seshiro	|2|G|G
Legendary Creature - Snake Shaman 1/3
Other Snake creatures you control get +0/+1.
Shamans you control have "|T: Add |G|G to your mana pool." */

	check_legend_rule(player, card, event);

	int result = permanents_you_control_can_tap_for_mana(player, card, event, TYPE_PERMANENT, SUBTYPE_SHAMAN, COLOR_GREEN, 2);
	if (event == EVENT_CAN_ACTIVATE){
		return result;
	}

	return boost_creature_type(player, card, event, SUBTYPE_SNAKE, 0, 1, 0, BCT_CONTROLLER_ONLY);
}

int card_sakura_tribe_elder(int player, int card, event_t event){
	/*
	  Sakura-Tribe Elder |1|G
	  Creature - Snake Shaman 1/1
	  Sacrifice Sakura-Tribe Elder: Search your library for a basic land card, put that card onto the battlefield tapped, then shuffle your library.
	*/
	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_land(player, 1, 1);
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_samurai_enforcers(int player, int card, event_t event){
/* CARD_ID_SAMURAI_ENFORCERS	7365
Samurai Enforcers	|4|W|W
Creature - Human Samurai 4/4
Bushido 2 (Whenever this creature blocks or becomes blocked, it gets +2/+2 until end of turn.) */

	return bushido(player, card, event, 2);
}

int card_samurai_of_the_pale_courtain(int player, int card, event_t event)
{
  /* Samurai of the Pale Curtain	|W|W
   * Creature - Fox Samurai 2/2
   * Bushido 1
   * If a permanent would be put into a graveyard, exile it instead. */

	if (event == EVENT_GRAVEYARD_FROM_PLAY)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_PERMANENT, "");
	  if_a_card_would_be_put_into_graveyard_from_play_exile_it_instead(player, card, event, ANYBODY, &test);
	}

	return bushido(player, card, event, 1);
}

int card_seizan_perverter_of_truth(int player, int card, event_t event){
/* CARD_ID_SEIZAN_PERVERTER_OF_TRUTH	7368
Seizan, Perverter of Truth	|3|B|B
Legendary Creature - Demon Spirit 6/5
At the beginning of each player's upkeep, that player loses 2 life and draws two cards. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(current_turn, 2);
		draw_cards(current_turn, 2);
	}

	return 0;
}

static int training_counter(int player, int card, event_t event){
	card_instance_t *instance= get_card_instance(player, card);
	return bushido(instance->targets[0].player, instance->targets[0].card, event, 0);
}

int card_sensei_golden_tail(int player, int card, event_t event){
/* Sensei Golden-Tail	|1|W
 * Legendary Creature - Fox Samurai 2/1
 * Bushido 1
 * |1|W, |T: Put a training counter on target creature. That creature gains bushido 1 and becomes a Samurai in addition to its
 * 		other creature types. Activate this ability only any time you could cast a sorcery. */

	check_legend_rule(player, card, event);

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;

		card_instance_t *instance= get_card_instance(player, card);
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				add_counter(instance->targets[0].player, instance->targets[0].card, COUNTER_TRAINING);	// does nothing, per rulings
				add_a_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_SAMURAI);
				card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
				target->targets[8].player++;
				if( ! check_for_special_ability(instance->targets[0].player, instance->targets[0].card, SP_KEYWORD_BUSHIDO) ){
					create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &training_counter,
												instance->targets[0].player, instance->targets[0].card);
				}
			}
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XW(1, 1), 0,
										&td, "TARGET_CREATURE");
	}

	return bushido(player, card, event, 1);
}

int card_senseis_divining_top(int player, int card, event_t event){
/* CARD_ID_SENSEIS_DIVINING_TOP	1543
Sensei's Divining Top	|1
Artifact
|1: Look at the top three cards of your library, then put them back in any order.
|T: Draw a card, then put ~ on top of its owner's library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_REARRANGE_3 = 1,
		CHOICE_DRAW
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int abilities[3] = {
							0,
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Rearrange top 3 cards", 	abilities[1], 5,
							"Draw a card and flip Top", abilities[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_REARRANGE_3 ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL);
		}

		if( choice == CHOICE_DRAW ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REARRANGE_3 ){
			rearrange_top_x(player, player, 3);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_a_card(player);
			if( in_play(instance->parent_controller, instance->parent_card) ){
				put_on_top_of_deck(instance->parent_controller, instance->parent_card);
			}
		}
	}
	return 0;
}

int card_seshiro_the_anointed(int player, int card, event_t event){
/* CARD_ID_SESHIRO_THE_ANOINTED	7371
Seshiro the Anointed	|4|G|G
Legendary Creature - Snake Monk 3/4
Other Snake creatures you control get +2/+2.
Whenever a Snake you control deals combat damage to a player, you may draw a card. */

	check_legend_rule(player, card, event);

	if( subtype_deals_damage(player, card, event, player, SUBTYPE_SNAKE, DDBM_MUST_BE_COMBAT_DAMAGE+DDBM_MUST_DAMAGE_PLAYER) ){
		draw_some_cards_if_you_want(player, card, player, get_card_instance(player, card)->targets[1].card);
		get_card_instance(player, card)->targets[1].card = 0;
	}

	return boost_creature_type(player, card, event, SUBTYPE_SNAKE, 2, 2, 0, BCT_CONTROLLER_ONLY);
}

int card_shimatsu_the_bloodcloaked(int player, int card, event_t event)
{
/* CARD_ID_SHIMATSU_THE_BLOODCLOAKED	7373
Shimatsu the Bloodcloaked	|3|R
Legendary Creature - Demon Spirit 0/0
As ~ enters the battlefield, sacrifice any number of permanents. ~ enters the battlefield with that many +1/+1 counters on it. */

	check_legend_rule(player, card, event);

	// As ~ enters the battlefield, sacrifice any number of permanents. Shimatsu enters the battlefield with that many +1/+1 counters on it.
	if (event == EVENT_RESOLVE_SPELL)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "");
		test.not_me = 1;

		card_instance_t* instance = get_card_instance(player, card);
		instance->state |= STATE_OUBLIETTED;	// so it isn't destroyed after each sacrifice

		int n = 0;
		while (new_sacrifice(player, card, player, 0, &test))
			++n;

		instance->state &= ~STATE_OUBLIETTED;
		add_1_1_counters(player, card, n);
	}

	return 0;
}

int card_shinka_the_bloodsoaked_keep(int player, int card, event_t event){
/* Shinka, the Bloodsoaked Keep	""
 * Legendary Land
 * |T: Add |R to your mana pool.
 * |R, |T: Target legendary creature gains first strike until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_ACTIVATE ){
		int choice = instance->number_of_targets = instance->info_slot = 0;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(2), 0, &td, NULL) )
		{
			choice = do_dialog(player, player, card, -1, -1, " Produce mana\n Give First Strike\n Cancel", 1);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}

		if( choice == 1 ){
			instance->state |= STATE_TAPPED;
			if (charge_mana_for_activated_ability(player, card, MANACOST_R(1)) && pick_target(&td, "TARGET_LEGENDARY_CREATURE") ){
				instance->info_slot = 1;
				tap_card(player, card); // Now get the proper event sent
			}
			else{
				untap_card_no_event(player, card);
			}
		}

		if (choice == 2){
			cancel = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 ){
			if( valid_target(&td) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FIRST_STRIKE, 0);
			}
		}
	}

	return 0;
}

int card_shisato_whispering_hunter(int player, int card, event_t event){
/* CARD_ID_SHISATO_WHISPERING_HUNTER	7375
Shisato, Whispering Hunter	|3|G
Legendary Creature - Snake Warrior 2/2
At the beginning of your upkeep, sacrifice a Snake.
Whenever ~ deals combat damage to a player, that player skips his or her next untap step. */

	check_legend_rule(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.subtype = SUBTYPE_SNAKE;
		new_sacrifice(player, card, player, SAC_NO_CANCEL, &this_test);
	}

	if( has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
		target_player_skips_next_untap(player, card, 1-player);
	}

	return 0;
}

int card_shizo_deaths_storehouse(int player, int card, event_t event){
	/* Shizo, Death's Storehouse	""
	 * Legendary Land
	 * |T: Add |B to your mana pool.
	 * |B, |T: Target legendary creature gains fear until end of turn. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_LEGEND;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_FEAR
	};

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = instance->info_slot = 0;
		int choice = CHOICE_MANA;
		int ab = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_B(2), 0, &td, NULL);
		if( ! paying_mana() && ab ){
			choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Get mana", 1, 1,
							"Give Fear to Legendary creature", ab, 5);
		}
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_FEAR ){
			instance->state |= STATE_TAPPED;
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_B(1), 0, &td, "TARGET_LEGENDARY_CREATURE");
			if( spell_fizzled == 1 ){
				untap_card_no_event(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_FEAR ){
			if(valid_target(&td) ){
				pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_FEAR);
			}
		}
	}

	return 0;
}

ARCANE(card_sift_through_sands){
/* CARD_ID_SIFT_THROUGH_SANDS	7378
Sift Through Sands	|1|U|U
Instant - Arcane
Draw two cards, then discard a card.
If you've cast a spell named Peer Through Depths and a spell named Reach Through Mists this turn, you may search your library for a card named The Unspeakable,
 put it onto the battlefield, then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		discard(player, 0, player);
		if( get_trap_condition(player, TRAP_PEER_THROUGH_DEPTHS) && get_trap_condition(player, TRAP_REACH_THROUGH_MISTS) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card named The Unspeakable.");
			this_test.id = CARD_ID_THE_UNSPEAKABLE;
			new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_silent_chant_zubera(int player, int card, event_t event){
/* CARD_ID_SILENT_CHANT_ZUBERA	7379
Silent-Chant Zubera	|1|W
Creature - Zubera Spirit 1/2
When ~ dies, you gain 2 life for each Zubera that died this turn. */

	if( this_dies_trigger(player, card, event, 2) ){
		gain_life(player, get_zubera_count()*2);
	}

	return 0;
}

int card_soratami_cloudskater(int player, int card, event_t event){
/* CARD_ID_SORATAMI_CLOUDSKATER	7383
Soratami Cloudskater	|1|U
Creature - Moonfolk Rogue 1/1
Flying
|2, Return a land you control to its owner's hand: Draw a card, then discard a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance= get_card_instance(player, card);


	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "Select a land you control.");
		if( spell_fizzled != 1 ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
		discard(player, 0, player);
	}

	return 0;
}

int card_soratami_savant(int player, int card, event_t event){
/* Soratami Savant	|2|U|U
 * Creature - Moonfolk Wizard 2/2
 * Flying
 * |3, Return a land you control to its owner's hand: Counter target spell unless its controller pays |3. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_target(&td) ){
			return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_X(3), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int land = -1;
		if( new_pick_target(&td, "Select a land you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			land = instance->targets[0].card;
			instance->number_of_targets = 0;
		}
		if( land ){
			generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_X(3), 0, NULL, NULL);
			if( spell_fizzled != 1 ){
				bounce_permanent(player, land);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, 3);
	}

	return 0;
}

int card_soratami_seer(int player, int card, event_t event){
/*
Soratami Seer |4|U
Creature — Moonfolk Wizard 2/3
Flying
{4}, Return two lands you control to their owner's hand: Discard all the cards in your hand, then draw that many cards.
*/
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( target_available(player, card, &td) >= 2 ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(4), 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int land[2];
		int i;
		for(i=0; i<2; i++){
			if( new_pick_target(&td, "Select a land you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				land[i] = instance->targets[0].card;
				instance->number_of_targets = 0;
			}
			else{
				return 0;
			}
		}
		generic_activated_ability(player, card, event, 0, MANACOST_X(4), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			for(i=0; i<2; i++){
				bounce_permanent(player, land[i]);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int amount = hand_count[player];
		new_discard_all(player, player);
		draw_cards(player, amount);
	}

	return 0;
}

int card_sosuke_son_of_seshiro(int player, int card, event_t event){
/* CARD_ID_SOSUKE_SON_OF_SESHIRO	7389
Sosuke, Son of Seshiro	|2|G|G
Legendary Creature - Snake Warrior 3/4
Other Snake creatures you control get +1/+0.
Whenever a Warrior you control deals combat damage to a creature, destroy that creature at end of combat. */

	card_instance_t *instance = get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( subtype_deals_damage(player, card, event, player, SUBTYPE_WARRIOR, DDBM_MUST_BE_COMBAT_DAMAGE+DDBM_MUST_DAMAGE_CREATURE+DDBM_TRACE_DAMAGED_CREATURES) ){
		int i;
		for(i=0; i<instance->targets[1].card; i++){
			if( in_play(instance->targets[i+2].player, instance->targets[i+2].card) ){
				create_targetted_legacy_effect(player, card, &die_at_end_of_combat, instance->targets[i+2].player, instance->targets[i+2].card);
			}
		}
		instance->targets[1].card = 0;
	}

	return boost_creature_type(player, card, event, SUBTYPE_SNAKE, 1, 0, 0, BCT_CONTROLLER_ONLY);
}

int card_soulblast(int player, int card, event_t event){
	/* Soulblast	|3|R|R|R	0x2006128
	 * Instant
	 * As an additional cost to cast this spell, sacrifice all creatures you control.
	 * ~ deals damage to any target equal to the total power of the sacrificed creatures. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		if( can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		get_card_instance(player, card)->info_slot = 0;
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 && ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int count = active_cards_count[player]-1;
			while( count > -1 ){
					if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
						get_card_instance(player, card)->info_slot+=get_power(player, count);
						kill_card(player, count, KILL_SACRIFICE);
					}
					count--;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, get_card_instance(player, card)->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

ARCANE_WITH_SPLICE(card_soulless_revival, MANACOST_XB(1,1)){
/* CARD_ID_SOULLESS_REVIVAL	7392
Soulless Revival	|1|B
Instant - Arcane
Return target creature card from your graveyard to your hand.
Splice onto Arcane |1|B (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return 0;
}

ARCANE_WITH_SPLICE(card_strange_inversion, MANACOST_XR(1,1))
{
/* CARD_ID_STRANGE_INVERSION	7394
Strange Inversion	|2|R
Instant - Arcane
Switch target creature's power and toughness until end of turn.
Splice onto Arcane |1|R (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost. If you do, add this card's effects to that spell.)*/

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = ANYBODY;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

  if (event == EVENT_RESOLVE_SPELL && valid_target(&td))
	switch_power_and_toughness_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);

  return 0;
}

// not in card_id ??
int card_swallowing_plague(int player, int card, event_t event){
/* CARD_ID_SWALLOWING_PLAGUE	15549
Swallowing Plague	|X|B|B
Sorcery - Arcane
~ deals X damage to target creature and you gain X life. */

	return generic_x_spell(player, card, event, TARGET_ZONE_IN_PLAY, COLOR_COLORLESS, 9);
}

int card_takeno_samurai_general(int player, int card, event_t event){
/* CARD_ID_TAKENO_SAMURAI_GENERAL	7400
Takeno, Samurai General	|5|W
Legendary Creature - Human Samurai 3/3
Bushido 2 (Whenever this creature blocks or becomes blocked, it gets +2/+2 until end of turn.)
Each other Samurai creature you control gets +1/+1 for each point of bushido it has. */

	check_legend_rule(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( ! affect_me(player, card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			if( has_subtype(affected_card_controller, affected_card, SUBTYPE_SAMURAI) &&
				check_for_special_ability(affected_card_controller, affected_card, SP_KEYWORD_BUSHIDO) )
			{
				event_result += get_card_instance(affected_card_controller, affected_card)->targets[8].player;
			}
		}
	}

	return bushido(player, card, event, 2);
}

int card_tatsumasa_the_dragon_fang(int player, int card, event_t event){
/* Tatsumasa, the Dragon's Fang	|6
 * Legendary Artifact - Equipment
 * Equipped creature gets +5/+5.
 * |6, Exile ~: Put a 5/5 |Sblue Dragon Spirit creature token with flying onto the battlefield.
			Return ~ to the battlefield under its owner's control when that token dies.
 * Equip |3 */

	check_legend_rule(player, card, event);

	if (is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		modify_pt_and_abilities(instance->damage_target_player, instance->damage_target_card, event, 5, 5, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_DRAGON
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_X(6), 0, NULL, NULL) ){
			return 1;
		}
		return basic_equipment(player, card, event, 3);
	}
	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int abils[3] = {
						0,
						can_activate_basic_equipment(player, card, event, 3),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(6), 0, NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip", abils[1], 5,
							"Exile and generate Dragon", abils[2], 15-(count_subtype(player, TYPE_CREATURE, -1)*5));
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 3);
		}
		if( choice == CHOICE_DRAGON ){
			return generic_activated_ability(player, card, event, GAA_RFG_ME, MANACOST_X(6), 0, NULL, NULL);
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 3);
		}
		if( instance->info_slot == CHOICE_DRAGON ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_DRAGON_SPIRIT, &token);
			token.pow = token.tou = 5;
			token.key_plus = KEYWORD_FLYING;
			token.color_forced = COLOR_TEST_BLUE;
			generate_token(&token);
		}
	}

	return 0;
}

int card_tenza_godos_maul(int player, int card, event_t event){
/*
Tenza, Godo's Maul |3
Legendary Artifact — Equipment
Equipped creature gets +1/+1. As long as it's legendary, it gets an additional +2/+2. As long as it's red, it has trample.
Equip {1} ({1}: Attach to target creature you control. Equip only as a sorcery.)
*/
	check_legend_rule(player, card, event);

	if( is_equipping(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		int pow = is_legendary(instance->damage_target_player, instance->damage_target_card) ? 3 : 1;
		int key = (get_color(instance->damage_target_player, instance->damage_target_card) &
					get_sleighted_color_test(player, card, COLOR_TEST_RED)) ? KEYWORD_TRAMPLE : 0;
		modify_pt_and_abilities(instance->damage_target_player, instance->damage_target_card, event, pow, pow, key);
	}

	return basic_equipment(player, card, event, 1);
}

int card_the_unspeakable(int player, int card, event_t event){
/* CARD_ID_THE_UNSPEAKABLE	7405
The Unspeakable	|6|U|U|U
Legendary Creature - Spirit 6/7
Flying, trample
Whenever ~ deals combat damage to a player, you may return target Arcane card from your graveyard to your hand. */

	check_legend_rule(player, card, event);

	if( has_combat_damage_been_inflicted_to_a_player(player, card, event) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select an Arcane card.");
		this_test.subtype = SUBTYPE_ARCANE;
		new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	return 0;
}

int card_thief_of_hope(int player, int card, event_t event){
/* CARD_ID_THIEF_OF_HOPE	7406
Thief of Hope	|2|B
Creature - Spirit 2/2
Whenever you cast a Spirit or Arcane spell, target opponent loses 1 life and you gain 1 life.
Soulshift 2 (When this creature dies, you may return target Spirit card with converted mana cost 2 or less from your graveyard to your hand.) */

	if( arcane_spirit_spell_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		if( would_validate_arbitrary_target(&td, 1-player, -1)  ){
			lose_life(1-player, 1);
			gain_life(player, 1);
		}
	}

	return soulshift(player, card, event, 2, 1);
}

ARCANE_WITH_SPLICE(card_through_the_breach, MANACOST_XR(2,2)){
/* CARD_ID_THROUGH_THE_BREACH	7409
Through the Breach	|4|R
Instant - Arcane
You may put a creature card from your hand onto the battlefield. That creature gains haste. Sacrifice that creature at the beginning of the next end step.
Splice onto Arcane |2|R|R (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		int result = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		if( result > -1 ){
			haste_and_remove_at_beginning_of_next_end_step(player, card, player, result, KILL_SACRIFICE);
		}
	}
	return 0;
}

int card_time_of_need(int player, int card, event_t event){
/* CARD_ID_TIME_OF_NEED	7411
Time of Need	|1|G
Sorcery
Search your library for a legendary creature card, reveal it, and put it into your hand. Then shuffle your library. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a Legendary creature.");
		this_test.subtype = TYPE_CREATURE;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_uyo_silent_prophet(int player, int card, event_t event){
/*
Uyo, Silent Prophet |4|U|U
Legendary Creature — Moonfolk Wizard 4/4
Flying
{2}, Return two lands you control to their owner's hand: Copy target instant or sorcery spell. You may choose new targets for the copy.
*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance= get_card_instance(player, card);

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		if( target_available(player, card, &td) >= 2 && generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL) ){
			return activate_twincast(player, card, event, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			int to_bounce[2];
			int tbc = 0;
			int i;
			for(i=0; i<2; i++){
				if( new_pick_target(&td, "Select a land you control.", 0, 1 | GS_LITERAL_PROMPT) ){
					to_bounce[tbc] = instance->targets[0].card;
					state_untargettable(player, to_bounce[tbc], 1);
					tbc++;
				}
				else{
					break;
				}
			}
			for(i=0; i<tbc; i++){
				state_untargettable(player, to_bounce[i], 0);
			}
			instance->number_of_targets = 0;
			if( tbc == 2 ){
				activate_twincast(player, card, event, NULL, NULL);
				for(i=0; i<2; i++){
					bounce_permanent(player, to_bounce[i]);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		activate_twincast(player, card, event, NULL, NULL);
	}

	return 0;
}

int card_villainous_ogre(int player, int card, event_t event){
/* CARD_ID_VILLAINOUS_OGRE	7423
Villainous Ogre	|2|B
Creature - Ogre Warrior 3/2
~ can't block.
As long as you control a Demon, ~ has "|B: Regenerate ~." */

	cannot_block(player, card, event);

	if( event == EVENT_ABILITIES && affect_me(player, card) && in_play(player, card) && ! is_humiliated(player, card) ){
		if( check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON) ){
			event_result |= KEYWORD_REGENERATION;
		}
	}

	if( event == EVENT_CAN_ACTIVATE && check_battlefield_for_subtype(player, TYPE_PERMANENT, SUBTYPE_DEMON) ){
		return regeneration(player, card, event, MANACOST_B(1));
	}

	if( event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST_B(1));
	}

	return 0;
}

ARCANE(card_waking_nightmare){
/* CARD_ID_WAKING_NIGHTMARE	7425
Waking Nightmare	|2|B
Sorcery - Arcane
Target player discards two cards. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
	}
	return 0;
}

ARCANE_WITH_SPLICE(card_wear_away, MANACOST_XG(3,1)){
/* CARD_ID_WEAR_AWAY	7428
Wear Away	|G|G
Instant - Arcane
Destroy target artifact or enchantment.
Splice onto Arcane |3|G (As you cast an Arcane spell, you may reveal this card from your hand and pay its splice cost.
If you do, add this card's effects to that spell.) */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_wicked_akuba(int player, int card, event_t event){
/*
Wicked Akuba |B|B
Creature — Spirit 2/2
{B}: Target player dealt damage by Wicked Akuba this turn loses 1 life.
*/
	check_damage_test(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS, player, card, 0, NULL);

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->targets[1].player = 0;
		get_card_instance(player, card)->targets[8].player = 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL) ){
			if( BYTE0(instance->targets[1].player) && would_validate_arbitrary_target(&td, 0, -1) ){
				return 1;
			}
			if( BYTE1(instance->targets[1].player) && would_validate_arbitrary_target(&td, 1, -1) ){
				return 1;
			}
		}
	}
	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_B(1)) ){
			int mode = 0;
			if( BYTE0(instance->targets[1].player) && would_validate_arbitrary_target(&td, 0, -1) ){
				instance->targets[0].player = 0;
				instance->targets[0].card = -1;
				instance->number_of_targets = 1;
				mode |= 1;
			}
			if( BYTE1(instance->targets[1].player) && would_validate_arbitrary_target(&td, 1, -1) ){
				instance->targets[0].player = 1;
				instance->targets[0].card = -1;
				instance->number_of_targets = 1;
				mode |= 2;
			}
			if( mode == 3 ){
				instance->number_of_targets = 0;
				pick_target(&td, "TARGET_PLAYER");
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 1);
		}
	}

	return 0;
}

int card_yamabushis_flame(int player, int card, event_t event){
	/* Annihilating Fire	|1|R|R	0x2006173
	 * Instant
	 * ~ deals 3 damage to any target. If a creature dealt damage this way would die this turn, exile it instead. */
	/* Incendiary Flow	|1|R	0x2006173
	 * Sorcery
	 * ~ deals 3 damage to any target. If a creature dealt damage this way would die this turn, exile it instead. */
	/* Touch of the Void	|2|R	0x2006173
	 * Sorcery
	 * Devoid
	 * ~ deals 3 damage to any target. If a creature dealt damage this way would die this turn, exile it instead. */
	/* Yamabushi's Flame	|2|R	0x2006173
	 * Instant
	 * ~ deals 3 damage to any target. If a creature dealt damage this way would die this turn, exile it instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td)  ){
			card_instance_t *instance = get_card_instance( player, card );
			int dc = damage_target0(player, card, 3);
			if( instance->targets[0].card != -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
				get_card_instance(player, dc)->targets[3].card |= DMG_EXILE_IF_LETHALLY_DAMAGED_THIS_WAY;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_yamabushis_storm(int player, int card, event_t event){
/* CARD_ID_YAMABUSHIS_STORM	7431
Yamabushi's Storm	|1|R
Sorcery
~ deals 1 damage to each creature. If a creature dealt damage this way would die this turn, exile it instead. */

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
						int dc = damage_creature(i, count, 1, player, card);
						get_card_instance(player, dc)->targets[3].card |= DMG_EXILE_IF_LETHALLY_DAMAGED_THIS_WAY;
					}
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_yosei(int player, int card, event_t event){
/* CARD_ID_YOSEI_THE_MORNING_STAR	7432
Yosei, the Morning Star	|4|W|W
Legendary Creature - Dragon Spirit 5/5
Flying
When ~ dies, target player skips his or her next untap step. Tap up to five target permanents that player controls. */

	check_legend_rule(player, card, event);

	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			instance->number_of_targets = 1;
			target_player_skips_next_untap(player, card, instance->targets[0].player);

			instance->targets[2] = instance->targets[0];

			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_PERMANENT);
			td1.allowed_controller = instance->targets[2].player;
			td1.preferred_controller = instance->targets[2].player;
			td1.illegal_state = TARGET_STATE_TAPPED;

			int count = 0;
			while( count < 5 && can_target(&td1) ){
					if( pick_target(&td1, "TARGET_PERMANENT") ){
						instance->number_of_targets = 1;
						tap_card( instance->targets[0].player, instance->targets[0].card);
						count++;
					}
					else{
						break;
					}
			}
		}
	}

	return 0;
}

int card_zozu_the_punisher(int player, int card, event_t event){
/* CARD_ID_ZO_ZU_THE_PUNISHER	7433
Zo-Zu the Punisher	|1|R|R
Legendary Creature - Goblin Warrior 2/2
Whenever a land enters the battlefield, ~ deals 2 damage to that land's controller. */

	check_legend_rule(player, card, event);

	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_LAND, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		damage_player(trigger_cause_controller, 2, player, card);
	}

	return 0;
}
