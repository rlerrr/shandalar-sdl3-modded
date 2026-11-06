#include "manalink.h"

// Functions

static int brood_of_cockroaches_legacy(int player, int card, event_t event);
static int giant_caterpillar_legacy(int player, int card, event_t event);

static int fx_when_creature_attacks_it_gains_p1(int player, int card, event_t event);
static int fx_milled_power(int player, int card, event_t event);

static int not_playable_lands_legacy(int player, int card, event_t event);

static int peace_talks_effect(int player, int card, event_t event);

#ifndef SHANDALAR

static int check_for_spitting_drake(int player);
int card_matopi_golem(int player, int card, event_t event);

#endif

static int chimera(int player, int card, event_t event, int key, int s_key){

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_CHIMERA;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P2_P2, 1);
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, key, s_key | SP_KEYWORD_DOES_NOT_END_AT_EOT);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target Chimera creature.");
}

static int vis_karoo(int player, int card, event_t event, color_t color, int subtype)
{
  /* Land
   * ~ enters the battlefield tapped.
   * When ~ enters the battlefield, sacrifice it unless you return an untapped [subtype] you control
   * to its owner's hand. |T: Add |1|C to your mana pool. */

  comes_into_play_tapped(player, card, event);

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    subtype = get_hacked_subtype(player, card, subtype);

    target_definition_t td;
    base_target_definition(player, card, &td, TYPE_LAND);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.required_subtype = subtype;

    card_instance_t *instance = get_card_instance(player, card);
    instance->number_of_targets = 0;
    if (can_target(&td) &&
        select_target(
            player, card, &td,
            get_hacked_land_text(player, card, "Select an untapped %s you control.", subtype),
            NULL))
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    else
      kill_card(player, card, KILL_SACRIFICE);
  }

  return two_mana_land(player, card, event, COLOR_COLORLESS, color);
}

// Card

int card_aku_djinn(int player, int card, event_t event)
{
  /* Aku Djinn	|3|B|B
   * Creature - Djinn 5/6
   * Trample
   * At the beginning of your upkeep, put a +1/+1 counter on each creature each opponent controls. */

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	manipulate_type(player, card, 1-player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));

  return 0;
}

int card_anvil_of_bagardan(int player, int card, event_t event)
{
  /* CARD_ID_ANVIL_OF_BOGARDAN	2670
   * Anvil of Bogardan	|2
   * Artifact
   * Players have no maximum hand size.
   * At the beginning of each player's draw step, that player draws an additional card, then discards a card. */

  card_instance_t *instance;
  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (event == EVENT_DRAW_PHASE)
    {
      if (!is_humiliated(player, card, AL_TRIG))
      {
        event_result = event_result + 1;
        discard(current_turn, 0, player);
      }
    }
    else if (event == EVENT_MAX_HAND_SIZE)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        event_result = 0x31;
      }
    }
  }
  return 0;
}

int card_army_ants(int player, int card, event_t event){
/* CARD_ID_ARMY_ANTS	2671
Army Ants	|1|B|R
Creature - Insect 1/1
|T, Sacrifice a land: Destroy target land. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allow_cancel = 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0,
															&td, "TARGET_LAND", 0, &test, 0);
}

int card_blanket_of_night(int player, int card, event_t event){
/* CARD_ID_BLANKET_OF_NIGHT	2673
Blanket of Night	|1|B|B
Enchantment
Each land is |Ha Swamp in addition to its other land types. */

	if( event == EVENT_SHOULD_AI_PLAY ){
		return should_ai_play(player, card);
	}
	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}
	return card_urborg_tomb_of_yawgmoth(player, card, event);
}

int card_bogardan_phoenix(int player, int card, event_t event)
{
  /* Bogardan Phoenix	|2|R|R|R
   * Creature - Phoenix 3/3
   * Flying
   * When ~ dies, exile it if it had a death counter on it. Otherwise, return it to the battlefield under your control and put a death counter on it. */

  if (event == EVENT_ABILITIES && affect_me(player, card) && !count_counters(player, card, COUNTER_DEATH))
	nice_creature_to_sacrifice(player, card);

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	{
	  int owner, position;
	  if (count_counters(player, card, COUNTER_DEATH))
		exile_from_owners_graveyard(player, card);
	  else if (find_in_owners_graveyard(player, card, &owner, &position))
		{
		  int zombie = reanimate_permanent(player, -1, owner, position, REANIMATE_DEFAULT);
		  add_counters(player, card, player, zombie, COUNTER_DEATH, 1);
		}
	}

  return 0;
}

int card_brass_talon_chimera(int player, int card, event_t event)
{
  /* Brass-Talon Chimera	|4
	 * Artifact Creature - Chimera 2/2
	 * First strike
	 * Sacrifice ~: Put a +2/+2 counter on target Chimera creature. It gains first strike. */

  if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player && affected_card != card &&
      has_subtype(player, affected_card, SUBTYPE_CHIMERA) &&
      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL))
  {
    check_abilities_power += 2;
    check_abilities_toughness += 2;
    check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
  }
  return chimera(player, card, event, KEYWORD_FIRST_STRIKE, 0);
}

int card_breathstealers_crypt(int player, int card, event_t event)
{
  /* Breathstealer's Crypt	|2|U|B
   * Enchantment
   * If a player would draw a card, instead he or she draws a card and reveals it. If it's a creature card, that player discards it unless he or she pays 3
   * life. */

  /* Worded as a replacement effect, but always replaces with a draw.  So we trigger during the post-draw trigger, but respond during EVENT_TRIGGER instead of
   * EVENT_RESOLVE_TRIGGER so that this always triggers first. */

  if (trigger_condition == TRIGGER_CARD_DRAWN && affect_me(player, card) && reason_for_trigger_controller == player
	  && in_play(player, card) && !is_humiliated(player, card, AL_STATIC) && event == EVENT_TRIGGER)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->state & STATE_PROCESSING)	// already done
		return 0;
	  instance->state |= STATE_PROCESSING;

	  int iid = get_original_internal_card_id(trigger_cause_controller, trigger_cause);
	  if (iid == -1)
		return 0;

	  if (!is_what(-1, iid, TYPE_CREATURE))
		reveal_card_iid(player, card, iid);	// otherwise revealed by the dialog
	  else
		{
		  int priority = IS_AI(trigger_cause_controller) && ((life[trigger_cause_controller] > 5 && has_mana_to_cast_iid(trigger_cause_controller, EVENT_TRIGGER, iid)) || life[trigger_cause_controller] > 12) ? 3 : 0;
		  int choice = DIALOG(player, card, EVENT_ACTIVATE,
							  DLG_WHO_CHOOSES(trigger_cause_controller), DLG_SMALLCARD_ID(iid),
							  "Pay 3 life", can_pay_life(trigger_cause_controller, 3), priority,
							  "Discard", 1, 1);
		  if (choice == 1)
			lose_life(trigger_cause_controller, 3);
		  else
			new_discard_card(trigger_cause_controller, trigger_cause, player, 0);
		}
	}

  return global_enchantment(player, card, event);
}

int card_breezekeeper(int player, int card, event_t event){
/* CARD_ID_BREEZEKEEPER	2677
Breezekeeper	|3|U
Creature - Djinn 4/4
Flying
Phasing (This phases in or out before you untap during each of your untap steps. While it's phased out, it's treated as though it doesn't exist.) */

	return phasing(player, card, event);
}

int card_bull_elephant(int player, int card, event_t event){
/* CARD_ID_BULL_ELEPHANT	2679
Bull Elephant	|3|G
Creature - Elephant 4/4
When ~ enters the battlefield, sacrifice it unless you return two |H1Forests you control to their owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int killme = 0;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = td.preferred_controller = player;
		td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( target_available(player, card, &td) >= 2 ){
			char msg[100];
			scnprintf(msg, 100, get_hacked_land_text(player, card, "Select a %s you control.", SUBTYPE_FOREST));
			int i;
			for(i=0; i<2; i++){
				if( new_pick_target(&td, msg, i, GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
				}
				else{
					break;
				}
			}
			for(i=0; i<instance->number_of_targets; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
				if( instance->number_of_targets == 2 ){
					bounce_permanent(instance->targets[i].player, instance->targets[i].card);
				}
			}
			if( instance->number_of_targets == 2 ){
				killme = 0;
			}
		}
		if( killme ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_chronatog(int player, int card, event_t event){
/* CARD_ID_CHRONATOG	2680
Chronatog	|1|U
Creature - Atog 1/2
|0: ~ gets +3/+3 until end of turn. You skip your next turn. Activate this ability only once each turn. */

	if (event == EVENT_CHECK_ABILITIES){
		if (affect_me(player, card) && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL)){
			check_abilities_power += 3;
			check_abilities_toughness += 3;
		}
	}
	else if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 3, 3);
		skip_next_turn(instance->parent_controller, instance->parent_card, player);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST0, 0, NULL, NULL);
}

int card_city_of_solitude(int player, int card, event_t event)
{
  /* CARD_ID_CITY_OF_SOLITUDE	2681
   * City of Solitude	|2|G
   * Enchantment
   * Players can cast spells and activate abilities only during their own turns. */

  int turn_or_result;
  card_instance_t *instance;
  int other_player;
  test_definition_t test;
  turn_or_result = is_humiliated(player, card, AL_STATIC);
  if (turn_or_result == 0)
  {
    if (event != EVENT_MODIFY_COST_GLOBAL)
    {
      if (event == EVENT_RESOLVE_SPELL)
      {
        default_test_definition(&test, 0x47);
        new_manipulate_all(player, card, 1 - current_turn, &test,
                           ACT_DISABLE_ALL_ACTIVATED_ABILITIES);
        turn_or_result = current_turn;
        instance = get_card_instance(player, card);
        instance->targets[2].player = 1 - turn_or_result;
        leaves_play(player, card, EVENT_RESOLVE_SPELL, 0, 0);
      }
      else if (event == EVENT_BEGIN_TURN)
      {
        other_player = 1 - current_turn;
        instance = get_card_instance(player, card);
        turn_or_result = current_turn;
        if (other_player != instance->targets[2].player)
        {
          instance = get_card_instance(player, card);
          instance->targets[2].player = 1 - turn_or_result;
          default_test_definition(&test, 0x47);
          new_manipulate_all(player, card, current_turn, &test, ACT_ENABLE_ALL_ACTIVATED_ABILITIES);
          new_manipulate_all(player, card, 1 - current_turn, &test,
                             ACT_DISABLE_ALL_ACTIVATED_ABILITIES);
        }
        leaves_play(player, card, EVENT_BEGIN_TURN, 0, 0);
      }
      else
      {
        leaves_play(player, card, event, 0, 0);
        if (event == EVENT_LEAVES_PLAY_ABILITY)
        {
          default_test_definition(&test, 0x47);
          new_manipulate_all(player, card, 1 - current_turn, &test,
                             ACT_ENABLE_ALL_ACTIVATED_ABILITIES);
        }
      }
      return global_enchantment(player, card, event);
    }
    instance = in_play(player, card);
    if ((((instance != NULL) && (1 - current_turn == affected_card_controller)) &&
         (turn_or_result = is_what(1 - current_turn, affected_card, 1), turn_or_result == 0)) &&
        (turn_or_result = is_humiliated(player, card, AL_STATIC), turn_or_result == 0))
    {
      infinite_casting_cost();
      return 0;
    }
  }
  return 0;
}

int card_coercion(int player, int card, event_t event){
  /* Coercion |2|B
   * 	  Sorcery
   * 	  Target opponent reveals his or her hand. You choose a card from it. That player discards that card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, "TARGET_CREATURE", 1, NULL);
}

int card_coral_atoll(int player, int card, event_t event){
	/* Coral Atoll	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, sacrifice it unless you return an untapped |H2Island you control to its owner's hand.
	 * |T: Add |1|U to your mana pool. */

	return vis_karoo(player, card, event, COLOR_BLUE, SUBTYPE_ISLAND);
}

int card_corrosion(int player, int card, event_t event)
{
  /* Corrosion	|1|B|R
   * Enchantment
   * Cumulative upkeep |1
   * At the beginning of your upkeep, put a rust counter on each artifact target opponent controls. Then destroy each artifact with converted mana cost less
   * than or equal to the number of rust counters on it. Artifacts destroyed this way can't be regenerated.
   * When ~ leaves the battlefield, remove all rust counters from all permanents. */

  int p;
  int opponent_or_result;
  card_instance_t *instance;
  int cmc_or_result;
  int rust_count;
  int c;

  cumulative_upkeep(player, card, event, 1, 0, 0, 0, 0, 0);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    instance = in_play(player, card);
    if ((instance != NULL) &&
        (opponent_or_result = opponent_is_valid_target(player, card), opponent_or_result != 0))
    {
      opponent_or_result = 1 - player;
      if (0 < active_cards_count[opponent_or_result])
      {
        c = 0;
        do
        {
          instance = in_play(opponent_or_result, c);
          if ((instance != NULL) &&
              (cmc_or_result = is_what(opponent_or_result, c, 0x40), cmc_or_result != 0))
          {
            add_counters(player, card, opponent_or_result, c, COUNTER_RUST, 1);
          }
          c = c + 1;
        } while (c < active_cards_count[opponent_or_result]);
      }
      p = 0;
      do
      {
        c = 0;
        if (0 < active_cards_count[p])
        {
          do
          {
            instance = in_play(p, c);
            if (((instance != NULL) && (cmc_or_result = is_what(p, c, 0x40), cmc_or_result != 0)) &&
                (cmc_or_result = count_counters(p, c, COUNTER_RUST), 0 < cmc_or_result))
            {
              cmc_or_result = get_cmc(p, c);
              rust_count = count_counters(p, c, COUNTER_RUST);
              if (cmc_or_result <= rust_count)
              {
                kill_card(p, c, KILL_BURY);
              }
            }
            c = c + 1;
          } while (c < active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
    }
    leaves_play(player, card, EVENT_UPKEEP_TRIGGER_ABILITY, 1, 1);
  }
  else
  {
    leaves_play(player, card, event, 1, 1);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      for (p = 0; p < 2; ++p)
        for (c = 0; c < active_cards_count[p]; ++c)
        {
          instance = in_play(p, c);
          if (instance && is_what(p, c, 0x47))
            remove_all_counters(player, card, p, c, COUNTER_RUST);
        }
    }
  }
  return global_enchantment(player, card, event);
}

int card_creeping_mold(int player, int card, event_t event ){
  /* Creeping Mold |2|G|G
   * 	  Sorcery
   * 	  Destroy target artifact, enchantment, or land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT | TYPE_LAND);

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target artifact, enchantment, or land.", 1, NULL);
}

int card_crypt_rats(int player, int card, event_t event)
{
  /* Crypt Rats	|2|B
   * Creature - Rat 1/1
   * |X: ~ deals X damage to each creature and each player. Spend only |Sblack mana this way. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	new_damage_all(player, card, ANYBODY, get_card_instance(player, card)->info_slot, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);

  int rval = generic_activated_ability(player, card, event, 0, MANACOST_B(-1), 0, NULL, NULL);

  // Reuse Pestilence's AI
  if ((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_SHOULD_AI_PLAY)
	return pestilence_impl(player, card, event, 0, COLOR_BLACK);

  // Keep AI from activating with 0
  if (IS_AI(player))
	{
	  if (event == EVENT_CAN_ACTIVATE && !has_mana_for_activated_ability(player, card, MANACOST_B(1)))
		return 0;

	  if (event == EVENT_ACTIVATE && get_card_instance(player, card)->info_slot <= 0)
		ai_modifier -= 128;
	}

  return rval;
}

int card_desertion(int player, int card, event_t event){
/* CARD_ID_DESERTION	2688
Desertion	|3|U|U
Instant
Counter target spell. If an artifact or creature spell is countered this way,
put that card onto the battlefield under your control instead of into its owner's graveyard. */

	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_RESOLVE_SPELL){
		if (counterspell_validate(player, card, NULL, 0)){
			int iid = -1;
			if (is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE | TYPE_ARTIFACT)){
				iid = get_card_instance(instance->targets[0].player, instance->targets[0].card)->internal_card_id;
			}
			if (pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card)){
				if (iid != -1){
					obliterate_card(instance->targets[0].player, instance->targets[0].card);
					int card_added = add_card_to_hand(player, iid);
					put_into_play(player, card_added);
					if (instance->targets[0].player != player){
						if (instance->targets[0].player == AI){ add_state(player, card_added, STATE_OWNED_BY_OPPONENT); }
						else{ remove_state(player, card_added, STATE_OWNED_BY_OPPONENT); }
					}
				}else{ kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE); }
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_diamond_kaleidoscope(int player, int card, event_t event){
	/* Diamond Kaleidoscope	|4
	 * Artifact
	 * |3, |T: Put a 0/1 colorless Prism artifact creature token onto the battlefield.
	 * Sacrifice a Prism token: Add one mana of any color to your mana pool. */

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_PRISM;
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, count_subtype(player, TYPE_PERMANENT, SUBTYPE_PRISM));
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Prism to sacrifice.");
	test.subtype = SUBTYPE_PRISM;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_PRISM = 1,
		CHOICE_MANA
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_produce_mana(player, card) && new_can_sacrifice_as_cost(player, card, &test) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
	}
	if( event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL),
						can_produce_mana(player, card) && new_can_sacrifice_as_cost(player, card, &test) ? 1 : 0
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Create a Prism", abils[1] && ! paying_mana(), 5,
							"Sac prism & get mana", abils[2], 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_PRISM ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL);
		}
		if( choice == CHOICE_MANA ){
			if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				FORCE(produce_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1));
				tapped_for_mana_color = -2;
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PRISM ){
			generate_token_by_id(instance->parent_controller, instance->parent_card, CARD_ID_PRISM);
		}
	}

	return 0;
}

int card_prism2(int player, int card, event_t event){

	return 0;
}

int card_dormant_volcano(int player, int card, event_t event){
	/* Dormant Volcano	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, sacrifice it unless you return an untapped |H2Mountain you control to its owner's hand.
	 * |T: Add |1|R to your mana pool. */

	return vis_karoo(player, card, event, COLOR_RED, SUBTYPE_MOUNTAIN);
}

int card_dragons_mask(int player, int card, event_t event){
/* CARD_ID_DRAGON_MASK	2692
Dragon Mask	|3
Artifact
|3, |T: Target creature you control gets +2/+2 until end of turn. Return it to its owner's hand at the beginning of the next end step.
(Return it only if it's on the battlefield.) */

	// Used externally
	// Dragon Mask
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_t pump;
			default_pump_ability_definition(player, card, &pump, 2, 2, 0, 0);
			pump.paue_flags = PAUE_END_AT_EOT | PAUE_REMOVE_TARGET_AT_EOT;
			pump.eot_removal_method = ACT_BOUNCE;
			pump_ability(player, card, instance->targets[0].player, instance->targets[0].card, &pump);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0,
									&td, "Select target creature you control.");
}

int card_dragon_mask(int player, int card, event_t event)
{
  /* CARD_ID_DRAGON_MASK	2692
   * Dragon Mask	|3
   * Artifact
   * |3, |T: Target creature you control gets +2/+2 until end of turn. Return it to its owner's hand at the beginning of the next end step.
   * (Return it only if it's on the battlefield.) */

if (event == EVENT_CHECK_ABILITIES)
  {
    if (affected_card_controller == player &&
        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(3), 0, NULL, NULL))
    {
      check_abilities_power += 2;
      check_abilities_toughness += 2;
    }
  }
  else if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = td.allowed_controller = player;
    card_instance_t* instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    {
      pump_ability_t pump;
      default_pump_ability_definition(player, card, &pump, 2, 2, 0, 0);
      pump.paue_flags = PAUE_END_AT_EOT | PAUE_REMOVE_TARGET_AT_EOT;
      pump.eot_removal_method = 5; // Native kill-method value for returning a permanent to hand.
      pump_ability(player, card, instance->targets[0].player, instance->targets[0].card, &pump);
    }
    return generic_activated_ability(player, card, event, 7, MANACOST_X(3), 0, &td,
        "Select target creature you control it gets +2/+2 until end of turn.");
  }
  return 0;
}

int card_dream_tides(int player, int card, event_t event){

	/* Dream Tides	|2|U|U
	 * Enchantment
	 * Creatures don't untap during their controllers' untap steps.
	 * At the beginning of each player's upkeep, that player may choose any number of tapped non|Sgreen creatures he or she controls and pay |2 for each
	 * creature chosen this way. If the player does, untap those creatures. */

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			card_instance_t *instance= get_card_instance(affected_card_controller, affected_card);
			instance->untap_status &= ~3;
		}
	}

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int p = current_turn;
		int c1 = active_cards_count[p]-1;
		int green = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		while( c1 > -1 ){
				if( has_mana(player, COLOR_COLORLESS, 2) && in_play(p, c1) && is_what(p, c1, TYPE_CREATURE) && is_tapped(p, c1) &&
					!(get_color(p, c1) & green) ){
					int choice = do_dialog(p, player, card, p, c1, " Pay 2 to untap\n Pass", 0);
					if( choice == 0 && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, p, COLOR_COLORLESS, 2) ){
						untap_card(p, c1);
					}
				}
				c1--;
		}
	}

	return global_enchantment(player, card, event);
}

int card_elephant_grass(int player, int card, event_t event){
/* CARD_ID_ELEPHANT_GRASS	2695
Elephant Grass	|G
Enchantment
Cumulative upkeep |1 (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.)
|SBlack creatures can't attack you.
Non|Sblack creatures can't attack you unless their controller pays |2 for each creature he or she controls that's attacking you. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ATTACK_LEGALITY && affected_card_controller != player ){
			int color = get_color(affected_card_controller, affected_card);
			if( color & get_sleighted_color_test(player, card, COLOR_TEST_BLACK) ){
				event_result = 1;
			}
		}
		tax_attack(player, card, event);
		cumulative_upkeep(player, card, event, MANACOST_X(1));
	}
	return global_enchantment(player, card, event);
}

int card_emerald_charm(int player, int card, event_t event)
{
  /* Emerald Charm	|G
   * Instant
   * Choose one - Untap target permanent; or destroy target non-Aura enchantment; or target creature loses flying until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td_permanent;
	default_target_definition(player, card, &td_permanent, TYPE_PERMANENT);
	td_permanent.preferred_controller = player;

	target_definition_t td_nonaura;
	default_target_definition(player, card, &td_nonaura, TYPE_ENCHANTMENT);
	td_nonaura.required_subtype = SUBTYPE_AURA;
	td_nonaura.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);

	card_instance_t* instance = get_card_instance(player, card);

	enum
	{
		CHOICE_UNTAP = 1,
		CHOICE_NON_AURA,
		CHOICE_FLYING
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td_permanent, NULL, 1, NULL) )
			return 1;
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td_nonaura, NULL, 1, NULL) )
			return 1;
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, NULL, 1, NULL) )
			return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td_permanent, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td_nonaura, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td_creature, NULL, 1, NULL)
			};
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Untap permanent", abils[1], 5,
								"Destroy non-Aura", abils[2], current_phase >= PHASE_AFTER_BLOCKING ? 10 : 1,
								"Lose flying", abils[3], current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_UNTAP ){
			generic_spell(player, card, event, GS_CAN_TARGET, &td_permanent, "TARGET_PERMANENT", 1, NULL);
			if( spell_fizzled != 1 ){
				ai_mod_for_untap_effects(player, card, 0, 1);
			}
		}
		if( instance->info_slot == CHOICE_NON_AURA ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td_nonaura, "Select target non-Aura enchantment.",
								1, NULL);
		}
		if( instance->info_slot == CHOICE_FLYING ){
			generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, "TARGET_CREATURE", 1, NULL);
			if( spell_fizzled != 1 ){
				ai_modifier += (check_for_ability(instance->targets[0].player, instance->targets[0].card, KEYWORD_FLYING) ? 48 : -48);
				ai_modifier += (instance->targets[0].player == 1-player ? 48 : -48);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;
		switch (choice)
		{
			case CHOICE_UNTAP:
			{
				if( valid_target(&td_permanent) ){
					untap_card(instance->targets[0].player, instance->targets[0].card);
				}
				break;
			}

			case CHOICE_NON_AURA:
			{
				if( valid_target(&td_nonaura) ){
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}
				break;
			}

			case CHOICE_FLYING:
			{
				if( valid_target(&td_creature) ){
					negate_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, KEYWORD_FLYING);
				}
				break;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_equipoise(int player, int card, event_t event){
  /* Equipoise |2|W
   * Enchantment
   * At the beginning of your upkeep, for each land target player controls in excess of the number you control, choose a land he or she controls,
   * 	then the chosen permanents phase out. Repeat this process for artifacts and creatures. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);

		int types[3];
		types[0] = TYPE_LAND;
		types[1] = TYPE_ARTIFACT;
		types[2] = TYPE_CREATURE;

		int k;
		for(k=0; k < 3; k++){
			target_definition_t td;
			default_target_definition(player, card, &td, types[k]);
			td.allowed_controller = td.preferred_controller = 1-player;
			td.allow_cancel = 0;

			int diff = count_permanents_by_type(1-player, types[k]) - count_permanents_by_type(player, types[k]);
			if( diff > 0 ){
				int z;
				for(z=0; z < diff; z++){
					if( k == 0 && can_target(&td) ){
						pick_target(&td, "TARGET_LAND");
						instance->number_of_targets = 0;
					}
					else if( k == 1 && can_target(&td) ){
							pick_target(&td, "TARGET_ARTIFACT");
							instance->number_of_targets = 0;
					}
					else if( k == 2 && can_target(&td) ){
							pick_target(&td, "TARGET_CREATURE");
							instance->number_of_targets = 0;
					}
					phase_out(instance->targets[0].player, instance->targets[0].card);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_everglades(int player, int card, event_t event){
	/* Everglades	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, sacrifice it unless you return an untapped |H2Swamp you control to its owner's hand.
	 * |T: Add |1|B to your mana pool. */

	return vis_karoo(player, card, event, COLOR_BLACK, SUBTYPE_SWAMP);
}

static void eos_effect(int id){
	//"destroy each permanent with the same name as another permanent, except for basic lands. They can't be regenerated."

	if( id == -1 ){ //No effect if the card has no name.
		return;
	}

	int i;
	int sum = 0;
	int f_player = -1;
	int f_card = -1;
	for(i=0; i<2; i++){
		int count = active_cards_count[i]-1;
		while( count > -1 ){
				if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && get_id(i, count) == id ){
					sum++;
					if( sum == 1 ){
						f_player = i;
						f_card = count;
					}
					if( sum > 1 ){
						kill_card(i, count, KILL_BURY);
					}
				}
				count--;
		}
	}
	if( sum > 1 ){
		kill_card(f_player, f_card, KILL_BURY);
	}
}

int card_eye_of_singularity(int player, int card, event_t event){
  /* Eye of Singularity |3|W
   * 	  World Enchantment
   * 	  When Eye of Singularity enters the battlefield, destroy each permanent with the same name as another permanent, except for basic lands.
   * 	  They can't be regenerated.
   * 	  Whenever a permanent other than a basic land enters the battlefield, destroy all other permanents with that name. They can't be regenerated. */

	enchant_world(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if( event == EVENT_ETB_ABILITY ){
		int i;
		for(i=0; i<2; i++){
			int count = active_cards_count[i]-1;
			while( count > -1 ){
					if( in_play(i, count) && is_what(i, count, TYPE_PERMANENT) && ! is_basic_land(i, count)  ){
						eos_effect(get_card_name(i, count));
					}
					count--;
			}
		}
	}

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_BASIC;
		test.subtype_flag = DOESNT_MATCH;
		test.not_me = 1;

		etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		int id = get_card_name(trigger_cause_controller, trigger_cause);
		if( id != -1 ){ //No effect if the permanent entered the battlefield has no name.
			state_untargettable(trigger_cause_controller, trigger_cause, 1);
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.id = id;
			new_manipulate_all(player, card, ANYBODY, &this_test, ACT_KILL_BURY);
			state_untargettable(trigger_cause_controller, trigger_cause, 0);
		}
	}

	return global_enchantment(player, card, event);
}

int card_fallen_askari(int player, int card, event_t event){
/* CARD_ID_FALLEN_ASKARI	2702
Fallen Askari	|1|B
Creature - Human Knight 2/2
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
~ can't block. */

	flanking(player, card, event);

	cannot_block(player, card, event);

	return 0;
}

static int femeref_enchantress_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_FEMEREF_ENCHANTRESS)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }

  if (get_card_instance(player, card)->damage_target_player > -1 &&
      effect_follows_control_of_attachment(player, card, event))
  {
    return 0;
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(affected_card_controller, affected_card))
    {
      int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
      if (kill_code)
      {
        if (instance->damage_target_player > -1)
        {
          int p = instance->damage_target_player;
          int c = instance->damage_target_card;
          if (affect_me(p, c))
          {
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
            {
              instance->targets[11].player = 0;
            }
            SET_BYTE1(instance->targets[11].player) = 1;
          }
        }
        count_for_gfp_ability(player, card, event, ANYBODY, TYPE_ENCHANTMENT, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    draw_cards(player, BYTE0(result));
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_femeref_enchantress(int player, int card, event_t event){
/* CARD_ID_FEMEREF_ENCHANTRESS	2703
Femeref Enchantress	|G|W
Creature - Human Druid 1/2
Whenever an enchantment is put into a graveyard from the battlefield, draw a card. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)femeref_enchantress_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &femeref_enchantress_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, ANYBODY, TYPE_ENCHANTMENT, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			draw_cards(player, amount);
		}
	}

	return 0;
}

int card_fireblast(int player, int card, event_t event){
	/* Fireblast	|4|R|R	0x2001317
	 * Instant
	 * You may sacrifice two |H1Mountains rather than pay this spell's mana cost.
	 * ~ deals 4 damage to any target. */

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = SUBTYPE_MOUNTAIN;
		test.qty = 2;
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			get_card_instance(player, card)->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			get_card_instance(player, card)->info_slot = 0;
		}
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
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 1;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
								"Sacrifice two mountains", 1, count_subtype(player, TYPE_LAND, -1) * 2,
								"Cast normally", 1, 5);
			}

			if( choice == 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a Mountain to sacrifice.");
			    test.subtype = get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN);	// not hacked; it's a cost, so there's no opportunity to cast Magical Hack

				int ts = 0;
				int sacced[2];
				while( ts < 2 ){
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
					if( ts == 2 ){
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
	    pick_target(&td, "TARGET_ANY");
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_firestorm_hellkite(int player, int card, event_t event){
/* CARD_ID_FIRESTORM_HELLKITE	2706
Firestorm Hellkite	|4|U|R
Creature - Dragon 6/6
Flying, trample
Cumulative upkeep |U|R (At the beginning of your upkeep, put an age counter on this permanent,
then sacrifice it unless you pay its upkeep cost for each age counter on it.) */

	cumulative_upkeep(player, card, event, MANACOST_UR(1, 1));

	return 0;
}

int card_flooded_shoreline(int player, int card, event_t event){
/* CARD_ID_FLOODED_SHORELINE	2707
Flooded Shoreline	|U|U
Enchantment
|U|U, Return two |H1Islands you control to their owner's hand: Return target creature to its owner's hand. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(2), 0, &td2, NULL) ){
			return target_available(player, card, &td) >= 2 ? 1 : 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		char msg[100];
		scnprintf(msg, 100, "%s", get_hacked_land_text(player, card, "Select an %s you control.", SUBTYPE_ISLAND));
		int to_bounce[2];
		int i;
		for(i=0; i<2; i++){
			if( new_pick_target(&td, msg, i, GS_LITERAL_PROMPT) ){
				to_bounce[i] = instance->targets[i].card;
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
			}
			else{
				break;
			}
		}
		for(i=0; i<instance->number_of_targets; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
		}
		if( instance->number_of_targets < 2 ){
			spell_fizzled = 1;
			return 0;
		}
		instance->number_of_targets = 0;
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_U(2), 0, &td2, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			for(i=0; i<2; i++){
				bounce_permanent(player, to_bounce[i]);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td2)  ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_forbidden_ritual(int player, int card, event_t event){
  /* Forbidden Ritual English |2|B|B
   * 	  Sorcery
   * 	  Sacrifice a nontoken permanent. If you do, target opponent loses 2 life unless he or she sacrifices a permanent or discards a card.
   * 	  You may repeat this process any number of times. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.allowed_controller = 1-player;
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td)  ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_PERMANENT);
			this_test.type_flag = F1_NO_TOKEN;
			int max = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
			while( max ){
					if( sacrifice(player, card, player, 0, TYPE_PERMANENT, F1_NO_TOKEN, 0, 0, 0, 0, 0, 0, -1, 0) ){
						int ai_choice = 0;
						if( life[1-player]-2 < 6 ){
							if( hand_count[1-player] ){
								ai_choice = 1;
							}
							else if( can_sacrifice(player, 1-player, 1, TYPE_PERMANENT, 0) ){
									ai_choice = 2;
							}
						}
						int choice = do_dialog(1-player, player, card, -1, -1, " Lose 2 life\n Discard a card\n Sac a permanent", ai_choice);
						if( choice == 0 ){
							lose_life(1-player, 2);
						}
						if( choice == 1 ){
							discard(1-player, 0, player);
						}
						if( choice == 2 ){
							impose_sacrifice(player, card, 1-player, 1, TYPE_PERMANENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
						}
						max--;
					}
					else{
						break;
					}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_funeral_charm(int player, int card, event_t event){
  /* Funeral Charm |B
   * Instant
   * Choose one �
   * � Target player discards a card.
   * � Target creature gets +2/-1 until end of turn.
   * � Target creature gains swampwalk until end of turn. (It can't be blocked as long as defending player controls a Swamp.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.preferred_controller = ANYBODY;

	target_definition_t td3;
	default_target_definition(player, card, &td3, TYPE_CREATURE);
	td3.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum
	{
		CHOICE_DISCARD = 1,
		CHOICE_P2_M1,
		CHOICE_SWAMPWALK
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) )
			return 1;
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td2, NULL, 1, NULL) )
			return 1;
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td3, NULL, 1, NULL) )
			return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td2, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td3, NULL, 1, NULL)
			};
			int priority_walk = check_battlefield_for_subtype(1-player, TYPE_LAND, SUBTYPE_SWAMP) ? 10 : 1;
			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"Target player discard", abils[1], 5,
								"Give +2/-1", abils[2], current_phase >= PHASE_AFTER_BLOCKING ? 10 : 1,
								"Give Swampwalk", abils[3], current_phase < PHASE_DECLARE_BLOCKERS ? priority_walk : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_DISCARD ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
		if( instance->info_slot == CHOICE_P2_M1 ){
			generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td2, "Select target creature to give +2/-1.", 1, NULL);
			if( spell_fizzled != 1 ){
				if( instance->targets[0].player == player ){
					ai_modifier += (get_toughness(instance->targets[0].player, instance->targets[0].card) > 1 ? 48 : -48);
				}
				if( instance->targets[0].player == 1-player ){
					ai_modifier += (get_toughness(instance->targets[0].player, instance->targets[0].card) == 1 ? 48 : -48);
				}
			}
		}
		if( instance->info_slot == CHOICE_SWAMPWALK ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td3, "Select target creature to give swampwalk", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int choice = instance->info_slot;
		switch (choice)
		{
			case CHOICE_DISCARD:
			{
				if( valid_target(&td) ){
					discard(instance->targets[0].player, 0, player);
				}
				break;
			}

			case CHOICE_P2_M1:
			{
				if( valid_target(&td2) ){
					return vanilla_instant_pump(player, card, event, ANYBODY, ANYBODY, 2, -1, 0, 0);
				}
				break;
			}

			case CHOICE_SWAMPWALK:
			{
				if( valid_target(&td3) ){
					int key = get_hacked_walk(player, card, KEYWORD_SWAMPWALK);
					pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, key, 0);
				}
				break;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_goblin_recruiter( int player, int card, event_t event){
/* CARD_ID_GOBLIN_RECRUITER	2712
Goblin Recruiter	|1|R
Creature - Goblin 1/1
When ~ enters the battlefield, search your library for any number of Goblin cards and reveal those cards.
Shuffle your library, then put them on top of it in any order. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, 0, "Select a Goblin card.");
		this_test.subtype = SUBTYPE_GOBLIN;
		this_test.no_shuffle = 1;

		int tutored[count_deck(player)];
		int t_index = 0;

		while( 1 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
				if( selected != -1 ){
					tutored[t_index] = deck_ptr[player][selected];
					remove_card_from_deck(player, selected);
					t_index++;
				}
				else{
					break;
				}
		}
		shuffle(player);
		if( t_index ){
			int i;
			for(i=0; i<=t_index; i++){
				raw_put_iid_on_top_of_deck(player, tutored[i]);
			}
			if( t_index > 1 ){
				rearrange_top_x(player, player, t_index);
			}
		}
	}
	return 0;
}

int card_gossamer_chains(int player, int card, event_t event){
/* CARD_ID_GOSSAMER_CHAINS	2714
Gossamer Chains	|W|W
Enchantment
Return ~ to its owner's hand: Prevent all combat damage that would be dealt by target unblocked creature this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	td.illegal_state = TARGET_STATE_ISBLOCKED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td)  ){
			negate_combat_damage_this_turn(player, card, instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_BOUNCE_ME | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target unblocked creature.");
}

int card_griffin_canyon(int player, int card, event_t event){
/* CARD_ID_GRIFFIN_CANYON	2715
Griffin Canyon	""
Land
|T: Add |C to your mana pool.
|T: Untap target Griffin. If it's a creature, it gets +1/+1 until end of turn. */
	if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player && has_subtype(player, affected_card, SUBTYPE_GRIFFIN) &&
		generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) && !paying_mana()){
		++check_abilities_power;
		++check_abilities_toughness;
	}

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_GRIFFIN;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_PUMP
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td, NULL) )
		{
			choice = CHOICE_PUMP;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_PUMP && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"+1/+1 & untap target Griffin", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA ){
			return mana_producer(player, card, event);
		}
		if( choice == CHOICE_PUMP ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_LITERAL_PROMPT, MANACOST0, 0,
										&td, "Select target Griffin.");
			if( spell_fizzled != 1 ){
				ai_modifier += (is_tapped(instance->targets[0].player, instance->targets[0].card) ? 24 : 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_PUMP ){
			if( valid_target(&td)  ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
				if( is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
					pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1, 1);
				}
			}
		}
	}

	return 0;
}

int card_guiding_spirit(int player, int card, event_t event){
/* CARD_ID_GUIDING_SPIRIT	2716
Guiding Spirit	|1|W|U
Creature - Angel Spirit 1/2
Flying
|T: If the top card of target player's graveyard is a creature card, put that card on top of that player's library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int iid = get_grave(player)[count_graveyard(player)-1];
		if( iid == -1 || (iid > -1 && ! is_what(-1, iid, TYPE_CREATURE)) ){
			ai_modifier -= 96;
		}
		else{
			ai_modifier += (cards_ptr[cards_data[iid].id]->ai_base_value);
			if( current_turn != player && current_phase >= PHASE_AFTER_BLOCKING ){
				ai_modifier += 24;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			const int *grave = get_grave(instance->targets[0].player);
			if( is_what(-1, grave[count_graveyard(instance->targets[0].player)-1], TYPE_CREATURE) ){
				int card_added = add_card_to_hand(instance->targets[0].player, grave[count_graveyard(instance->targets[0].player)-1]);
				remove_card_from_grave(instance->targets[0].player, count_graveyard(instance->targets[0].player)-1);
				put_on_top_of_deck(instance->targets[0].player, card_added);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_heart_charm(int player, int card, event_t event){
  /* Hearth Charm |R
   * 	  Instant
   * 	  Choose one -
   * Destroy target artifact creature.
   * Attacking creatures get +1/+0 until end of turn.
   * Target creature with power 2 or less can't be blocked this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	enum{
		CHOICE_KILL_AC = 1,
		CHOICE_PUMP,
		CHOICE_UNBLOCKABLE
	};

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_ARTIFACT_CREATURE;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.power_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			instance->info_slot = 0;
			int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_RANDOM,
								"Kill an artifact creature", can_target(&td), 10,
								"Pump attacking creatures", 1, current_turn == player && current_phase == PHASE_AFTER_BLOCKING ? 5 : 0,
								"Give unblockable", can_target(&td1), current_turn == player && current_phase == PHASE_MAIN1 ? 8 : 0);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			instance->info_slot = choice;
		}
		instance->number_of_targets = 0;

		if( instance->info_slot == CHOICE_KILL_AC ){
			new_pick_target(&td, "Select target artifact creature.", 0, 1 | GS_LITERAL_PROMPT);
		}
		if( instance->info_slot == CHOICE_UNBLOCKABLE ){
			new_pick_target(&td1, "Select target creature with power 2 or less.", 0, 1 | GS_LITERAL_PROMPT);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->info_slot == CHOICE_KILL_AC ){
			if( valid_target(&td) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
		if( instance->info_slot == CHOICE_PUMP ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.state = STATE_ATTACKING;
			pump_creatures_until_eot(player, card, current_turn, 0, 1, 0, 0, 0, &this_test);
		}
		if( instance->info_slot == CHOICE_UNBLOCKABLE && valid_target(&td1) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_helm_of_awakening(int player, int card, event_t event)
{
/* CARD_ID_HELM_OF_AWAKENING	2719
Helm of Awakening	|2
Artifact
Spells cost |1 less to cast. */

	if(event == EVENT_MODIFY_COST_GLOBAL && ! is_humiliated(player, card, AL_STATIC) ){
		card_data_t* card_d = get_card_data(affected_card_controller, affected_card);
		if(! (card_d->type & TYPE_LAND) ){
			COST_COLORLESS--;
		}
	}

	return 0;
}

int card_ivory_charm(int player, int card, event_t event)
{
	/* Ivory Charm	|W	0x2009f1c
	 * Instant
	 * Choose one -
	 * * All creatures get -2/-0 until end of turn.
	 * * Tap target creature.
	 * * Prevent the next 1 damage that would be dealt to any target this turn. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);

	target_definition_t td_damage;
	default_target_definition(player, card, &td_damage, 0);
	td_damage.extra = damage_card;

	card_instance_t* instance = get_card_instance(player, card);

	enum
	{
		CHOICE_M2_M0 = 1,
		CHOICE_TAP,
		CHOICE_PREVENT
	};

	if( event == EVENT_CAN_CAST ){
		if( generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td_damage, NULL, 1, NULL) ){
			return 99;
		}
		if( generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, NULL, 1, NULL) ){
			return 1;
		}
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			int abils[4] = {
							0,
							1,
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET, &td_creature, NULL, 1, NULL),
							generic_spell(player, card, EVENT_CAN_CAST, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td_damage, NULL, 1, NULL)
			};

			int choice = DIALOG(player, card, event, DLG_RANDOM,
								"-2/-0", abils[1], 5,
								"Tap creature", abils[2], current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS ? 10 : 1,
								"Prevent damage", abils[3], 15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( instance->info_slot == CHOICE_M2_M0 ){
			return basic_spell(player, card, event);
		}
		if( instance->info_slot == CHOICE_TAP ){
			generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, "TARGET_CREATURE", 1, NULL);
			if( spell_fizzled != 1 ){
				ai_modifier += (is_tapped(instance->targets[0].player, instance->targets[0].card) ? -48 : 0);
			}
		}
		if( instance->info_slot == CHOICE_PREVENT ){
			return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td_damage, "TARGET_DAMAGE", 1, NULL);
		}
	}

	if( event == EVENT_RESOLVE_SPELL)
	{
		int choice = instance->info_slot;
		switch (choice)
		{
			case CHOICE_M2_M0:
				pump_creatures_until_eot(player, card, ANYBODY, 0, -2,0, 0,0, NULL);
				break;

			case CHOICE_TAP:
			{
				if( valid_target(&td_creature) ){
					tap_card(instance->targets[0].player, instance->targets[0].card);
				}
				break;
			}

			case CHOICE_PREVENT:
			{
				if( valid_target(&td_damage) ){
					prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
				}
				break;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_iron_heart_chimera(int player, int card, event_t event){
	/* Iron-Heart Chimera	|4
	 * Artifact Creature - Chimera 2/2
	 * Vigilance
	 * Sacrifice ~: Put a +2/+2 counter on target Chimera creature. It gains vigilance. */
	vigilance(player, card, event);
	return chimera(player, card, event, 0, SP_KEYWORD_VIGILANCE);
}

int card_impulse(int player, int card, event_t event){
	/* Impulse	|1|U
	 * Instant
	 * Look at the top four cards of your library. Put one of them into your hand and the rest on the bottom of your library in any order. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 48 : 24);
	}

	if(event == EVENT_RESOLVE_SPELL){
		impulse_effect(player, 4, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_inspiration(int player, int card, event_t event){
  /* Inspiration |3|U
   * 	  Instant
   * 	  Target player draws two cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_targeted_card_drawing(player, card, 0, 2);
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			draw_cards(get_card_instance(player, card)->targets[0].player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_jamuraa_lion(int player, int card, event_t event){
/* CARD_ID_JAMURAAN_LION	2726
Jamuraan Lion	|2|W
Creature - Cat 3/1
|W, |T: Target creature can't block this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_W(1), 0, &td, "TARGET_CREATURE");
}

int card_juju_bubble(int player, int card, event_t event){
/* CARD_ID_JUJU_BUBBLE	2727
Juju Bubble	|1
Artifact
Cumulative upkeep |1 (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.)
When you play a card, sacrifice ~.
|2: You gain 1 life. */

	cumulative_upkeep(player, card, event, MANACOST_X(1));

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && player == reason_for_trigger_controller &&
		!is_humiliated(player, card, AL_TRIG) && trigger_cause_controller == player && ! is_token(trigger_cause_controller, trigger_cause) )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
}

int card_jungle_basin(int player, int card, event_t event){
	/* Jungle Basin	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, sacrifice it unless you return an untapped |H2Forest you control to its owner's hand.
	 * |T: Add |1|G to your mana pool. */

	return vis_karoo(player, card, event, COLOR_GREEN, SUBTYPE_FOREST);
}

int card_kaerveks_spite(int player, int card, event_t event){
/* CARD_ID_KAERVEKS_SPITE	2729
Kaervek's Spite	|B|B|B
Instant
As an additional cost to cast ~, sacrifice all permanents you control and discard your hand.
Target player loses 5 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			if( life[instance->targets[0].player] > 5 ){
				ai_modifier -= 256;
			}
			else{
				ai_modifier += 96;
			}
			test_definition_t test;
			default_test_definition(&test, TYPE_PERMANENT);
			new_manipulate_all(player, card, player, &test, KILL_SACRIFICE);
			discard_all(player);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

   return 0;
}

int card_karoo(int player, int card, event_t event){
	/* Karoo	""
	 * Land
	 * ~ enters the battlefield tapped.
	 * When ~ enters the battlefield, sacrifice it unless you return an untapped |H2Plains you control to its owner's hand.
	 * |T: Add |1|W to your mana pool. */

	return vis_karoo(player, card, event, COLOR_WHITE, SUBTYPE_PLAINS);
}

static int katabatic_winds_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->targets[0].player > -1 ){
		if( event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			if( check_state(instance->targets[0].player, instance->targets[0].card, STATE_OUBLIETTED) ){
				remove_special_flags2(affected_card_controller, affected_card, SF2_KATABATIC_WINDS);
			}
		}
	}
	return 0;
}

int card_katabatic_winds(int player, int card, event_t event)
{
  /* CARD_ID_KATABATIC_WINDS	2731
   * Katabatic Winds	|2|G
   * Enchantment
   * Phasing (This phases in or out before you untap during each of your untap steps. While it's phased out, it's treated as though it doesn't exist.)
   * Creatures with flying can't attack or block, and their activated abilities with |T in their costs can't be activated. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = create_legacy_effect(player, card, katabatic_winds_legacy);
    instance = get_card_instance(player, result);
    instance->targets[0].player = player;
    instance->targets[0].card = card;
    instance->number_of_targets = '\x01';
    add_status(player, result, 0x10000);
    is_humiliated(player, card, AL_STATIC);
  }
  else
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      if (event == EVENT_ABILITIES)
      {
        result = is_what(affected_card_controller, affected_card, 2);
        if (result != 0)
        {
          result = check_for_ability(affected_card_controller, affected_card, 0x20);
          if (result == 0)
          {
            remove_special_flags2(affected_card_controller, affected_card, 0x10);
          }
          else
          {
            set_special_flags2(affected_card_controller, affected_card, 0x10);
          }
        }
      }
      else
      {
        if (event != EVENT_ATTACK_LEGALITY)
        {
          if (event != EVENT_BLOCK_LEGALITY)
          {
            if (event == EVENT_PHASING)
            {
              return phasing(player, card, EVENT_PHASING);
            }
            return global_enchantment(player, card, event);
          }
          if (attacking_card_controller != current_turn)
            return global_enchantment(player, card, event);
        }
        result = check_for_ability(affected_card_controller, affected_card, 0x20);
        if (result != 0)
        {
          event_result = 1;
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

int card_keeper_of_kookus(int player, int card, event_t event){
/* CARD_ID_KEEPER_OF_KOOKUS	2732
Keeper of Kookus	|R
Creature - Goblin 1/1
|R: ~ gains protection from |Sred until end of turn. */

	return generic_shade(player, card, event, 0, 0, 0, 0, 0, 1, 0, 0, 0, KEYWORD_PROT_RED, 0);
}

int card_knight_of_the_mists(int player, int card, event_t event){
  /* Knight of the Mists |2|U
   * Creature — Human Knight 2/2
   * Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
   * When Knight of the Mists enters the battlefield, you may pay {U}. If you don't, destroy target Knight and it can't be regenerated. */

	flanking(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td_any_knight;
		default_target_definition(player, card, &td_any_knight, TYPE_PERMANENT);
		td_any_knight.required_subtype = SUBTYPE_KNIGHT;
		td_any_knight.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int kill_knight = 1;
		if( has_mana(player, COLOR_BLUE, 1) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_BLUE, 1) ){
			kill_knight = 0;
		}
			
		spell_fizzled = 0; //Just in case
		if( kill_knight && can_target(&td_any_knight) ){
			if( new_pick_target(&td_any_knight, "Select target Knight.", 0, GS_LITERAL_PROMPT) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			}
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		target_definition_t td_opp_knight;
		default_target_definition(player, card, &td_opp_knight, TYPE_PERMANENT);
		td_opp_knight.required_subtype = SUBTYPE_KNIGHT;
		td_opp_knight.allow_cancel = 0;
		td_opp_knight.allowed_controller = 1 - player;

		if( ! can_target(&td_opp_knight) && ! has_mana(player, COLOR_BLUE, 1) ){
			ai_modifier -= 96;
		}
	}

	return 0;
}

static void creatures_without_flanking_get_minus1minus1(int player, int card, int blocking_player, int blocking_card)
{
  if (is_what(blocking_player, blocking_card, TYPE_CREATURE)
	  && !check_for_special_ability(blocking_player, blocking_card, SP_KEYWORD_FLANKING))
	pump_until_eot(player, card, blocking_player, blocking_card, -1, -1);
}

int card_knight_of_valor(int player, int card, event_t event)
{
/* CARD_ID_KNIGHT_OF_VALOR	2734
Knight of Valor	|2|W
Creature - Human Knight 2/2
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
|1|W: Each creature without flanking blocking ~ gets -1/-1 until end of turn. Activate this ability only once each turn. */

  flanking(player, card, event);

  if (event == EVENT_CAN_ACTIVATE && current_phase != PHASE_AFTER_BLOCKING
	  && (player == AI || ai_is_speculating == 1))
	return 0;	// otherwise fall through to generic_activated_ability()

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if ((instance->state & (STATE_ATTACKING | STATE_ISBLOCKED)) != (STATE_ATTACKING | STATE_ISBLOCKED))
		return 0;

	  for_each_creature_blocking_me(instance->parent_controller, instance->parent_card, creatures_without_flanking_get_minus1minus1, player, card);
	}

  return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_XW(1,1), 0, NULL, NULL);
}

int card_kookus(int player, int card, event_t event){
/* CARD_ID_KOOKUS	2736
Kookus	|3|R|R
Creature - Djinn 3/5
Trample
At the beginning of your upkeep, if you don't control a creature named Keeper of ~, ~ deals 3 damage to you and attacks this turn if able.
|R: ~ gets +1/+0 until end of turn. */

	card_instance_t *instance = get_card_instance(player, card);

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( check_battlefield_for_id(player, CARD_ID_KEEPER_OF_KOOKUS) ){
			instance->info_slot = 0;
		}
		else{
			damage_player(player, 3, player, card);
			get_card_instance(player, card)->info_slot = 66;
		}
	}

	if( get_card_instance(player, card)->info_slot == 66 && ! is_humiliated(player, card, AL_TRIG) ){
		attack_if_able(player, card, event);
	}

	return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
}

int card_lead_belly_chimera(int player, int card, event_t event){
	/* Lead-Belly Chimera	|4
	 * Artifact Creature - Chimera 2/2
	 * Trample
	 * Sacrifice ~: Put a +2/+2 counter on target Chimera creature. It gains trample. */
	return chimera(player, card, event, KEYWORD_TRAMPLE, 0);
}

int card_lightning_cloud(int player, int card, event_t event){
	/* Lightning Cloud	|3|R	0x20031f8
	 * Enchantment
	 * Whenever a player casts a |Sred spell, you may pay |R. If you do, ~ deals 1 damage to any target. */
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		if (!has_mana(player, COLOR_RED, 1)){ return 0; }
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && has_mana(player, COLOR_RED, 1) &&
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_RED, 1) &&
			pick_target(&td, "TARGET_ANY") )
		{
			damage_target0(player, card, 1);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_magma_mine(int player, int card, event_t event)
{
  /* Magma Mine	|1	0x2009f3f
   * Artifact
   * |4: Put a pressure counter on ~.
   * |T, Sacrifice ~: It deals damage equal to the number of pressure counters on it to any target. */

  int choice;
  card_instance_t *instance;
  int available_or_mode;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 4, 0, 0, 0,
                                                    0, 0, 0, NULL, NULL);
      if ((available_or_mode != 0) && (player != 1))
      {
        return 1;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 0, 0, 0, 0, 0, 0, 0,
                                       &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      instance->info_slot = 0;
      available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 4, 0, 0, 0,
                                                    0, 0, 0, NULL, NULL);
      if ((available_or_mode == 0) || (player == 1))
      {
        instance->info_slot = 0x43;
        if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) == 0)
        {
          return 0;
        }
        if (pick_target(&td, "TARGET_ANY") == 0)
        {
          return 0;
        }
        available_or_mode = count_counters(player, card, COUNTER_PRESSURE);
        instance->targets[1].player = available_or_mode;
        add_state(player, card, 0x10);
        tapped_for_mana_color = -1;
        kill_card(player, card, KILL_SACRIFICE);
        return 0;
      }
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 0, 0, 0, 0, 0, 0, 0, &td,
                                    NULL) == 0)
      {
        instance->info_slot = 0x42;
      }
      else
      {
        choice = do_dialog(player, player, card, -1, -1,
                           " Add a pressure counter\n Explode\n Cancel", 0);
        if (choice == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        instance->info_slot = choice + 0x42;
        if (choice != 0)
        {
          if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) == 0)
          {
            return 0;
          }
          if (choice != 1)
          {
            return 0;
          }
          if (pick_target(&td, "TARGET_ANY") == 0)
          {
            return 0;
          }
          available_or_mode = count_counters(player, card, COUNTER_PRESSURE);
          instance->targets[1].player = available_or_mode;
          add_state(player, card, 0x10);
          tapped_for_mana_color = -1;
          kill_card(player, card, KILL_SACRIFICE);
          return 0;
        }
      }
      if (charge_mana_for_activated_ability(player, card, 4, 0, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      if (current_turn == player)
      {
        available_or_mode = 0x18;
      }
      else
      {
        available_or_mode = ((0x1d < current_phase) ? 48U : 24U);
      }
      ai_modifier = ai_modifier + available_or_mode;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    available_or_mode = instance->info_slot;
    if (available_or_mode == 0x42)
    {
      add_counters(player, card, player, card, COUNTER_PRESSURE, 1);
      available_or_mode = instance->info_slot;
    }
    if ((available_or_mode == 0x43) &&
        (available_or_mode = valid_target(&td), available_or_mode != 0))
    {
      damage_target0(player, card, instance->targets[1].player);
      return 0;
    }
  }
  return 0;
}

int card_man_o_war(int player, int card, event_t event){
/* CARD_ID_MAN_O_WAR	1613
Man-o'-War	|2|U
Creature - Jellyfish 2/2
When ~ enters the battlefield, return target creature to its owner's hand. */

	// Used externally
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td1) && pick_target(&td1, "TARGET_CREATURE")){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_miracolous_recovery(int player, int card, event_t event){
/* CARD_ID_MIRACULOUS_RECOVERY	2743
Miraculous Recovery	|4|W
Instant
Return target creature card from your graveyard to the battlefield. Put a +1/+1 counter on it. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_PLUS1_PLUS1_COUNTER);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

int card_mundungu(int player, int card, event_t event){
/* CARD_ID_MUNDUNGU	2746
Mundungu	|1|U|B
Creature - Human Wizard 1/1
|T: Counter target spell unless its controller pays |1 and 1 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION && counterspell_validate(player, card, &td, 0)){
		int counter = 1;

		if( has_mana(instance->targets[0].player, COLOR_COLORLESS, 1) && can_pay_life(instance->targets[0].player, 1) ){
			int ai_choice = 0;
			if( life[instance->targets[0].player]-1 < 6 ){
				ai_choice = 1;
			}
			int choice = do_dialog( instance->targets[0].player, player, card, -1, -1," Pay 1 mana and 1 life\n Pass", ai_choice);
			if( choice == 0 ){
				charge_mana(instance->targets[0].player, COLOR_COLORLESS, 1);
				if( spell_fizzled != 1 ){
					lose_life(instance->targets[0].player, 1);
					counter = 0;
				}
			}
		}

		if( counter == 1 ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST0, 0, &td, NULL);
}

int card_natural_order(int player, int card, event_t event){
/* CARD_ID_NATURAL_ORDER	2748
Natural Order	|2|G|G
Sorcery
As an additional cost to cast ~, sacrifice a |Sgreen creature.
Search your library for a |Sgreen creature card and put it onto the battlefield. Then shuffle your library. */

	if (!IS_GS_EVENT(player, card, event)){ return 0; }
	if (event == EVENT_CAST_SPELL && affect_me(player, card)){ ai_modifier += 96; }
	char msg[100];
	scnprintf(msg, 100, get_sleighted_color_text(player, card, "Select a %s creature card.", COLOR_GREEN));
	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, msg);
	test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
	if (event == EVENT_RESOLVE_SPELL){
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_SAC_PERMANENT_AS_COST, NULL, NULL, 0, &test);
}

int card_necromancy(int player, int card, event_t event){
  /* CARD_ID_NECROMANCY	2749
   * Necromancy	|2|B
   * Enchantment
   * You may cast ~ as though it had flash. If you cast it any time a sorcery couldn't have been cast,
   * the controller of the permanent it becomes sacrifices it at the beginning of the next cleanup step.
   * When ~ enters the battlefield, if it's on the battlefield, it becomes an Aura with "enchant creature put onto the battlefield with ~."
   * Put target creature card from a graveyard onto the battlefield under your control and attach ~ to it. When ~ leaves the battlefield,
   * that creature's controller sacrifices it. */

	// See also card_animate_dead() in unlimited.c and card_dance_of_the_dead() in ice_age.c


	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) && get_card_instance( player, card )->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		leaves_play(player, card, event, 1, 0);
		if (event == EVENT_LEAVES_PLAY_ABILITY){
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
		}
	}

	if (event == EVENT_BEGIN_TURN && get_card_instance( player, card )->targets[3].player == 142){
		get_card_instance( player, card )->targets[3].player = 141;
	}

	if (current_phase == PHASE_CLEANUP && get_card_instance( player, card )->targets[3].player == 141){
		kill_card(player, card, KILL_SACRIFICE);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return flash(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");

	if( event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance( player, card );
		/* real_put_into_play removes STATE_INVISIBLE just after sending EVENT_RESOLVE_SPELL instead of just before; remove it now, so in_play() works right in
		 * any come-into-play effects.  (This might be a more reliable way of determining whether a card was played from hand.) */
		instance->state &= ~STATE_INVISIBLE;

		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_ATTACH_AS_AURA);
			if (in_play(player, card) && !can_sorcery_be_played(player, event)){
				instance->targets[3].player = current_phase == PHASE_DISCARD ? 142 : 141;
			}
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}

int card_necrosavant(int player, int card, event_t event){
	/* Necrosavant	|3|B|B|B
	 * Creature - Zombie Giant 5/5
	 * |3|B|B, Sacrifice a creature: Return ~ from your graveyard to the battlefield. Activate this ability only during your upkeep. */

	if( IS_AI(player) ){
		if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
			if( has_mana_multi(player, MANACOST_XB(3, 2))  && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
				int choice = do_dialog(player, player, card, -1, -1," Return Necrosavant to play\n Pass\n", 0);
				if( choice == 0 ){
					charge_mana_multi(player, MANACOST_XB(3, 2));
					if( spell_fizzled != 1){
						card_instance_t *instance = get_card_instance(player, card);
						controller_sacrifices_a_creature(player, card);
						instance->state &= ~STATE_INVISIBLE;
						++hand_count[player];
						put_into_play(player, card);
						return -1;
					}
				}
			}
			return -2;
		}
	}
	else{
		if(event == EVENT_GRAVEYARD_ABILITY){
			if( has_mana_multi(player, MANACOST_XB(3, 2)) && current_phase == PHASE_UPKEEP && current_turn == player &&
				can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) )
			{
				return GA_RETURN_TO_PLAY;
			}
		}

		if( event == EVENT_PAY_FLASHBACK_COSTS ){
			charge_mana_multi(player, MANACOST_XB(3, 2));
			if( spell_fizzled != 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
				if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					return GAPAID_REMOVE;
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	return 0;
}

int card_nekrataal(int player, int card, event_t event){
  /* CARD_ID_NEKRATAAL	1098
   * Nekrataal	|2|B|B
   * Creature - Human Assassin 2/1
   * First strike
   * When ~ enters the battlefield, destroy target nonartifact, non|Sblack creature. That creature can't be regenerated. */

	// Used externally


	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.illegal_type = TYPE_ARTIFACT;
		td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card);

		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target target nonartifact, non%s creature.", COLOR_BLACK));
		if( can_target(&td) && new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_parapet(int player, int card, event_t event){
/* CARD_ID_PARAPET	2754
Parapet	|1|W
Enchantment
You may cast ~ as though it had flash. If you cast it any time a sorcery couldn't have been cast,
the controller of the permanent it becomes sacrifices it at the beginning of the next cleanup step.
Creatures you control get +0/+1. */

	if( event == EVENT_SHOULD_AI_PLAY ){
		return should_ai_play(player, card);
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! can_sorcery_be_played(player, event) ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->targets[3].player = 141;
			instance->targets[3].card = current_turn;
			if( current_phase == PHASE_DISCARD ){
				instance->targets[3].card = 1-current_turn;
			}
		}
	}

	if( get_card_instance(player, card)->targets[3].player == 141 && get_card_instance(player, card)->targets[3].card == current_turn &&
		current_phase == PHASE_CLEANUP )
	{
		kill_card(player, card, KILL_SACRIFICE);
	}

	boost_creature_type(player, card, event, -1, 0, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	return flash(player, card, event);
}

int card_pillar_tombs_of_aku(int player, int card, event_t event){
/* CARD_ID_PILLAR_TOMBS_OF_AKU	2757
Pillar Tombs of Aku	|2|B|B
World Enchantment
At the beginning of each player's upkeep, that player may sacrifice a creature. If that player doesn't, he or she loses 5 life and you sacrifice ~. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int ll = 1;
		if( can_sacrifice(player, current_turn, 1, TYPE_CREATURE, 0) ){
			int ai_choice = 1;
			if( life[current_turn]-5 < 6 ){
				ai_choice = 0;
			}
			int choice = do_dialog(current_turn, player, card, -1, -1, " Sac a creature\n Pass", ai_choice);
			if( choice == 0 && sacrifice(player, card, current_turn, 0, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
				ll = 0;
			}
		}
		if( ll == 1 ){
			lose_life(current_turn, 5);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return enchant_world(player, card, event);
}

/* ManalinkEh 2022: 0x2014b10; original cards/visions.c:2578. */
int prosperity_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (trigger_condition != TRIGGER_REPLACE_CARD_DRAW)
  {
    return 0;
  }
  if ((((card == affected_card) && (player == affected_card_controller)) && (suppress_draw == 0)) &&
      (*deck_ptr[reason_for_trigger_controller] == -1))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      instance = get_card_instance(player, card);
      if (player == reason_for_trigger_controller)
      {
        instance->targets[6].player = 1;
      }
      else if (reason_for_trigger_controller == 1 - player)
      {
        instance->targets[6].card = 1;
      }
      suppress_draw = 1;
    }
  }
  return 0;
}

int card_prosperity(int player, int card, event_t event)
{
  /* CARD_ID_PROSPERITY	1497
Prosperity	|X|U
Sorcery
Each player draws X cards. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    card_instance_t* instance = get_card_instance(player, card);
    int legacy = create_targetted_legacy_effect(player, card, prosperity_legacy, player, card);
    APNAP(p, {
      draw_cards(p, instance->info_slot);
    });
    card_instance_t* result = get_card_instance(player, legacy);
    int first = result->targets[6].player;
    int second = get_card_instance(player, legacy)->targets[6].card;
    if (second < 0 && first >= 0)
      lose_the_game(player);
    else if (first < 0 && second >= 0)
      lose_the_game(1-player);
    else if (first >= 0 && second >= 0)
      lose_the_game(ANYBODY);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_pygmy_hippo(int player, int card, event_t event){
  /* Pygmy Hippo English ||U|G
   * Creature — Hippo 2/2
   * Whenever Pygmy Hippo attacks and isn't blocked, you may have defending player activate a mana ability of each land he or she controls
   * and empty his or her mana pool. If you do, Pygmy Hippo assigns no combat damage this turn and at the beginning of your next main phase
   * this turn, you add an amount of {C} to your mana pool equal to the amount of mana emptied from that player's mana pool this way. */

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
 	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_attacking(player, card) && is_unblocked(player, card) && current_phase == PHASE_AFTER_BLOCKING && instance->info_slot != 66 ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE){
		instance->info_slot = 66;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
			negate_combat_damage_this_turn(instance->parent_controller, instance->parent_card,
											instance->parent_controller, instance->parent_card, 0);

			int count;
			for( count = active_cards_count[1-player]-1; count > -1 ; count--){
				if( in_play(1-player, count) && is_what(1-player, count, TYPE_LAND) ){
					if( mana_producer(1-player, count, EVENT_CAN_ACTIVATE) ){
						call_card_function(1-player, count, EVENT_ACTIVATE);
					}
				}
			}

			int mana_drained = 0;
			int i;
			for (i = COLOR_COLORLESS; i <= COLOR_WHITE; ++i){
				int amount = mana_pool[1-player][i];
				mana_pool[1-player][i] = 0;
				mana_drained += amount;
			}

			int legacy_card = create_legacy_effect(player, card, &effect_mana_drain );
			card_instance_t *legacy = get_card_instance(player, legacy_card);
			legacy->targets[0].card = mana_drained;
			if( current_turn == player && current_phase < PHASE_MAIN2 ){
				legacy->targets[0].player = PHASE_MAIN2;
			}
			else{
				legacy->targets[0].player = PHASE_MAIN1;
			}
			legacy->targets[1].card = get_id(player, card);
	}

	return 0;
}

int card_quicksand(int player, int card, event_t event){
  /* Quicksand	""
   * Land
   * |T: Add |C.
   * |T, Sacrifice this land: Target attacking creature without flying gets -1/-2 until end of turn. */

	/*
	  Quicksand
	  Land
	  {T}: Add {1} to your mana pool.
	  {T}, Sacrifice Quicksand: Target attacking creature without flying gets -1/-2 until end of turn.
	*/

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	td.illegal_abilities |= KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE){
		instance->info_slot = 0;
		int choice = 0;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0,
										&td, NULL) )
		{
			choice = do_dialog(player, player, card, -1, -1, " Get mana\n Trap a creature\n Cancel", 1);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, MANACOST0) &&
					new_pick_target(&td, "Select target attacking creature without flying.", 0, 1 | GS_LITERAL_PROMPT)
				  ){
					instance->info_slot = 1;
					kill_card(player, card, KILL_SACRIFICE);
				}
				else{
					remove_state(player, card, STATE_TAPPED);
				}
			tapped_for_mana_color = -1;
		}
		else{
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 && validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -1, -2);
		}
	}

	return 0;
}

int quirion_ranger_ability(int player, int card, event_t event, target_definition_t* td_bounce, const char* prompt)
{
	if (event == EVENT_CLEANUP && affect_me(player, card))
		get_card_instance(player, card)->targets[2].player = 0;

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_untap;
	default_target_definition(player, card, &td_untap, TYPE_CREATURE);
	td_untap.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(td_bounce) &&
		!(instance->targets[2].player > 0 && (instance->targets[2].player & (1 << player))) &&
		can_use_activated_abilities(player, card) &&
		has_mana_for_activated_ability(player, card, MANACOST_X(0)) )
		return can_target(&td_untap);

	if (event == EVENT_ACTIVATE)
	{
		int tgtd = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(0)) &&
			select_target(player, card, td_bounce, prompt, &instance->targets[1]) )
		{
			state_untargettable(instance->targets[1].player, instance->targets[1].card, 1);
			if (!can_target(&td_untap))
				tgtd = -1;
			else
				tgtd = pick_target(&td_untap, "TARGET_CREATURE") ? 1 : 0;
			state_untargettable(instance->targets[1].player, instance->targets[1].card, 0);
		}

		if (tgtd)	// either successfully targeted, or couldn't target both bounce and untap targets (so still prevent from reactivating, to prevent AI loops)
		{
			if (instance->targets[2].player < 0)
				instance->targets[2].player = 0;

			instance->targets[2].player |= 1 << player;
		}

		if (tgtd > 0)	// successfully targeted
		{
			bounce_permanent(instance->targets[1].player, instance->targets[1].card);
			instance->number_of_targets = 1;
		}
		else
			cancel = 1;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td_untap))
		untap_card(instance->targets[0].player, instance->targets[0].card);

	return 0;
}

int card_quirion_ranger(int player, int card, event_t event){
  /* CARD_ID_QUIRION_RANGER	2762
   * Quirion Ranger	|G
   * Creature - Elf 1/1
   * Return |Ha Forest you control to its owner's hand: Untap target creature. Activate this ability only once each turn. */

	// Used externally


	if (event == EVENT_CLEANUP && affect_me(player, card))
		get_card_instance(player, card)->targets[2].player = 0;

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_FOREST;
	td.illegal_abilities = 0;

	return quirion_ranger_ability(player, card, event, &td, "Select a Forest you control.");
}

int card_resistance_fighter(int player, int card, event_t event){
/* CARD_ID_RESISTANCE_FIGHTER	2767
Resistance Fighter	|W
Creature - Human Soldier 1/1
Sacrifice ~: Prevent all combat damage target creature would deal this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if( player == AI ){
		td.required_state = TARGET_STATE_ATTACKING;
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			negate_combat_damage_this_turn(player, card, instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_rainbow_efreet(int player, int card, event_t event){
  /* CARD_ID_RAINBOW_EFREET	2764
   * Rainbow Efreet	|3|U
   * Creature - Efreet 3/1
   * Flying
   * |U|U: ~ phases out. (While it's phased out, it's treated as though it doesn't exist. It phases in before you untap during your next untap step.) */

	// Used externally


	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		phase_out(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_NONE, MANACOST_U(2), 0, NULL, NULL);
}

void relentless_assault_effect(int player, int card){
	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.state = STATE_ATTACKING | STATE_ATTACKED;
	new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	get_an_additional_combat_phase();
}

int card_relentless_assault(int player, int card, event_t event){
/* CARD_ID_RELENTLESS_ASSAULT	1129
Relentless Assault	|2|R|R
Sorcery
Untap all creatures that attacked this turn. After this main phase, there is an additional combat phase followed by an additional main phase. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == player && current_phase == PHASE_MAIN2 ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_ATTACKING | STATE_ATTACKED;
		extra_combat_effect(player, card, event, 1, &test, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_retribution_of_the_meek(int player, int card, event_t event){
/* CARD_ID_RETRIBUTION_OF_THE_MEEK	2768
Retribution of the Meek	|2|W
Sorcery
Destroy all creatures with power 4 or greater. They can't be regenerated. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.power = 3;
		test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_BURY);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_righteous_aura(int player, int card, event_t event){
/* CARD_ID_RIGHTEOUS_AURA	2769
Righteous Aura	|1|W
Enchantment
|W, Pay 2 life: The next time a source of your choice would deal damage to you this turn, prevent that damage. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
	td.required_type = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_W(1), 2, &td, "TARGET_DAMAGE");
}

int card_righteous_war(int player, int card, event_t event){
/* CARD_ID_RIGHTEOUS_WAR	2770
Righteous War	|1|W|B
Enchantment
|SWhite creatures you control have protection from |Sblack.
|SBlack creatures you control have protection from |Swhite. */

	if( event == EVENT_ABILITIES && affected_card_controller == player && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) &&
		in_play(affected_card_controller, affected_card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
	{
		int color = get_color(affected_card_controller, affected_card);
		if( color & get_sleighted_color_test(player, card, COLOR_TEST_BLACK) ){
			event_result |= get_sleighted_protection(player, card, KEYWORD_PROT_WHITE);
		}
		if( color & get_sleighted_color_test(player, card, COLOR_TEST_WHITE) ){
			event_result |= get_sleighted_protection(player, card, KEYWORD_PROT_BLACK);
		}
	}

	return global_enchantment(player, card, event);
}

int card_rock_slide(int player, int card, event_t event){
/* CARD_ID_ROCK_SLIDE	2771
Rock Slide	|X|R
Instant
~ deals X damage divided as you choose among any number of target attacking or blocking creatures without flying. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_abilities |= KEYWORD_FLYING;
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 1, NULL);
		if( spell_fizzled != 1 ){
			int trgs;
			for (trgs = 0; trgs < instance->info_slot && can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", -1, 1); ++trgs)
				{}
		}
	}

	if (event == EVENT_RESOLVE_SPELL){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}

   return 0;
}

int card_rowen(int player, int card, event_t event){

	/* Rowen	|2|G|G
	 * Enchantment
	 * Reveal the first card you draw each turn. Whenever you reveal a basic land card this way, draw a card. */

	if( trigger_condition == TRIGGER_CARD_DRAWN && affect_me(player, card) && reason_for_trigger_controller == player &&
		trigger_cause_controller == player && in_play(player, card) && !is_humiliated(player, card, AL_TRIG)
	  ){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->info_slot == 66){
			return 0;
		}

		if( event == EVENT_TRIGGER){
			event_result |= 2;
		}
		else if( event == EVENT_RESOLVE_TRIGGER ){
				instance->info_slot = 66;
				int iid = get_original_internal_card_id(trigger_cause_controller, trigger_cause);
				reveal_card_iid(player, card, iid);
				if (is_basic_land_by_id(cards_data[iid].id)){
					draw_a_card(player);
				}
		}
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance(player, card)->info_slot = 0;
	}

	return global_enchantment(player, card, event);
}

int card_sands_of_time(int player, int card, event_t event){
/* CARD_ID_SANDS_OF_TIME	2773
Sands of Time	|4
Artifact
Each player skips his or her untap step.
At the beginning of each player's upkeep, that player simultaneously untaps each tapped artifact, creature,
and land he or she controls and taps each untapped artifact, creature, and land he or she controls. */

  // Skip-untap effect handled in engine.c:untap_phase()

  upkeep_trigger_ability(player, card, event, 2);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
	int i, end = active_cards_count[current_turn];
	int marked[151] = {0};

	for (i = 0; i < end; ++i)
	  if (in_play(current_turn, i) && is_what(current_turn, i, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND))
		marked[i] = is_tapped(current_turn, i) ? 1 : 2;

	for (i = 0; i < end; ++i)
	  if (marked[i] == 1)
		untap_card(current_turn, i);
	  else if (marked[i] == 2)
		tap_card(current_turn, i);
  }

  return 0;
}

int card_shimmering_efreet(int player, int card, event_t event){
/* CARD_ID_SHIMMERING_EFREET	2774
Shimmering Efreet	|2|U
Creature - Efreet 2/2
Flying
Phasing (This phases in or out before you untap during each of your untap steps. While it's phased out, it's treated as though it doesn't exist.)
Whenever ~ phases in, target creature phases out. (It phases in before its controller untaps during his or her next untap step.) */

	// phase-in ability implemented in phase_in_impl()
	return phasing(player, card, event);
}

int card_simoon(int player, int card, event_t event){
/* CARD_ID_SIMOON	2776
Simoon	|R|G
Instant
~ deals 1 damage to each creature target opponent controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			new_damage_all(player, card, instance->targets[0].player, 1, NDA_ALL_CREATURES, NULL);
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_snake_basket(int player, int card, event_t event){
/* Snake Basket	|4
 * Artifact
 * |X, Sacrifice ~: Put X 1/1 |Sgreen Snake creature tokens onto the battlefield. Activate this ability only any time you could cast
 *		a sorcery. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_tokens_by_id(player, card, CARD_ID_SNAKE, get_card_instance(player, card)->info_slot);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_SORCERY_BE_PLAYED, MANACOST_X(-1), 0, NULL, NULL);
}

int card_squandered_resources(int player, int card, event_t event){
/* CARD_ID_SQUANDERED_RESOURCES	2783
Squandered Resources	|B|G
Enchantment
Sacrifice a land: Add to your mana pool one mana of any type the sacrificed land could produce. */

	if( event == EVENT_CAN_ACTIVATE ){
		return can_sacrifice_type_as_cost(player, 1, TYPE_LAND) && can_produce_mana(player, card);
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier -= 36;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if( ! sac ){
			cancel = 1;
			return 0;
		}
		int lnd = BYTE3(sac);
		if( !is_mana_producer_land(player, lnd) ){
			ai_modifier -= 92;
		}
		else{
			int color = get_colors_of_mana_land_could_produce_ignoring_costs(player, lnd);
			if (color & COLOR_TEST_ARTIFACT){
				// by analogy with the Mana Flare ruling on restricted-use mana, this shouldn't copy the restrictions
				color &= ~COLOR_TEST_ARTIFACT;
				color |= COLOR_TEST_COLORLESS;
			}
			if (!color){
				ai_modifier -= 92;
			}
			else {
				produce_mana_all_one_color(player, color, 1);
			}
			if (cancel != 1){
				tapped_for_mana_color = -2;
				kill_card(player, lnd, KILL_SACRIFICE);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_stampeding_wildebeest(int player, int card, event_t event){
/* CARD_ID_STAMPEDING_WILDEBEESTS	2784
Stampeding Wildebeests	|2|G|G
Creature - Antelope Beast 5/4
Trample (This creature can deal excess combat damage to defending player or planeswalker while attacking.)
At the beginning of your upkeep, return a |Sgreen creature you control to its owner's hand. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;
		td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		td.illegal_abilities = 0;

		card_instance_t *instance = get_card_instance(player, card);

		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select a %s creature you control.", COLOR_GREEN));
		if( new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_suleimans_legacy(int player, int card, event_t event){
/* CARD_ID_SULEIMANS_LEGACY	2785
Suleiman's Legacy	|R|W
Enchantment
When ~ enters the battlefield, destroy all Djinns and Efreets. They can't be regenerated.
Whenever a Djinn or Efreet enters the battlefield, destroy it. It can't be regenerated. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_EFREET;
		test.sub2 = SUBTYPE_DJINN;
		test.subtype_flag = F2_MULTISUBTYPE;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_BURY);
	}

	if (trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card))
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.subtype = SUBTYPE_EFREET;
		test.sub2 = SUBTYPE_DJINN;
		test.subtype_flag = F2_MULTISUBTYPE;
		if( new_specific_cip_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card) ){
			kill_card(trigger_cause_controller, trigger_cause, KILL_BURY);
		}
	}

	return global_enchantment(player, card, event);
}

int card_summer_bloom(int player, int card, event_t event){
	/* Summer Bloom	|1|G
	 * Sorcery
	 * You may play up to three additional lands this turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.zone = TARGET_ZONE_HAND;
		ai_modifier += (check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test) * 16);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int effect = create_legacy_effect(player, card, &check_playable_lands_legacy);
		card_instance_t *leg = get_card_instance(player, effect);
		leg->targets[2].card = get_id(player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_suqata_assassin(int player, int card, event_t event){
/* CARD_ID_SUQATA_ASSASSIN	2788
Suq'Ata Assassin	|1|B|B
Creature - Human Assassin 1/1
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
Whenever ~ attacks and isn't blocked, defending player gets a poison counter. (A player with ten or more poison counters loses the game.) */

	fear(player, card, event);
	if( event == EVENT_DECLARE_BLOCKERS ){
		if( is_unblocked(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
			poison(1-player, 1);
		}
	}
	return 0;
}

int card_suq_ata_lancer(int player, int card, event_t event){
/* CARD_ID_SUQATA_LANCER	2789
Suq'Ata Lancer	|2|R
Creature - Human Knight 2/2
Haste
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.) */

	haste(player, card, event);

	flanking(player, card, event);

	return 0;
}

int card_teferi_honor_guard(int player, int card, event_t event){
/* CARD_ID_TEFERIS_HONOR_GUARD	2793
Teferi's Honor Guard	|2|W
Creature - Human Knight 2/2
Flanking (Whenever a creature without flanking blocks this creature, the blocking creature gets -1/-1 until end of turn.)
|U|U: ~ phases out. (While it's phased out, it's treated as though it doesn't exist. It phases in before you untap during your next untap step.) */

	flanking(player, card, event);

	return card_rainbow_efreet(player, card, event);
}

int teferis_puzzle_box_effect(int s_player, int s_card, int t_player){
// AKA "put the cards in your hand on the bottom of your library in any order, then draw that many cards."
	int amount = hand_count[t_player];
	int choice = 1;
	if( ! IS_AI(t_player) ){
		choice = DIALOG(s_player, s_card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_WHO_CHOOSES(t_player),
						"Auto", 1, 1,
						"Manual", 1, 1);
	}
	if( choice == 1 ){
		int c;
		for(c = active_cards_count[t_player]-1; c > -1; c--){
			if( in_hand(t_player, c) ){
				put_on_bottom_of_deck(t_player, c);
			}
		}
	}
	if( choice == 2 ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a card to put on bottom.");
		while( hand_count[t_player] ){
				int selected = new_select_a_card(t_player, t_player, TUTOR_FROM_HAND, 1, AI_FIRST_FOUND, -1, &test);
				put_on_bottom_of_deck(t_player, selected);
		}
	}
	draw_cards(t_player, amount);

	return amount;
}

int card_teferis_puzzle_box(int player, int card, event_t event){

	/* Teferi's Puzzle Box	|4
	 * Artifact
	 * At the beginning of each player's draw step, that player puts the cards in his or her hand on the bottom of his or her library in any order, then draws
	 * that many cards. */

	if( event == EVENT_DRAW_PHASE && in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		teferis_puzzle_box_effect(player, card, current_turn);
	}

	return 0;
}

int card_teferis_realm(int player, int card, event_t event){

	/* Teferi's Realm	|1|U|U
	 * World Enchantment
	 * At the beginning of each player's upkeep, that player chooses artifact, creature, land, or non-Aura enchantment. All nontoken permanents of that type
	 * phase out. */

	upkeep_trigger_ability(player, card, event, 2);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int types[] = {TYPE_ARTIFACT, TYPE_CREATURE, TYPE_LAND, TYPE_ENCHANTMENT};
		int choice = DIALOG(player, card, EVENT_ACTIVATE,
							DLG_WHO_CHOOSES(current_turn), DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
							"Artifact", 1, 1,
							"Creature", 1, 1,
							"Land", 1, 1,
							"Non-Aura enchantment", 1, 1);

		test_definition_t this_test;
		default_test_definition(&this_test, types[choice - 1]);
		this_test.type_flag = F1_NO_TOKEN;
		if (choice == 4){
			this_test.subtype = SUBTYPE_AURA;
			this_test.subtype_flag = DOESNT_MATCH;
		}
		new_manipulate_all(player, card, 2, &this_test, ACT_PHASE_OUT);
	}

	return enchant_world(player, card, event);
}

static int effect_three_wishes(int player, int card, event_t event)
{
  // Destroy may-play-from-exile effects at start of turn
  if (event == EVENT_BEGIN_TURN && current_turn == player)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int i, found = 0;
	  for (i = 0; i < 3; ++i)
		if (instance->targets[i].card != -1)
		  {
			card_instance_t* eff;
			if ((eff = in_play(player, instance->targets[i].card)))
			  {
				instance->targets[i].player = eff->targets[0].card;	// csvid of exiled card
				kill_card(player, instance->targets[i].card, KILL_EXILE);
				++found;
			  }
			instance->targets[i].card = -1;
		  }

	  if (!found)
		kill_card(player, card, KILL_EXILE);
	}

  if (trigger_condition == TRIGGER_UPKEEP && current_turn == player)
	{
	  // become visible at start of upkeep triggers, so can be properly ordered
	  card_instance_t* instance = get_card_instance(player, card);
	  instance->token_status &= ~STATUS_INVISIBLE_FX;

	  if (upkeep_trigger(player, card, event))
		{
		  /* If any may-play-from-exile effects are still around - perhaps this was played during a previous upkeep in the same turn with Paradox Haze or such -
		   * destroy them now. */
		  int i;
		  for (i = 0; i < 3; ++i)
			if (instance->targets[i].card != -1)
			  {
				card_instance_t* eff;
				if ((eff = in_play(player, instance->targets[i].card)))
				  {
					instance->targets[i].player = eff->targets[0].card;	// csvid of exiled card
					kill_card(player, instance->targets[i].card, KILL_EXILE);
				  }
				instance->targets[i].card = -1;
			  }

		  // And put any cards that weren't played via the may-play-from-exile effects into graveyard
		  for (i = 0; i < 3; ++i)
			if (instance->targets[i].player != -1)
			  {
				int pos = add_csvid_to_rfg(player, instance->targets[i].player);
				from_exile_to_graveyard(player, pos);
			  }

		  kill_card(player, card, KILL_EXILE);
		}
	}

  return 0;
}

int card_three_wishes(int player, int card, event_t event)
{
  /* Three Wishes	|1|U|U
   * Instant
   * Exile the top three cards of your library face down. You may look at those cards for as long as they remain exiled. Until your next turn, you may play
   * those cards. At the beginning of your next upkeep, put any of those cards you didn't play into your graveyard. */

	if (event == EVENT_CAN_CAST)
		return basic_spell(player, card, event);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) )
		ai_modifier += (count_subtype(player, TYPE_LAND, -1) * 8);

	if (event == EVENT_RESOLVE_SPELL)
	{
	  int legacies[3] = { -1, -1, -1 };
	  int i;
	  for (i = 0; i < 3; ++i)
		if (deck_ptr[player][0] != -1)
		  {
			int csvid = cards_data[deck_ptr[player][0]].id;
			obliterate_top_card_of_deck(player);
			legacies[i] = create_may_play_card_from_exile_effect(player, card, player, csvid, MPCFE_FACE_DOWN);
			// not MPCFE_UNTIL_EOT, since we'll be taking care of it in effect_three_wishes
		  }

	  if (legacies[0] != -1)	// i.e., if any cards were exiled at all
		{
		  int leg = create_legacy_effect(player, card, effect_three_wishes);
		  if (leg != -1)
			{
			  card_instance_t* legacy = get_card_instance(player, leg);
			  legacy->targets[0].card = legacies[0];
			  legacy->targets[1].card = legacies[1];
			  legacy->targets[2].card = legacies[2];
			  legacy->token_status |= STATUS_INVISIBLE_FX;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_tin_wing_chimera(int player, int card, event_t event){
	/* Tin-Wing Chimera	|4
	 * Artifact Creature - Chimera 2/2
	 * Flying
	 * Sacrifice ~: Put a +2/+2 counter on target Chimera creature. It gains flying. */
	return chimera(player, card, event, KEYWORD_FLYING, 0);
}

int card_tithe(int player, int card, event_t event){

	/* Tithe	|W
	 * Instant
	 * Search your library for |Ha Plains card. If target opponent controls more lands than you, you may search your library for an additional |H2Plains
	 * card. Reveal those cards and put them into your hand. Then shuffle your library. */

	if( event == EVENT_CAN_CAST ){
		ai_modifier += (count_permanents_by_type(player, TYPE_LAND) < count_permanents_by_type(1-player, TYPE_LAND) ? 24 : 48);
		ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 24 : 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int num = 1;
		if( count_permanents_by_type(player, TYPE_LAND) < count_permanents_by_type(1-player, TYPE_LAND) ){
			num++;
		}

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND,
									get_hacked_land_text(player, card, num == 2 ? "Select up to two %s cards." : "Select %a card.", SUBTYPE_PLAINS));
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
		test.qty = num;

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);

		kill_card(player, card, KILL_DESTROY );
	}

	return basic_spell(player, card, event);
}

int card_tremor(int player, int card, event_t event){
  /* Tremor |R
   * 	  Sorcery
   * 	  Tremor deals 1 damage to each creature without flying. */

	if (event == EVENT_RESOLVE_SPELL){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.keyword = KEYWORD_FLYING;
		test.keyword_flag = DOESNT_MATCH;

		new_damage_all(player, card, ANYBODY, 1, 0, &test);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* Also used by Viridian Shaman */
int card_uktabi_orangutan(int player, int card, event_t event){
  /* CARD_ID_UKTABI_ORANGUTAN	1698
   * Uktabi Orangutan	|2|G
   * Creature - Ape 2/2
   * When ~ enters the battlefield, destroy target artifact. */

	// Used externally


	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_ARTIFACT")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_undiscovered_paradise(int player, int card, event_t event){
/* CARD_ID_UNDISCOVERED_PARADISE	2801
Undiscovered Paradise	""
Land
|T: Add one mana of any color to your mana pool. During your next untap step, as you untap your permanents, return ~ to its owner's hand. */

	if( event == EVENT_ACTIVATE ){
		mana_producer(player, card, event);
		if( spell_fizzled != 1 ){
			get_card_instance(player, card)->info_slot = 66;
			create_targetted_legacy_effect(player, card, &empty, player, card);
		}
		return 0;
	}

	if( get_card_instance(player, card)->info_slot == 66 && event == EVENT_UNTAP && current_phase == PHASE_UNTAP ){
		if( affect_me(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
			bounce_permanent(player, card);
		}
	}

	return mana_producer(player, card, event);
}

int card_undo(int player, int card, event_t event){
/* CARD_ID_UNDO	2802
Undo	|1|U|U
Sorcery
Return two target creatures to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<2; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

   return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

/* ManalinkEh 2022: 0x201f820; original cards/visions.c:3612. */
int card_vanishing(int player, int card, event_t event)
{
  /* Vanishing	|U
   * Enchantment - Aura
   * Enchant creature
   * |U|U: Enchanted creature phases out. (While it's phased out, it's treated as though it doesn't exist. It phases in before its controller untaps during their next untap step.) */

  card_instance_t *instance;

  if (event != EVENT_ACTIVATE)
  {
    if (((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2) || (event == EVENT_CLEANUP))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        phase_out((int)instance->damage_target_player, instance->damage_target_card);
      }
      return generic_activated_ability(player, card, event, 0, 0, 0, 2, 0, 0, 0, 0, NULL, NULL);
    }
    if (event == EVENT_CAN_CAST)
    {
      return vanilla_aura(player, card, event, player);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return vanilla_aura(player, card, event, player);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return vanilla_aura(player, card, event, player);
    if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CAN_MOVE_AURA) ||
         ((((uint32_t)(event - EVENT_POWER) < 3 || (event == EVENT_RESOLVE_MOVING_AURA)) ||
           (event == EVENT_CHANGE_TYPE)))) ||
        (((event == EVENT_SET_COLOR || (event == EVENT_GET_SELECTED_CARD)) ||
          (event == EVENT_STATIC_EFFECTS))))
      return vanilla_aura(player, card, event, player);
  }
  get_card_instance(player, card);
  return generic_activated_ability(player, card, event, 0, 0, 0, 2, 0, 0, 0, 0, NULL, NULL);
}

int card_viashivan_dragon(int player, int card, event_t event){
/* CARD_ID_VIASHIVAN_DRAGON	2806
Viashivan Dragon	|2|R|R|G|G
Creature - Dragon 4/4
Flying
|R: ~ gets +1/+0 until end of turn.
|G: ~ gets +0/+1 until end of turn. */

	if( event == EVENT_POW_BOOST && current_turn == player ){
		return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
	}

	if( event == EVENT_TOU_BOOST && current_turn == player ){
		return generic_shade(player, card, event, 0, MANACOST_G(1), 0, 1, 0, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_P1_P0 = 1,
		CHOICE_P0_P1
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0) ){
			return 1;
		}
		return generic_shade(player, card, event, 0, MANACOST_G(1), 0, 1, 0, 0);
	}

	if( event == EVENT_ACTIVATE  && can_use_activated_abilities(player, card) ){
		int abils[3] = {
						0,
						has_mana_for_activated_ability(player, card, MANACOST_R(1)),
						has_mana_for_activated_ability(player, card, MANACOST_G(1))
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"+1/+0", abils[1], current_turn == player ? 10 : 1,
							"+0/+1", abils[2], current_turn == 1-player ? 10 : 1);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_P1_P0 ){
			return generic_shade(player, card, event, 0, MANACOST_R(1), 1, 0, 0, 0);
		}
		if( choice == CHOICE_P0_P1 ){
			return generic_shade(player, card, event, 0, MANACOST_G(1), 0, 1, 0, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_P1_P0 ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
		}
		if( instance->info_slot == CHOICE_P0_P1 ){
			return generic_shade_merge_pt(player, card, event, 0, MANACOST_G(1), 0, 1);
		}
	}

	return 0;
}

int card_wand_of_denial(int player, int card, event_t event){
/* CARD_ID_WAND_OF_DENIAL	2809
Wand of Denial	|2
Artifact
|T: Look at the top card of target player's library. If it's a nonland card, you may pay 2 life. If you do, put it into that player's graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[instance->targets[0].player];
			if( deck[0] != -1 ){
				show_deck( player, deck, 1, "Here's the first card of target player's deck", 0, gs_done_008b40e0 );
				if( !(cards_data[deck[0]].type & TYPE_LAND) && can_pay_life(player, 2) ){
					card_ptr_t* c = cards_ptr[ cards_data[deck[0]].id ];
					int ai_choice = 1;
					if( player == AI && life[player] > 5 && c->ai_base_value > 75 ){
						ai_choice = 0;
					}
					int choice = do_dialog(player, player, card, -1, -1, " Pay 2 life and mill\n Pass", ai_choice);
					if( choice == 0 ){
						lose_life(player, 2);
						mill(instance->targets[0].player, 1);
					}
				}
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK+GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_warriors_honor(int player, int card, event_t event){
  /* Warrior's Honor |2|W
   * 	  Instant
   * 	  Creatures you control get +1/+1 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_waterspout_djinn(int player, int card, event_t event){
/* CARD_ID_WATERSPOUT_DJINN	2811
Waterspout Djinn	|2|U|U
Creature - Djinn 4/4
Flying
At the beginning of your upkeep, sacrifice ~ unless you return an untapped |H2Island you control to its owner's hand. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.allowed_controller = td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.illegal_state = TARGET_STATE_TAPPED;
		td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);

		card_instance_t *instance = get_card_instance(player, card);

		int kill_me = 1;
		char msg[100];
		scnprintf(msg, 100, "%s", get_hacked_land_text(player, card, "Select an untapped %s to bounce.", SUBTYPE_ISLAND));

		if( can_target(&td) && new_pick_target(&td, msg, 0, GS_LITERAL_PROMPT) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			kill_me = 0;
		}

		instance->number_of_targets = 0;
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_wind_shear(int player, int card, event_t event){
/* CARD_ID_WIND_SHEAR	2813
Wind Shear	|2|G
Instant
Attacking creatures with flying get -2/-2 and lose flying until end of turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		this_test.keyword = KEYWORD_FLYING;
		pump_creatures_until_eot(player, card, ANYBODY, 0, -2,-2, 0,0, &this_test);
		int c;
		for( c=active_cards_count[current_turn]-1; c>-1; c--){
			if( in_play(current_turn, c) && new_make_test_in_play(current_turn, c, -1, &this_test) ){
				negate_ability_until_eot(player, card, current_turn, c, KEYWORD_FLYING);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static const char* is_damage_to_zhalfirin_crusader(int who_chooses, int player, int card, int targeting_player, int targeting_card){
	card_instance_t* dmg = get_card_instance(player, card);
	if( dmg->internal_card_id != damage_card ){
		return "must be a damage card";
	}
	if( dmg->damage_target_player != targeting_player || dmg->damage_target_card != targeting_card ){
		return "must be damage to Zhalfirin Crusader";
	}
	return NULL;
}

int card_zhalfirin_crusader(int player, int card, event_t event){
	/* Zhalfirin Crusader	|1|W|W	0x2003257
	 * Creature - Human Knight 2/2
	 * Flanking
	 * |1|W: The next 1 damage that would be dealt to ~ this turn is dealt to any target instead. */
	flanking(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = target_predicate_extra(is_damage_to_zhalfirin_crusader);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td2) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_XW(1, 1), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_XW(1, 1), 0, &td, "TARGET_DAMAGE");
		if( spell_fizzled != 1 ){
			new_pick_target(&td2, "TARGET_ANY", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td2, 1) ){
			card_instance_t *source = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( source->info_slot > 0 ){
				if( source->info_slot == 1 ){
					source->damage_target_player = instance->targets[1].player;
					source->damage_target_card = instance->targets[1].card;
				}
				else{
					source->info_slot--;
					damage_creature(instance->targets[1].player, instance->targets[1].card, 1,
									source->damage_source_player, source->damage_source_card);
				}
			}
		}
	}
	return 0;
}

/* ManalinkEh 2022: 0x2020790; original cards/visions.c:3880. */
int card_betrayal(int player, int card, event_t event)
{
  /* Betrayal	|U
   * Enchantment - Aura
   * Enchant creature an opponent controls
   * Whenever enchanted creature becomes tapped, you draw a card. */

  card_instance_t *instance;
  target_definition_t td;

  instance = in_play(player, card);
  if (instance &&
      (instance = get_card_instance(player, card), instance->damage_target_player >= 0) &&
      (instance = get_card_instance(player, card), event == EVENT_TAP_CARD) &&
      instance->damage_target_card == affected_card &&
      instance->damage_target_player == affected_card_controller)
    draw_a_card(player);
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.preferred_controller = td.allowed_controller;
  return targeted_aura_custom_prompt(player, card, event, &td,
                                     "Select target creature an opponent controls.");
}

/* ManalinkEh 2022: 0x2014cf0; original cards/visions.c:3906. */
static int brood_of_cockroaches_legacy(int player, int card, event_t event)
{
  int result;
  int owner;
  int position;
  result = eot_trigger(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  result = find_in_owners_graveyard(player, card, &owner, &position);
  if (result != 0)
  {
    result = graveyard_ptr_mutable[owner][position];
    remove_card_from_grave(owner, position);
    add_card_to_hand(owner, result);
    lose_life(owner, 1);
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2020910; original cards/visions.c:3920. */
int card_brood_of_cockroaches(int player, int card, event_t event)
{
  /* Brood of Cockroaches	|1|B
   * Creature - Insect 1/1
   * When this creature is put into your graveyard from the battlefield, at the beginning of the next end step, you lose 1 life and return this card to your hand. */

  int result;
  int owner;
  int position;
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    result = find_in_owners_graveyard(player, card, &owner, &position);
    if (result != 0)
    {
      create_legacy_effect(player, card, brood_of_cockroaches_legacy);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2020990; original cards/visions.c:3934. */
int card_daraja_griffin(int player, int card, event_t event)
{
  /* Daraja Griffin	|3|W
   * Creature - Griffin 2/2
   * Flying
   * Sacrifice this creature: Destroy target black creature. */

  card_instance_t *instance;
  const char *prompt;
  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  td.required_color = get_sleighted_color_test(player, card, 2);
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  }
  prompt = get_sleighted_color_text(player, card, "Select target %s creature", 1);
  result = generic_activated_ability(player, card, event, 0xe, 0, 0, 0, 0, 0, 0, 0, &td, prompt);
  return result;
}

/* ManalinkEh 2022: 0x2020b10; original cards/visions.c:3961. */
int card_dark_privilege(int player, int card, event_t event)
{
  /* Dark Privilege	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1.
   * Sacrifice a creature: Regenerate enchanted creature. */

  char attached_player;
  card_instance_t *instance;
  int attached_card_or_result;
  int activation_available;
  test_definition_t test;

  instance = get_card_instance(player, card);
  if (((-1 < instance->damage_target_player) &&
       (attached_card_or_result = is_humiliated(player, card, AL_NM_NL_ACT),
        attached_card_or_result == 0)) &&
      (instance = in_play(player, card), instance != NULL))
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      if ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) != 0)
      {
        instance = get_card_instance(player, card);
        attached_player = instance->damage_target_player;
        attached_card_or_result = instance->damage_target_card;
        new_default_test_definition(&test, 2, "");
        if (event == EVENT_CAN_ACTIVATE)
        {
          activation_available = can_use_activated_abilities(player, card);
          if (((activation_available != 0) &&
               (activation_available =
                    has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0),
                activation_available != 0)) &&
              (activation_available = new_can_sacrifice_as_cost(player, card, &test),
               activation_available != 0))
          {
            return can_regenerate((int)attached_player, attached_card_or_result);
          }
        }
        else
        {
          if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) != 0)
          {
            new_sacrifice(player, card, player, SAC_AS_COST, &test);
          }
        }
      }
    }
    else if (((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP)) &&
             ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) != 0))
    {

      instance = get_card_instance(player, card);
      attached_player = instance->damage_target_player;
      attached_card_or_result = instance->damage_target_card;
      new_default_test_definition(&test, 2, "");
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          (activation_available = can_regenerate((int)attached_player, attached_card_or_result),
           activation_available != 0))
      {
        regenerate_target((int)attached_player, attached_card_or_result);
      }
    }
  }
  return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2020dc0; original cards/visions.c:3993. */
int card_death_watch(int player, int card, event_t event)
{
  /* Death Watch	|B
   * Enchantment - Aura
   * Enchant creature
   * When enchanted creature dies, its controller loses life equal to its power and you gain life equal to its toughness. */

  int result;
  card_instance_t *instance;
  result =
      attached_creature_dies_trigger_for_controller(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result != 0)
  {
    instance = get_card_instance(player, card);
    result = get_power((int)instance->damage_target_player, instance->damage_target_card);
    lose_life((int)instance->damage_target_player, result);
    result = get_toughness((int)instance->damage_target_player, instance->damage_target_card);
    gain_life(instance->parent_controller, result);
  }
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return vanilla_aura(player, card, event, player);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return vanilla_aura(player, card, event, player);
    if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
         (((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
           (event != EVENT_CHANGE_TYPE)))) &&
        (((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      return 0;
    }
  }
  return vanilla_aura(player, card, event, player);
}

/* ManalinkEh 2022: 0x2020f20; original cards/visions.c:4018. */
int card_dwarven_vigilantes(int player, int card, event_t event)
{
  /* Dwarven Vigilantes	|2|R
   * Creature - Dwarf 2/2
   * Whenever this creature attacks and isn't blocked, you may have it deal damage equal to its power to target creature. If you do, this creature assigns no combat damage this turn. */

  int c;
  int power_or_result;
  card_instance_t *instance;
  int choice_or_available;
  int matching_attackers;
  int total_attack_power;
  target_definition_t td;

  if (event == EVENT_DECLARE_BLOCKERS)
  {

    matching_attackers = 0;
    total_attack_power = 0;
    c = active_cards_count[current_turn] + -1;
    if (-1 < c)
    {
      do
      {
        instance = in_play(current_turn, c);
        if ((((instance != NULL) &&
              (power_or_result = is_what(current_turn, c, 2), power_or_result != 0)) &&
             (power_or_result = is_attacking(current_turn, c), power_or_result != 0)) &&
            (power_or_result = is_unblocked(current_turn, c), power_or_result != 0))
        {
          power_or_result = get_power(current_turn, c);
          total_attack_power = total_attack_power + power_or_result;
          power_or_result = has_same_name(player, card, current_turn, c);
          matching_attackers = matching_attackers + power_or_result;
        }
        c = c + -1;
      } while (c != -1);
    }
    choice_or_available = is_unblocked(player, card);
    if ((choice_or_available != 0) &&
        (choice_or_available = is_humiliated(player, card, AL_TRIG), choice_or_available == 0))
    {

      default_target_definition(player, card, &td, 2);
      if (((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
          ((current_turn == player && (total_attack_power < life[1 - player]))))
      {
        choice_or_available = get_power(player, card);
        td.toughness_requirement = choice_or_available * matching_attackers | 0x6000;
        choice_or_available = 0;
      }
      else
      {
        choice_or_available = 1;
      }
      choice_or_available =
          do_dialog(player, player, card, -1, -1, " Damage creature\n Pass", choice_or_available);
      if (((choice_or_available == 0) &&
           (choice_or_available = can_target(&td), choice_or_available != 0)) &&
          (choice_or_available = pick_target(&td, "TARGET_CREATURE"), choice_or_available != 0))
      {
        damage_target0(player, card, get_power(player, card));
        negate_combat_damage_this_turn(player, card, player, card, 0);
      }
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2021190; original cards/visions.c:4061. */
int card_elkin_lair(int player, int card, event_t event)
{
  /* Elkin Lair	|3|R
   * World Enchantment
   * At the beginning of each player's upkeep, that player exiles a card at random from their hand. The player may play that card this turn. At the beginning of the next end step, if the player hasn't played the card, they put it into their graveyard. */

  int csvid;
  int hand_card_or_exile_index;
  int turn_player;

  upkeep_trigger_ability(player, card, event, current_turn);
  turn_player = current_turn;
  hand_card_or_exile_index = get_random_card_in_hand(current_turn);
  if ((hand_card_or_exile_index == -1) || (event != EVENT_UPKEEP_TRIGGER_ABILITY))
  {
    if (event == EVENT_CLEANUP)
    {

      hand_card_or_exile_index = find_iid_in_rfg(turn_player, 1);
      if (-1 < hand_card_or_exile_index)
      {
        from_exile_to_graveyard(turn_player, hand_card_or_exile_index);
      }
    }
  }
  else
  {
    rfg_card_in_hand(turn_player, hand_card_or_exile_index);
    csvid = get_original_id(turn_player, hand_card_or_exile_index);
    create_may_play_card_from_exile_effect(turn_player, card, turn_player, csvid, MPCFE_UNTIL_EOT);
  }
  return enchant_world(player, card, event);
}

/* ManalinkEh 2022: 0x2021270; original cards/visions.c:4090. */
int card_feral_instinct(int player, int card, event_t event)
{
  /* Feral Instinct	|1|G
   * Instant
   * Target creature gets +1/+1 until end of turn.
   * Draw a card at the beginning of the next turn's upkeep. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event == EVENT_CHECK_PUMP)
  {
    instance = get_card_instance(player, card);
    result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id);
    if (result != 0)
    {
      pumpable_power[player] = pumpable_power[player] + 1;
      pumpable_toughness[player] = pumpable_toughness[player] + 1;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
  }
  else
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        result = pump_until_eot(player, card, instance->targets[0].player,
                                instance->targets[0].card, 1, 1);
        alternate_legacy_text(2, player, result);
        result = cantrip(player, card, 1);
        alternate_legacy_text(1, player, result);
      }
      kill_card(player, card, KILL_DESTROY);
    }
  }
  result = generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
  return result;
}

/* ManalinkEh 2022: 0x2021400; original cards/visions.c:4120. */
int card_foreshadow(int player, int card, event_t event)
{
  /* Foreshadow	|1|U
   * Instant
   * Choose a card name, then target opponent mills a card. If a card with the chosen name was milled this way, you draw a card.
   * Draw a card at the beginning of the next turn's upkeep. */

  int t_player;
  int top_iid;
  int named_csvid;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {
      new_default_test_definition(&test, 0xff, "Choose a card name.");
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        named_csvid = ai_name_a_random_csvid(player, card, 1 - player, TYPE_NONE, &test);
      }
      else
      {
        named_csvid = new_card_from_list(player, 3, &test);
      }
      if (-1 < named_csvid)
      {
        instance = get_card_instance(player, card);
        t_player = instance->targets[0].player;
        top_iid = *deck_ptr[t_player];
        if (top_iid != -1)
        {
          mill(t_player, 1);
          if (has_same_name(-1, named_csvid, -1, (unsigned int)cards_data[top_iid].id) != 0)
          {
            draw_a_card(player);
          }
        }
      }
      cantrip(player, card, 1);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x40, &td, NULL, 1, NULL);
}

/* ManalinkEh 2022: 0x2014da0; original cards/visions.c:4161. */
static int giant_caterpillar_legacy(int player, int card, event_t event)
{
  if (!eot_trigger(player, card, event))
  {
    return 0;
  }
  generate_tokens_by_id(player, card, CARD_ID_BUTTERFLY, 1);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x20215d0; original cards/visions.c:4169. */
int card_giant_caterpillar(int player, int card, event_t event)
{
  /* Giant Caterpillar	|3|G
   * Creature - Insect 3/3
   * |G, Sacrifice this creature: Create a 1/1 green Insect creature token with flying named Butterfly at the beginning of the next end step. */

  int result;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 0, 0, 0, 1, 0, 0, 0,
                                       NULL, NULL);
    return result;
  }
  if (event == EVENT_ACTIVATE)
  {
    result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 1, 0, 0);
    if ((result != 0) && (result = can_sacrifice_this_as_cost(player, card), result != 0))
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    create_legacy_effect(player, card, giant_caterpillar_legacy);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2021710; original cards/visions.c:4192. */
int card_goblin_swine_rider(int player, int card, event_t event)
{
  /* Goblin Swine-Rider	|R
   * Creature - Goblin 1/1
   * Whenever this creature becomes blocked, it deals 2 damage to each attacking creature and each blocking creature. */

  int result;
  test_definition_t this_test;

  if ((event == EVENT_DECLARE_BLOCKERS) && (current_turn == player))
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          default_test_definition(&this_test, 2);
          this_test.state = 0xc;
          new_damage_all(player, card, 2, 2, 0, &this_test);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20217e0; original cards/visions.c:4210. */
int card_heat_wave(int player, int card, event_t event)
{
  /* Heat Wave	|2|R
   * Enchantment
   * Cumulative upkeep |R (At the beginning of your upkeep, put an age counter on this permanent, then sacrifice it unless you pay its upkeep cost for each age counter on it.)
   * Blue creatures can't block creatures you control.
   * Nonblue creatures can't block creatures you control unless their controller pays 1 life for each blocking creature they control. */

  int result;
  unsigned int source_colors;
  unsigned int required_color;

  cumulative_upkeep(player, card, event, 0, 0, 0, 0, 1, 0);
  if (event == EVENT_BLOCK_LEGALITY)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (1 - player == affected_card_controller))
    {
      source_colors = get_color(1 - player, affected_card);
      required_color = get_sleighted_color_test(player, card, 4);
      if ((source_colors & required_color) != 0)
      {
        event_result = 1;
      }
      source_colors = get_color(affected_card_controller, affected_card);
      required_color = get_sleighted_color_test(player, card, 4);
      if (((source_colors & required_color) == 0) && (forbid_attack != 0))
      {
        event_result = 1;
      }
    }
  }
  if ((((trigger_condition != TRIGGER_PAY_TO_BLOCK) || (card != affected_card)) ||
       (player != affected_card_controller)) ||
      (1 - current_turn != reason_for_trigger_controller))
    return global_enchantment(player, card, event);
  source_colors = get_color(trigger_cause_controller, trigger_cause);
  required_color = get_sleighted_color_test(player, card, 4);
  if ((required_color & source_colors) != 0 || forbid_attack != 0)
    return global_enchantment(player, card, event);
  result = is_humiliated(player, card, AL_STATIC);
  if (result != 0)
    return global_enchantment(player, card, event);
  result = can_pay_life(1 - current_turn, 1);
  if (result == 0)
    return global_enchantment(player, card, event);
  if (event == EVENT_TRIGGER)
  {
    result = can_pay_life(1 - current_turn, 1);
    if (result != 0)
    {
      event_result = event_result | 2;
      if (forbid_attack == 0)
        return global_enchantment(player, card, event);
    }
    else
      forbid_attack = 1;
  }
  else if (event == EVENT_RESOLVE_TRIGGER)
  {

    result = do_dialog(1 - current_turn, player, card, -1, -1, " Pay 1 life to block\n Pass",
                       (unsigned int)(1 < life[1 - current_turn]));
    if (result != 1)
    {
      lose_life(1 - current_turn, 1);
      forbid_attack = 0;
      return global_enchantment(player, card, event);
    }
    forbid_attack = 1;
    spell_fizzled = 0;
  }
  else
  {
    if (forbid_attack == 0)
      return global_enchantment(player, card, event);
  }
  remove_state(trigger_cause_controller, trigger_cause, 0x8000);
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2021ad0; original cards/visions.c:4259. */
int card_honorable_passage(int player, int card, event_t event)
{
  /* Honorable Passage	|1|W
   * Instant
   * The next time a source of your choice would deal damage to any target this turn, prevent that damage. If damage from a red source is prevented this way, ~ deals that much damage to the source's controller. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.extra = damage_card;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      card_instance_t *instance = get_card_instance(player, card);
      card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
      int amount = prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
      if (dmg->internal_card_id == damage_card)
      {
        unsigned int clr = get_color(dmg->damage_source_player, dmg->damage_source_card);
        unsigned int required_color = get_sleighted_color_test(player, card, 0x10);
        if (clr & required_color)
          damage_player(dmg->damage_source_player, amount, player, card);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x101, &td, "TARGET_DAMAGE", 1, NULL);
}

/* ManalinkEh 2022: 0x2021cb0; original cards/visions.c:4293. */
int card_hope_charm(int player, int card, event_t event)
{
  /* Hope Charm	|W
   * Instant
   * Choose one -
   * * Target creature gains first strike until end of turn.
   * * Target player gains 2 life.
   * * Destroy target Aura. */

  enum
  {
    CHOICE_FIRST_STRIKE = 1,
    CHOICE_GAIN_LIFE = 2,
    CHOICE_AURA = 3
  };

  card_instance_t *instance;
  int32_t selected_mode;
  int available_or_mode;
  int life_available;
  int aura_available;
  target_definition_t td_aura;
  target_definition_t td_player;
  target_definition_t td_creature;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td_aura, 4);
    td_aura.required_subtype = 0x4000;
    default_target_definition(player, card, &td_player, 0);
    td_player.preferred_controller = player;
    td_player.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td_creature, 2);
    get_card_instance(player, card);
    available_or_mode = generic_spell(player, card, EVENT_CAN_CAST, 1, &td_creature, NULL, 1, NULL);
    if (((available_or_mode != 0) ||
         (available_or_mode =
              generic_spell(player, card, EVENT_CAN_CAST, 1, &td_player, NULL, 1, NULL),
          available_or_mode != 0)) ||
        (available_or_mode =
             generic_spell(player, card, EVENT_CAN_CAST, 1, &td_aura, NULL, 1, NULL),
         available_or_mode != 0))
    {
      return 1;
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td_aura, 4);
      td_aura.required_subtype = 0x4000;
      default_target_definition(player, card, &td_player, 0);
      td_player.preferred_controller = player;
      td_player.zone = TARGET_ZONE_PLAYERS;
      default_target_definition(player, card, &td_creature, 2);
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {

          available_or_mode =
              generic_spell(player, card, EVENT_CAN_CAST, 1, &td_creature, NULL, 1, NULL);
          life_available =
              generic_spell(player, card, EVENT_CAN_CAST, 1, &td_player, NULL, 1, NULL);
          aura_available = generic_spell(player, card, EVENT_CAN_CAST, 1, &td_aura, NULL, 1, NULL);
          if (dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, "Target gains first strike",
                          available_or_mode, (unsigned int)(current_phase == 0x18) * 9 + 1,
                          "Gain 2 life", life_available,
                          ((life[player] < 6) ? 20U : 2U), "Destroy Aura",
                          aura_available, 5, 0, 0, 0, 0) == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        selected_mode = instance->info_slot;
        if (selected_mode == CHOICE_FIRST_STRIKE)
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 1, &td_creature, "TARGET_CREATURE", 1,
                        NULL);
          selected_mode = instance->info_slot;
        }
        if (selected_mode == CHOICE_GAIN_LIFE)
        {
          generic_spell(player, card, EVENT_CAST_SPELL, 1, &td_player, "TARGET_PLAYER", 1, NULL);
          selected_mode = instance->info_slot;
        }
        if (selected_mode == CHOICE_AURA)
        {
          return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td_aura,
                               "Select target Aura enchantment.", 1, NULL);
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td_aura, 4);
    td_aura.required_subtype = 0x4000;
    default_target_definition(player, card, &td_player, 0);
    td_player.preferred_controller = player;
    td_player.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td_creature, 2);
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {

      available_or_mode = instance->info_slot;
      if (available_or_mode == CHOICE_GAIN_LIFE)
      {
        if (valid_target(&td_player) != 0)
        {
          gain_life(instance->targets[0].player, 2);
        }
      }
      else if (available_or_mode == CHOICE_AURA)
      {
        if (valid_target(&td_aura) != 0)
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      else if ((available_or_mode == CHOICE_FIRST_STRIKE) &&
               (available_or_mode = valid_target(&td_creature), available_or_mode != 0))
      {
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               0, 0, 0x100, 0);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2022360; original cards/visions.c:4398. */
int card_infernal_harvest(int player, int card, event_t event)
{
  /* Infernal Harvest	|1|B
   * Sorcery
   * As an additional cost to cast this spell, return X Swamps you control to their owner's hand.
   * ~ deals X damage divided as you choose among any number of target creatures. */

  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  int target_limit_or_selected;
  int controlled_swamps;
  card_instance_t *instance;
  target_definition_t td;
  target_definition_t td2;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.allow_cancel = 3;
    if (player == 1)
    {
      td.illegal_abilities = get_protections_from(1, card);
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (intptr_t)not_indestructible;
      default_target_definition(1, card, &td2, 1);
      td2.preferred_controller = 1;
    }
    else
    {
      default_target_definition(player, card, &td2, 1);
      td2.preferred_controller = player;
    }
    td2.allow_cancel = 3;
    td2.required_subtype = 0x5009;
    td2.illegal_abilities = 0;
    td2.allowed_controller = td2.preferred_controller;
    return basic_spell(player, card, EVENT_CAN_CAST);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
  }
  else if (event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
           event != EVENT_CHANGE_TARGET)
    return 0;
  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td, 2);
  td.allow_cancel = 3;
  if (player == 1)
  {
    td.allow_cancel = 3;
    td.illegal_abilities = get_protections_from(1, card);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)not_indestructible;
  }
  default_target_definition(player, card, &td2, 1);
  if (event == EVENT_CHANGE_TARGET && player != 1)
    return 0;
  td2.allowed_controller = player;
  td2.allow_cancel = 3;
  td2.required_subtype = 0x5009;
  td2.illegal_abilities = 0;
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      td2.preferred_controller = td2.allowed_controller;
      target_limit_or_selected = target_available(player, card, &td2);
      controlled_swamps = count_subtype(player, 1, get_hacked_subtype(player, card, 0x5009));
      if (controlled_swamps < target_limit_or_selected)
      {
        target_limit_or_selected = controlled_swamps;
      }
      target_limit_or_selected = pick_up_to_n_targets_noload(
          &td2, "Select an untapped Swamp to bounce.", target_limit_or_selected);
      if ((spell_fizzled != 1) && (0 < target_limit_or_selected))
      {
        i = 0;
        do
        {
          other_target_cursor = instance->targets + i;
          target_cursor = instance->targets + i;
          i = i + 1;
          bounce_permanent(target_cursor->player, other_target_cursor->card);
          instance->info_slot = target_limit_or_selected;
        } while (i < target_limit_or_selected);
      }
    }
  }
  else
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      td2.preferred_controller = td2.allowed_controller;
      if (0 < instance->info_slot)
      {
        target_and_divide_damage(player, card, &td, "TARGET_CREATURE", instance->info_slot);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2014020; original cards/visions.c:4455. */
static int check_for_spitting_drake(int player)
{
  card_instance_t *instance;
  int result;
  int card;

  if (active_cards_count[player] < 1)
  {
    return 0;
  }
  card = 0;
  while (
      ((instance = in_play(player, card),
        instance == NULL || (result = get_card_name(player, card), result != 0xade)) ||
       (result = check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED), result != 0)))
  {
    card = card + 1;
    if (active_cards_count[player] <= card)
    {
      return 0;
    }
  }
  return 1;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20227c0; original cards/visions.c:4467. */
int card_kyscu_drake(int player, int card, event_t event)
{
  /* Kyscu Drake	|3|G
   * Creature - Drake 2/2
   * Flying
   * |G: This creature gets +0/+1 until end of turn. Activate only once each turn.
   * Sacrifice this creature and a creature named Spitting Drake: Search your library for a card named Viashivan Dragon, put that card onto the battlefield, then shuffle. */

  enum
  {
    CHOICE_TOUGHNESS = 1,
    CHOICE_SAC_DRAKE = 2
  };

  card_instance_t *instance;
  int available_or_mode;
  int sacrifice_available;
  unsigned int drake_available_or_sacrifice;
  test_definition_t test;

  if (event == EVENT_CHECK_ABILITIES)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 0,
                                                       0, 0, 1, 0, 0, 0, NULL, NULL),
         available_or_mode != 0))
    {
      check_abilities_toughness = check_abilities_toughness + 1;
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 0, 0, 0, 1, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 0, 0, 0, 0, 0, 0, 0,
                                        NULL, NULL) == 0)
          {
            return 0;
          }
          if (check_for_spitting_drake(player) == 0)
          {
            return 0;
          }
        }
        return 1;
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->info_slot = 0;
      available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 0, 0,
                                                    0, 1, 0, 0, 0, NULL, NULL);
      sacrifice_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 0, 0, 0,
                                                      0, 0, 0, 0, NULL, NULL);
      drake_available_or_sacrifice = 0;
      if (sacrifice_available != 0)
      {
        sacrifice_available = check_for_spitting_drake(player);
        drake_available_or_sacrifice = (unsigned int)(sacrifice_available != 0);
      }
      available_or_mode = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Boost toughness",
                                      available_or_mode, 5, "Sac Spitting Drake",
                                      drake_available_or_sacrifice, 10, 0, 0, 0, 0);
      if (available_or_mode == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = available_or_mode;
      if (available_or_mode == CHOICE_TOUGHNESS)
      {
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x2000, 0, 0, 0, 1, 0, 0, 0,
                                         NULL, NULL);
      }
      if ((available_or_mode == CHOICE_SAC_DRAKE) &&
          (available_or_mode = generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0,
                                                         0, 0, 0, 0, NULL, NULL),
           available_or_mode != 0))
      {
        new_default_test_definition(&test, 0x47, "Select a Spitting Drake to sacrifice.");
        test.id = CARD_ID_SPITTING_DRAKE;
        drake_available_or_sacrifice = new_sacrifice(
            player, card, player, SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
        if (drake_available_or_sacrifice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        kill_card(player, drake_available_or_sacrifice >> 0x18, KILL_SACRIFICE);
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      available_or_mode = instance->info_slot;
      if (available_or_mode == 1)
      {
        pump_until_eot(player, instance->parent_card, player, instance->parent_card, 0, 1);
        available_or_mode = instance->info_slot;
      }
      if (available_or_mode == 2)
      {
        new_default_test_definition(&test, 0xff, "Select a card named Viashivan Dragon.");
        test.id = CARD_ID_VIASHIVAN_DRAGON;
        new_global_tutor(player, player, 1, 4, 0, 5, &test);
      }
    }
    else if (event == EVENT_CLEANUP)
    {
      instance = get_card_instance(player, card);
      instance->targets[2].player = 0;
      return 0;
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2022db0; original cards/visions.c:4551. */
int card_lichenthrope(int player, int card, event_t event)
{
  /* Lichenthrope	|3|G|G
   * Creature - Plant Fungus 5/5
   * If damage would be dealt to this creature, put that many -1/-1 counters on it instead.
   * At the beginning of your upkeep, remove a -1/-1 counter from this creature. */

  card_instance_t *instance;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {

      instance = damage_being_prevented(event);
      if (((instance != NULL) && (instance->damage_target_player == player)) &&
          (instance->damage_target_card == card))
      {
        if (0 < instance->info_slot)
        {

          add_counters(player, card, player, card, COUNTER_M1_M1, instance->info_slot);
          instance->info_slot = 0;
        }
      }
    }
  }
  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  remove_counters(player, card, player, card, COUNTER_M1_M1, 1);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2022ea0; original cards/visions.c:4577. */
int card_matopi_golem(int player, int card, event_t event)
{
  /* Matopi Golem	|5
   * Artifact Creature - Golem 3/3
   * |1: Regenerate this creature. When it regenerates this way, put a -1/-1 counter on it. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = can_regenerate(instance->parent_controller, instance->parent_card);
    if (result != 0)
    {
      regenerate_target(instance->parent_controller, instance->parent_card);
      add_counters(player, card, instance->parent_controller, instance->parent_card,
                               COUNTER_M1_M1, 1);
    }
  }
  result = generic_activated_ability(player, card, event, GAA_REGENERATION, 1, 0, 0, 0, 0, 0,
                                          0, NULL, NULL);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2022fb0; original cards/visions.c:4594. */
int card_mob_mentality(int player, int card, event_t event)
{
  /* Mob Mentality	|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has trample.
   * Whenever all non-Wall creatures you control attack, enchanted creature gets +X/+0 until end of turn, where X is the number of attacking creatures. */

  card_instance_t *aura_or_creature;
  int attacker_count;
  card_instance_t *source_or_creature;
  int eligible;
  int c;

  aura_or_creature = get_card_instance(player, card);
  if (event == EVENT_CHECK_ABILITIES)
  {
    if (((aura_or_creature->damage_target_card == affected_card) &&
         (affected_card_controller == aura_or_creature->damage_target_player)) &&
        ((aura_or_creature->damage_target_player == player &&
          (aura_or_creature = in_play(player, card), aura_or_creature != NULL))))
    {
      attacker_count = count_attackers(player);
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          aura_or_creature = in_play(player, c);
          if (((aura_or_creature != NULL) && (eligible = is_what(player, c, 2), eligible != 0)) &&
              ((eligible = is_attacking(player, c),
                eligible == 0 && (eligible = has_subtype(player, c, SUBTYPE_WALL), eligible == 0))))
          {
            return 0;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }
      check_abilities_power = check_abilities_power + attacker_count;
    }
    get_card_instance(player, card);
  }
  else
  {
    source_or_creature = get_card_instance(player, card);
    if ((-1 < source_or_creature->damage_target_player) && (event == EVENT_DECLARE_ATTACKERS))
    {
      attacker_count = count_attackers(player);
      if (0 < active_cards_count[player])
      {
        c = 0;
        do
        {
          source_or_creature = in_play(player, c);
          if ((((source_or_creature != NULL) &&
                (eligible = is_what(player, c, 2), eligible != 0)) &&
               (eligible = is_attacking(player, c), eligible == 0)) &&
              (eligible = has_subtype(player, c, SUBTYPE_WALL), eligible == 0))
          {
            return 0;
          }
          c = c + 1;
        } while (c < active_cards_count[player]);
      }
      pump_until_eot(player, card, (int)aura_or_creature->damage_target_player,
                     aura_or_creature->damage_target_card, attacker_count, 0);
    }
  }
  return generic_aura(player, card, event, player, 0, 0, 0x80, 0, 0, 0, 0);
}
#endif

/* ManalinkEh 2022: 0x20231f0; original cards/visions.c:4642. */
int card_mystic_veil(int player, int card, event_t event)
{
  /* Mystic Veil	|1|U
   * Enchantment - Aura
   * You may cast this spell as though it had flash. If you cast it any time a sorcery couldn't have been cast, the controller of the permanent it becomes sacrifices it at the beginning of the next cleanup step.
   * Enchant creature
   * Enchanted creature has shroud. (It can't be the target of spells or abilities.) */

  card_instance_t *instance;
  int result;
  bool end_step;

  if (event == EVENT_MODIFY_COST)
  {
    result = can_sorcery_be_played(player, EVENT_MODIFY_COST);
    if (result == 0)
    {
      infinite_casting_cost();
    }
  }
  else if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = can_sorcery_be_played(player, EVENT_CAST_SPELL);
    if (result == 0)
    {
      instance = get_card_instance(player, card);
      result = current_turn;
      end_step = current_phase == 0x1f;
      instance->targets[3].player = 0x8d;
      instance->targets[3].card = result;
      if (end_step)
      {
        instance->targets[3].card = 1 - result;
      }
      ai_modifier = ai_modifier + -0x10;
    }
  }
  instance = get_card_instance(player, card);
  if (instance->targets[3].player == 0x8d)
  {
    instance = get_card_instance(player, card);
    if ((instance->targets[3].card == current_turn) && (current_phase == 0x22))
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  return generic_aura(player, card, event, player, 0, 0, 0x20000, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2023370; original cards/visions.c:4682. */
int card_ogre_enforcer(int player, int card, event_t event)
{
  /* Ogre Enforcer	|3|R|R
   * Creature - Ogre 4/4
   * This creature can't be destroyed by lethal damage unless lethal damage dealt by a single source is marked on it. */

  card_instance_t *instance;
  int largest_damage;
  int current_toughness;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (is_humiliated(player, card, AL_TRIG) == 0)
    {

      instance = damage_being_dealt(event);
      if ((instance != NULL) && (instance->damage_target_card == card))
      {
        largest_damage = instance->info_slot;
        current_toughness = get_toughness(player, card);
        if (largest_damage < current_toughness)
        {
          instance->info_slot = 0;
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x20233f0; original cards/visions.c:4700. */
int card_ovinomancer(int player, int card, event_t event)
{
  /* Ovinomancer	|2|U
   * Creature - Human Wizard 0/1
   * When this creature enters, sacrifice it unless you return three basic lands you control to their owner's hand.
   * |T, Return this creature to its owner's hand: Destroy target creature. It can't be regenerated. That creature's controller creates a 0/1 green Sheep creature token. */

  int i;
  target_t *target_cursor;
  target_t *other_target_cursor;
  unsigned char target_count;
  int available_lands;
  card_instance_t *instance;
  target_definition_t *creature_target;

  target_definition_t td2;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    default_target_definition(player, card, &td2, 2);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 7, 0, 0, 0, 0, 0, 0, 0,
                                       &td2, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 0, 0, &td2,
                                    "Select target creature to bury.") == 0)
      {
        return 0;
      }
      bounce_permanent(player, card);
      return 0;
    }
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
    if (valid_target(&td2) == 0)
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_SHEEP, &token);
    token.t_player = instance->targets[0].player;
    generate_token(&token);
    return 0;
  }
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 1);
    td.required_subtype = 0x2000;
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
    bool lands_returned = false;
    available_lands = target_available(player, card, &td);
    if (2 < available_lands)
    {
      char msg[100];
      scnprintf(msg, 100, "Select a basic land return it to it\'s owner\'s hand");
      i = 0;
      do
      {
        if (new_pick_target(&td, msg, i, 0x80) == 0)
          break;
        other_target_cursor = instance->targets + i;
        target_cursor = instance->targets + i;
        i = i + 1;
        state_untargettable(target_cursor->player, other_target_cursor->card, 1);
      } while (i != 3);
      i = 0;
      if (instance->number_of_targets != '\0')
      {
        do
        {
          state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
          target_count = instance->number_of_targets;
          if (target_count == 3)
          {
            bounce_permanent(instance->targets[i].player, instance->targets[i].card);
            target_count = instance->number_of_targets;
          }
          i = i + 1;
        } while (i < (int)(unsigned int)target_count);
        if (target_count == 3)
          lands_returned = true;
      }
    }
    if (!lands_returned)
      kill_card(player, card, KILL_SACRIFICE);
    creature_target = &td2;
    default_target_definition(player, card, creature_target, 2);
    return 0;
  }
}

/* ManalinkEh 2022: 0x2014640; original cards/visions.c:4780. */
static int peace_talks_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    give_shroud_to_player(player, card, event);
    give_shroud_to_player(1 - player, card, event);
  }
  if (event == EVENT_ABILITIES)
  {
    result = is_what(affected_card_controller, affected_card, 0x47);
    if (result != 0)
    {
      special_abilities2(affected_card_controller, affected_card, EVENT_ABILITIES,
                         SP_KEYWORD2_GRANTED_PROT_MISC);
    }
  }
  else if (event == EVENT_PROT_MISC)
  {
    instance = get_card_instance(affected_card_controller, affected_card);
    if (instance != NULL)
    {
      result = is_what(affected_card_controller, affected_card, 0x47);
      if ((result != 0) && (((unsigned int)event_result >> 24 & 0xff) == '\x05'))
      {
        event_result = event_result | 1;
      }
    }
  }
  else
  {
    if (event == EVENT_BEGIN_TURN)
    {
      instance = get_card_instance(player, card);
      instance->targets[1].player = 0x42;
      return 0;
    }
    if (event == EVENT_CLEANUP)
    {
      instance = get_card_instance(player, card);
      if (instance->targets[1].player == 0x42)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2023780; original cards/visions.c:4806. */
int card_peace_talks(int player, int card, event_t event)
{
  /* Peace Talks	|1|W
   * Sorcery
   * This turn and next turn, creatures can't attack, and players and permanents can't be the targets of spells or activated abilities. */

  int result;

  if (event == EVENT_RESOLVE_SPELL)
  {
    nobody_can_attack_this_turn_and_next_turn(player, card, 2);
    result = create_legacy_effect(player, card, peace_talks_effect);
    alternate_legacy_text(1, player, result);
    kill_card(player, card, KILL_SACRIFICE);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2023810; original cards/visions.c:4820. */
int card_phyrexian_marauder(int player, int card, event_t event)
{
  /* Phyrexian Marauder	|X
   * Artifact Creature - Phyrexian Construct 0/0
   * This creature enters with X +1/+1 counters on it.
   * This creature can't block.
   * This creature can't attack unless you pay |1 for each +1/+1 counter on it. */

  card_instance_t *instance;
  int32_t number;
  int result_or_counters;
  int mana_available;

  instance = get_card_instance(player, card);
  if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    set_special_flags2(player, card, 0x20000);
    if (check_special_flags2(player, card, -0x80000000) == 0)
    {
      instance->info_slot = 0;
    }
    result_or_counters = played_for_free(player, card);
    if ((result_or_counters != 0) ||
        (result_or_counters = is_token(player, card), result_or_counters != 0))
    {
      instance->info_slot = 0;
    }
    else
    {
      charge_mana(player, COLOR_COLORLESS, -1);
      set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
    }
  }
  number = instance->info_slot;
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, number);
  cannot_block(player, card, event);
  if (((trigger_condition == TRIGGER_PAY_TO_ATTACK) && (current_phase == 0x15)) &&
      ((result_or_counters = is_humiliated(player, card, AL_STATIC),
        card == affected_card &&
            ((((player == affected_card_controller && (reason_for_trigger_controller == player)) &&
               (result_or_counters == 0 && forbid_attack == 0)) &&
              ((trigger_cause_controller == player && (trigger_cause == card))))))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {

      result_or_counters = count_counters(player, card, COUNTER_P1_P1);
      mana_available = has_mana(player, 7, result_or_counters);
      if ((mana_available == 0) ||
          (result_or_counters = charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                                            player, COLOR_ANY, result_or_counters),
           result_or_counters == 0))
      {
        forbid_attack = 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2023a50; original cards/visions.c:4866. */
int card_quirion_druid(int player, int card, event_t event)
{
  /* Quirion Druid	|2|G
   * Creature - Elf Druid 1/2
   * |G, |T: Target land becomes a 2/2 green creature that's still a land. (This effect lasts indefinitely.) */

  card_instance_t *instance;
  int result;
  const char *prompt;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 1);
    td.illegal_type = TYPE_CREATURE;
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 1);
    td.illegal_type = TYPE_CREATURE;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = get_sleighted_color_test(player, card, 8);
      land_animation2(instance->parent_controller, instance->parent_card,
                      instance->targets[0].player, instance->targets[0].card, 0, 2, 2, 0, 0, result,
                      NULL);
    }
  }
  prompt =
      get_sleighted_color_text(player, card, "Select target land it becomes a %s creature.", 3);
  result = generic_activated_ability(player, card, event, 7, 0, 0, 0, 1, 0, 0, 0, &td, prompt);
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2023bf0; original cards/visions.c:4892. */
int card_raging_gorilla(int player, int card, event_t event)
{
  /* Raging Gorilla	|2|R
   * Creature - Ape 2/3
   * Whenever this creature blocks or becomes blocked, it gets +2/-2 until end of turn. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      check_abilities_power = check_abilities_power + 2;
    }
  }
  else
  {
    if (((event == EVENT_DECLARE_BLOCKERS) &&
         (result = is_humiliated(player, card, AL_TRIG), result == 0)) &&
        (((instance = get_card_instance(player, card),
           (instance->state & 4) != 0 && (result = is_unblocked(player, card), result == 0)) ||
          ((current_turn != player &&
            (result = blocking(player, card, EVENT_DECLARE_BLOCKERS), result != 0))))))
    {
      pump_until_eot(player, card, player, card, 2, -2);
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x2023d00; original cards/visions.c:4919. */
int card_relic_ward(int player, int card, event_t event)
{
  /* Relic Ward	|1|W
   * Enchantment - Aura
   * You may cast this spell as though it had flash. If you cast it any time a sorcery couldn't have been cast, the controller of the permanent it becomes sacrifices it at the beginning of the next cleanup step.
   * Enchant artifact
   * Enchanted artifact has shroud. (It can't be the target of spells or abilities.) */

  card_instance_t *instance;
  int result;
  bool end_step;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    result = can_sorcery_be_played(player, EVENT_MODIFY_COST);
    if (result == 0)
    {
      infinite_casting_cost();
    }
  }
  else if ((((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
            (player == affected_card_controller)) &&
           (result = can_sorcery_be_played(player, EVENT_CAST_SPELL), result == 0))
  {

    instance = get_card_instance(player, card);
    result = current_turn;
    end_step = current_phase == 0x1f;
    instance->targets[3].player = 0x8d;
    instance->targets[3].card = result;
    if (end_step)
    {
      instance->targets[3].card = 1 - result;
    }
    ai_modifier = ai_modifier + -0x10;
  }
  instance = get_card_instance(player, card);
  if (((instance->targets[3].player == 0x8d) &&
       (instance = get_card_instance(player, card), instance->targets[3].card == current_turn)) &&
      (current_phase == 0x22))
  {
    kill_card(player, card, KILL_SACRIFICE);
  }
  instance = in_play(player, card);
  if (instance &&
      (instance = get_card_instance(player, card), instance->damage_target_player != -1) &&
      (instance = get_card_instance(player, card), event == EVENT_ABILITIES) &&
      instance->damage_target_card == affected_card &&
      instance->damage_target_player == affected_card_controller)
    event_result |= 0x20000;
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 0x40);
  return targeted_aura(player, card, event, &td, "TARGET_ARTIFACT");
}

/* ManalinkEh 2022: 0x2023fa0; original cards/visions.c:4967. */
int card_remedy(int player, int card, event_t event)
{
  /* Remedy	|1|W
   * Instant
   * Prevent the next 5 damage that would be dealt this turn to any number of targets, divided as you choose. */

  card_instance_t *instance;
  int target_selected;
  int i;
  card_instance_t *target_record;
  target_t *duplicate_cursor;
  int32_t card_00;
  int32_t player_00;
  target_t *target_cursor;
  int next_target;
  int prevent_amount;
  target_definition_t td;
  char prompt[200];

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    get_card_instance(player, card);
    if (can_target(&td) != 0)
    {
      return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0x102);
      td.preferred_controller = player;
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        i = 0;
        do
        {
          next_target = i + 1;
          if (ai_is_speculating == 1)
          {
            prompt[0] = '\0';
          }
          else
          {
            load_text(NULL, "TARGET_ANY");
            scnprintf(prompt, 200, "%s (%d of 5)", text_lines[0], next_target);
          }
          target_selected = can_target(&td);
          if (((target_selected != 0) &&
               (target_selected = pick_next_target_noload(&td, prompt), target_selected != 0)) &&
              (target_selected = instance->targets[i].card, target_selected != -1))
          {
            target_record = get_card_instance(instance->targets[i].player, target_selected);
            target_record->state = target_record->state | 0x200000;
          }
          i = next_target;
        } while (next_target != 5);
      }
    }
  }
  else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      if (valid_target(&td) != 0)
      {
        target_cursor = instance->targets;
        i = 0;
        do
        {
          target_cursor = target_cursor + 1;
          player_00 = target_cursor[-1].player;
          i = i + 1;
          if (player_00 != -1)
          {

            card_00 = target_cursor[-1].card;
            if (card_00 != -1)
            {
              target_record = get_card_instance(player_00, card_00);
              target_record->state = target_record->state & 0xffdfffff;
              player_00 = target_cursor[-1].player;
              card_00 = target_cursor[-1].card;
            }
            if (i < 5)
            {
              prevent_amount = 1;
              duplicate_cursor = target_cursor;
              for (; duplicate_cursor != instance->targets + 5; ++duplicate_cursor)
              {
                if (duplicate_cursor->player == player_00 && duplicate_cursor->card == card_00)
                {
                  duplicate_cursor->player = -1;
                  ++prevent_amount;
                  card_00 = target_cursor[-1].card;
                  player_00 = target_cursor[-1].player;
                }
              }
            }
            else
            {
              prevent_amount = 1;
            }
            prevent_the_next_n_damage(player, card, player_00, card_00, prevent_amount, 0, -1, -1);
          }
        } while (i != 5);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  else if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    get_card_instance(player, card);
  }
  return 0;
}

/* ManalinkEh 2022: 0x20147c0; original cards/visions.c:5039. */
static int not_playable_lands_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    instance->targets[1].player = 0;
  }
  instance = in_play(player, card);
  if (instance && !is_humiliated(player, card, AL_STATIC))
  {
    instance = get_card_instance(player, card);
    result = instance->targets[1].player;
    player_bits[result] = player_bits[result] | 8;
    instance = get_card_instance(player, card);
    if ((instance->targets[1].player == current_turn) && ((land_can_be_played & 1U) == 0))
    {
      land_can_be_played = land_can_be_played | 1;
    }
  }
  leaves_play(player, card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    instance = get_card_instance(player, card);
    result = total_playable_lands(instance->targets[1].player);
    if (lands_played < result)
    {
      land_can_be_played = land_can_be_played & 0xfffffffe;
      instance = get_card_instance(player, card);
      result = instance->targets[1].player;
      player_bits[result] = player_bits[result] & 0xfffffff7;
    }
  }
  instance = get_card_instance(player, card);
  result = eot_trigger(instance->targets[1].player, card, event);
  if (result == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2024370; original cards/visions.c:5066. */
int card_solfatara(int player, int card, event_t event)
{
  /* Solfatara	|2|R
   * Instant
   * Target player can't play lands this turn.
   * Draw a card at the beginning of the next turn's upkeep. */

  int result;
  card_instance_t *instance;
  card_instance_t *land_restriction;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.preferred_controller = 1 - player;
  td.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      result = create_legacy_effect(player, card, not_playable_lands_legacy);
      result = alternate_legacy_text(2, player, result);
      land_restriction = get_card_instance(instance->targets[0].player, result);
      land_restriction->targets[1].player = instance->targets[0].player;
      result = cantrip(player, card, 1);
      alternate_legacy_text(1, player, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x2014980; original cards/visions.c:5097. */
static int fx_milled_power(int player, int card, event_t event)
{
  card_instance_t *effect = get_card_instance(player, card);
  int power = effect->targets[0].card;
  int count = declare_attackers_trigger(player, card, event, DAT_TRACK, ANYBODY, -1);
  if (count)
  {
    const uint8_t *attackers = (const uint8_t *)&effect->targets + 2 * sizeof(target_t);
    for (int index = count - 1; index >= 0; --index)
      if (in_play(current_turn, attackers[index]))
      {
        int legacy = pump_until_eot(player, card, current_turn, attackers[index], power, 0);
        alternate_legacy_text(1, player, legacy);
      }
  }
  if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2014350; original cards/visions.c:5115. */
static int fx_when_creature_attacks_it_gains_p1(int player, int card, event_t event)
{
  int i;
  xtrigger_t milled_trigger;
  card_instance_t *instance;
  int created_effect;
  int creatures_milled;

  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_MILLED;
  milled_trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    milled_trigger = xtrigger_impl_value_dont_use_directly;
  }
  if ((((milled_trigger == XTRIGGER_MILLED) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((reason_for_trigger_controller == player && (trigger_cause_controller == player))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {

      if (num_cards_milled < 1)
      {
        return 0;
      }
      creatures_milled = 0;
      for (i = 0; i < num_cards_milled; ++i)
        if (cards_milled[i].source != -1 &&
            is_what(-1, cards_milled[i].internal_card_id, TYPE_CREATURE))
          ++creatures_milled;
      if (creatures_milled == 0)
      {
        return 0;
      }

      created_effect = create_legacy_effect(player, card, fx_milled_power);
      instance = get_card_instance(player, created_effect);
      instance->targets[0].card = creatures_milled;
      instance = get_card_instance(player, created_effect);
      instance->token_status = instance->token_status | 0x10000;
      return 0;
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x2024530; original cards/visions.c:5144. */
int card_song_of_blood(int player, int card, event_t event)
{
  /* Song of Blood	|1|R
   * Sorcery
   * Mill four cards. Whenever a creature attacks this turn, it gets +1/+0 until end of turn for each creature card put into your graveyard this way. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = create_legacy_effect(player, card, fx_when_creature_attacks_it_gains_p1);
    result = alternate_legacy_text(1, player, result);
    instance = get_card_instance(player, result);
    instance->targets[0].player = player;
    mill(player, 4);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x20245c0; original cards/visions.c:5160. */
int card_spider_climb(int player, int card, event_t event)
{
  /* Spider Climb	|G
   * Enchantment - Aura
   * You may cast this spell as though it had flash. If you cast it any time a sorcery couldn't have been cast, the controller of the permanent it becomes sacrifices it at the beginning of the next cleanup step.
   * Enchant creature
   * Enchanted creature gets +0/+3 and has reach. (It can block creatures with flying.) */

  card_instance_t *instance;
  int result;
  bool end_step;

  if (event == EVENT_MODIFY_COST)
  {
    result = can_sorcery_be_played(player, EVENT_MODIFY_COST);
    if (result == 0)
    {
      infinite_casting_cost();
    }
  }
  else if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = can_sorcery_be_played(player, EVENT_CAST_SPELL);
    if (result == 0)
    {
      instance = get_card_instance(player, card);
      result = current_turn;
      end_step = current_phase == 0x1f;
      instance->targets[3].player = 0x8d;
      instance->targets[3].card = result;
      if (end_step)
      {
        instance->targets[3].card = 1 - result;
      }
      ai_modifier = ai_modifier + -0x10;
    }
  }
  instance = get_card_instance(player, card);
  if (instance->targets[3].player == 0x8d)
  {
    instance = get_card_instance(player, card);
    if ((instance->targets[3].card == current_turn) && (current_phase == 0x22))
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  return generic_aura(player, card, event, player, 0, 3, 0x400, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2024740; original cards/visions.c:5194. */
int card_sun_clasp(int player, int card, event_t event)
{
  /* Sun Clasp	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+3.
   * |W: Return enchanted creature to its owner's hand. */

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
          return generic_aura(player, card, event, player, 1, 3, 0, 0, 0, 0, 0);
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          bounce_permanent((int)instance->damage_target_player, instance->damage_target_card);
        }
      }
      return generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 1, 0, NULL, NULL);
    }
  }
  return generic_aura(player, card, event, player, 1, 3, 0, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x2013ff0; original cards/visions.c:5216. */
static void negate_first_strike(int player, int card, int t_player, int t_card)
{
  negate_ability_until_eot(player, card, t_player, t_card, KEYWORD_FIRST_STRIKE);
  return;
}

/* ManalinkEh 2022: 0x2024870; original cards/visions.c:5220. */
int card_talruum_champion(int player, int card, event_t event)
{
  /* Talruum Champion	|4|R
   * Creature - Minotaur 3/3
   * First strike
   * Whenever this creature blocks or becomes blocked by a creature, that creature loses first strike until end of turn. */

  card_instance_t *instance;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {

      instance = get_card_instance(player, card);
      if ((current_turn == player) && ((instance->state & 4) != 0))
      {
        for_each_creature_blocking_me(current_turn, card, negate_first_strike, current_turn, card);
      }
      if ((1 - current_turn == player) && ((instance->state & 8) != 0))
      {
        for_each_creature_blocked_by_me(player, card, negate_first_strike, player, card);
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2024930; original cards/visions.c:5238. */
int card_talruum_piper(int player, int card, event_t event)
{
  /* Talruum Piper	|4|R
   * Creature - Minotaur 3/3
   * All creatures with flying able to block this creature do so. */

  ability_must_block_me(player, card, event, 0x20, NULL);
  return 0;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2024970; original cards/visions.c:5251. */
int card_time_and_tide(int player, int card, event_t event)
{
  /* Time and Tide	|U|U
   * Instant
   * Simultaneously, all phased-out creatures phase in and all creatures with phasing phase out. */

  bool phased_any;
  int c;
  card_instance_t *instance;
  int turn_or_result;
  unsigned int phase_state;
  int scan_or_other_player;
  c = current_turn;
  if (event == EVENT_RESOLVE_SPELL)
  {
    phased_any = false;
    for (scan_or_other_player = active_cards_count[current_turn] - 1; scan_or_other_player >= 0;
         --scan_or_other_player)
    {
      instance = in_play(c, scan_or_other_player);
      if (((instance != NULL) &&
           (turn_or_result = is_what(c, scan_or_other_player, 2), turn_or_result != 0)) &&
          (instance = get_card_instance(c, scan_or_other_player), -1 < instance->internal_card_id))
      {
        phase_state = instance->state & 0x800022;
        if (phase_state == 0x800002)
        {
          phase_in_impl(c, scan_or_other_player, 0x4000000, 1);
          phased_any = true;
        }
        else if (phase_state == 2)
        {
          phase_out_impl(c, scan_or_other_player, 0x4000000, 1);
          phased_any = true;
        }
      }
    }
    scan_or_other_player = 1 - current_turn;
    c = active_cards_count[scan_or_other_player];
    while (turn_or_result = current_turn, c = c - 1, c >= 0)
    {
      instance = in_play(scan_or_other_player, c);
      if (((instance != NULL) &&
           (turn_or_result = is_what(scan_or_other_player, c, 2), turn_or_result != 0)) &&
          (instance = get_card_instance(scan_or_other_player, c), -1 < instance->internal_card_id))
      {
        phase_state = instance->state & 0x800022;
        if (phase_state == 0x800002)
        {
          phase_in_impl(scan_or_other_player, c, 0x4000000, 1);
          phased_any = true;
        }
        else if (phase_state == 2)
        {
          phase_out_impl(scan_or_other_player, c, 0x4000000, 1);
          phased_any = true;
        }
      }
    }
    if (phased_any)
    {
      c = active_cards_count[current_turn] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(turn_or_result, c);
          if (((instance != NULL) &&
               (scan_or_other_player = is_what(turn_or_result, c, 2), scan_or_other_player != 0)) &&
              (instance = get_card_instance(turn_or_result, c), -1 < instance->internal_card_id))
          {
            phase_state = instance->state & 0x800022;
            if (phase_state == 0x800002)
            {
              phase_in_impl(turn_or_result, c, 0xc000000, 0);
            }
            else
            {
              if ((phase_state == 2) &&
                  (scan_or_other_player = check_special_flags(turn_or_result, c, 0xc000000),
                   scan_or_other_player != 0))
              {
                phase_out_impl(turn_or_result, c, scan_or_other_player, 0);
              }
            }
          }
          c = c + -1;
        } while (c != -1);
      }
      c = 1 - current_turn;
      scan_or_other_player = active_cards_count[c] + -1;
      if (-1 < scan_or_other_player)
      {
        do
        {
          instance = in_play(c, scan_or_other_player);
          if (((instance != NULL) &&
               (turn_or_result = is_what(c, scan_or_other_player, 2), turn_or_result != 0)) &&
              (instance = get_card_instance(c, scan_or_other_player),
               -1 < instance->internal_card_id))
          {
            phase_state = instance->state & 0x800022;
            if (phase_state == 0x800002)
            {
              phase_in_impl(c, scan_or_other_player, 0xc000000, 0);
            }
            else
            {
              if ((phase_state == 2) &&
                  (turn_or_result = check_special_flags(c, scan_or_other_player, 0xc000000),
                   turn_or_result != 0))
              {
                phase_out_impl(c, scan_or_other_player, turn_or_result, 0);
              }
            }
          }
          scan_or_other_player = scan_or_other_player + -1;
        } while (scan_or_other_player != -1);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}
#endif

/* ManalinkEh 2022: 0x2024d20; original cards/visions.c:5317. */
int card_triangle_of_war(int player, int card, event_t event)
{
  /* Triangle of War	|1
   * Artifact
   * |2, Sacrifice this artifact: Target creature you control fights target creature an opponent controls. (Each deals damage equal to its power to the other.) */

  card_instance_t *instance;
  int result;
  target_definition_t *opponent_target;
  target_definition_t td;
  target_definition_t td1;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = 1 - player;
    td1.preferred_controller = td1.allowed_controller;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xc, 2, 0, 0, 0, 0, 0, 0,
                                         &td, NULL);
      if (result == 0)
      {
        return 0;
      }
      result = can_target(&td1);
      if (result == 0)
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 8, 2, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
      if (result == 0)
      {
        return 0;
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      result = new_pick_target(&td, "Select target creature you control.", 0, 0x81);
      if (result == 0)
      {
        return 0;
      }
      new_pick_target(&td1, "Select target creature opponent controls.", 1, 0x81);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = player;
    td.preferred_controller = player;
    default_target_definition(player, card, &td1, 2);
    td1.allowed_controller = 1 - player;
    td1.preferred_controller = td1.allowed_controller;
    instance = get_card_instance(player, card);
  }
  opponent_target = &td1;
  if (((event == EVENT_RESOLVE_ACTIVATION) &&
       (result = validate_target(player, card, &td, 0), result != 0)) &&
      (result = validate_target(player, card, opponent_target, 1), result != 0))
  {
    fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player,
          instance->targets[1].card);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2025030; original cards/visions.c:5362. */
int card_urborg_mindsucker(int player, int card, event_t event)
{
  /* Urborg Mindsucker	|2|B
   * Creature - Horror 2/2
   * |B, Sacrifice this creature: Target opponent discards a card at random. Activate only as a sorcery. */

  int result;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    discard(1 - player, 1, player);
  }
  result = generic_activated_ability(
      player, card, event,
      (GAA_SACRIFICE_ME | GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_ONLY_TARGET_OPPONENT), 0, 1, 0, 0, 0,
      0, 0, &td, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2025160; original cards/visions.c:5386. */
int card_vampirism(int player, int card, event_t event)
{
  /* Vampirism	|1|B
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, draw a card at the beginning of the next turn's upkeep.
   * Enchanted creature gets +1/+1 for each other creature you control.
   * Other creatures you control get -1/-1. */

  card_instance_t *instance;
  int attached_card;
  int card_00;
  target_definition_t td;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    cantrip(player, card, 1);
  instance = in_play(player, card);
  if (instance && !is_humiliated(player, card, AL_STATIC))
  {
    instance = get_card_instance(player, card);
    attached_card = instance->damage_target_card;

    if ((((-1 < instance->damage_target_player) && ((uint32_t)(event - EVENT_POWER) < 2)) &&
         (attached_card == affected_card)) &&
        ((instance->damage_target_player == affected_card_controller &&
          (0 < active_cards_count[player]))))
    {
      card_00 = 0;
      do
      {
        instance = in_play(player, card_00);
        if (((attached_card != card_00) && (instance != NULL)) &&
            (is_what(player, card_00, 2) != 0))
        {
          event_result = event_result + 1;
        }
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[player]);
    }
    boost_creature_type(player, attached_card, event, ~SUBTYPE_INVALID, -1, -1, 0,
                        BCT_CONTROLLER_ONLY);
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 2);
  return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

/* ManalinkEh 2022: 0x2014a90; original cards/visions.c:5428. */
static int vision_charm_landchange(int player, int card, event_t event)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  change_lands_into_new_land_type(player, card, event, instance->targets[1].player, 0,
                                  instance->targets[1].card);
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20253c0; original cards/visions.c:5438. */
int card_vision_charm(int player, int card, event_t event)
{
  /* Vision Charm	|U
   * Instant
   * Choose one -
   * * Target player mills four cards.
   * * Choose a land type and a basic land type. Each land of the first chosen type becomes the second chosen type until end of turn.
   * * Target artifact phases out. (While it's phased out, it's treated as though it doesn't exist. It phases in before its controller untaps during their next untap step.) */

  enum
  {
    CHOICE_MILL = 1,
    CHOICE_LAND_CHANGE = 2,
    CHOICE_PHASE_OUT = 3
  };

  int choice;
  card_instance_t *instance;
  int mode_or_landtype;
  int mill_available_or_old_subtype;
  card_instance_t *charm;
  int phase_available;
  int phase_priority;
  static int land_subtypes[13] = {20481, 20482, 20486, 20487, 20489, 20480, 20492,
                                        20483, 20484, 20485, 20488, 20490, 20491};
  static int basic_land_subtypes[5] = {20489, 20482, 20481, 20486, 20487};
  target_definition_t td;
  target_definition_t td3;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td3, 0x40);
    td3.preferred_controller = player;
    get_card_instance(player, card);
    mode_or_landtype = generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    if ((mode_or_landtype == 0) &&
        (mode_or_landtype = generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL),
         mode_or_landtype == 0))
    {
      return 0;
    }
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 0);
      td.zone = TARGET_ZONE_PLAYERS;
      default_target_definition(player, card, &td3, 0x40);
      td3.preferred_controller = player;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        if (check_special_flags2(player, card, -0x80000000) == 0)
        {
          mill_available_or_old_subtype =
              generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
          phase_available = generic_spell(player, card, EVENT_CAN_CAST, 1, &td3, NULL, 1, NULL);
          phase_priority = check_battlefield_for_subtype(1 - player, 1, 0x5009);
          mode_or_landtype = 1;
          phase_priority = ((phase_priority == 0) ? 1U : 10U);
          if (0x16 < current_phase)
          {
            phase_priority = 1;
            mode_or_landtype = (unsigned int)(0x18 < current_phase) * 9 + 1;
          }
          if (dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, "Target player mills",
                          mill_available_or_old_subtype, 5, "Change landtype", 1, mode_or_landtype,
                          "Target artifact phase out", phase_available, phase_priority, 0, 0, 0,
                          0) == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
        }
        if (instance->info_slot == CHOICE_MILL)
        {
          mode_or_landtype =
              generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_PLAYER", 1, NULL);
        }
        else
        {
          if (instance->info_slot != CHOICE_PHASE_OUT)
          {
            return 0;
          }
          mode_or_landtype = generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td3,
                                           "Select target artifact it phases out.", 1, NULL);
        }
        return mode_or_landtype;
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    default_target_definition(player, card, &td3, 0x40);
    td3.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      mode_or_landtype = instance->info_slot;
      if (mode_or_landtype == CHOICE_LAND_CHANGE)
      {
        instance = get_card_instance(player, card);
        mode_or_landtype = get_deck_color(player, 1 - player);
        choice = do_dialog(player, player, card, -1, -1,
                           " Forest\n Island\n Mountain\n Plains\n Swamp\n Desert\n Gate\n Lair\n "
                           "Locus\n Mine\n Power-Plant\n Tower\n Urza\'s",
                           mode_or_landtype + -1);
        instance->targets[0].player = 1 << ((unsigned char)choice & 0x1f);
        charm = get_card_instance(player, card);
        mode_or_landtype =
            do_dialog(player, player, card, -1, -1, " Swamp\n Island\n Forest\n Mountain\n Plains",
                      mode_or_landtype + -1);
        mode_or_landtype = 1 << ((unsigned char)mode_or_landtype & 0x1f);
        charm->targets[0].card = mode_or_landtype;
        mode_or_landtype = basic_land_subtypes[mode_or_landtype];
        mill_available_or_old_subtype = land_subtypes[instance->targets[0].player];
        instance =
            get_card_instance(player, create_legacy_effect(player, card, vision_charm_landchange));
        instance->targets[1].player = mill_available_or_old_subtype;
        instance->targets[1].card = mode_or_landtype;
      }
      else if (mode_or_landtype == CHOICE_PHASE_OUT)
      {
        if (valid_target(&td3) != 0)
        {
          phase_out(instance->targets[0].player, instance->targets[0].card);
        }
      }
      else if ((mode_or_landtype == CHOICE_MILL) &&
               (mode_or_landtype = valid_target(&td), mode_or_landtype != 0))
      {
        mill(instance->targets[0].player, 4);
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x20259c0; original cards/visions.c:5572. */
int card_wake_of_voltures(int player, int card, event_t event)
{
  /* Wake of Vultures	|3|B
   * Creature - Bird 3/1
   * Flying
   * |1|B, Sacrifice a creature: Regenerate this creature. */

  int result;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test, 2, "Select a creature to sacrifice.");
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
                                                                GAA_NONE, 1, 1, 0, 0, 0, 0, 0, NULL,
                                                                NULL, TYPE_NONE, &test, SARV_0);
      if (result != 0)
      {
        if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
        {
          test.not_me = 1;
        }
        result = can_regenerate(player, card);
        return result;
      }
    }
    else
    {
      result = charge_mana_for_activated_ability(player, card, 1, 1, 0, 0, 0, 0);
      if (result != 0)
      {
        state_untargettable(player, card, 1);
        result = new_sacrifice(player, card, player, SAC_AS_COST, &test);
        if (result == 0)
        {
          spell_fizzled = 1;
        }
        state_untargettable(player, card, 0);
      }
    }
  }
  else if (((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP)) &&
           (new_default_test_definition(&test, 2, "Select a creature to sacrifice."),
            event == EVENT_RESOLVE_ACTIVATION))
  {
    result = regeneration(player, card, EVENT_RESOLVE_ACTIVATION, 0, 0, 0, 0, 0, 0);
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2025c20; original cards/visions.c:5609. */
int card_wicked_reward(int player, int card, event_t event)
{
  /* Wicked Reward	|1|B
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * Target creature gets +4/+2 until end of turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CHECK_PUMP)
  {
    instance = get_card_instance(player, card);
    result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id);
    if (((result != 0) && (result = count_subtype(player, 2, -1), 1 < result)) &&
        (result = can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE), result != 0))
    {
      pumpable_power[player] = pumpable_power[player] + 4;
      pumpable_toughness[player] = pumpable_toughness[player] + 2;
    }
    return 0;
  }
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
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        instance = get_card_instance(player, card);
        if (event == EVENT_RESOLVE_SPELL)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 4,
                           2);
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return generic_spell(player, card, event, 0x401, &td, "TARGET_CREATURE", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  get_card_instance(player, card);
  return generic_spell(player, card, event, 0x401, &td, "TARGET_CREATURE", 1, NULL);
}
