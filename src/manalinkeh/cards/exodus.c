#include "manalink.h"

//----- GLOBAL FUNCTION ----

#ifndef SHANDALAR

int card_furnace_brood(int player, int card, event_t event);
int card_medicine_bag(int player, int card, event_t event);

#endif

static int generic_type_recycler_creature(int player, int card, event_t event,
                                          test_definition_t *this_test)
{
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    strcpy(this_test->message, "Select target ");
    if (ai_is_speculating != 1)
    {
      int l = strlen(this_test->message);
      type_text(this_test->message + l, 100 - l, this_test->type, 0);
      l = strlen(this_test->message);
      scnprintf(this_test->message + l, 100 - l, " card.");
    }
    if (new_special_count_grave(player, this_test) > 0 && !graveyard_has_shroud(player))
      new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, this_test);
  }
  return 0;
}

/* Shared in October 2022 (COFF external); also used by newer sets. */
void pump_when_blocked(int player, int card, event_t event, int pow_plus, int tou_plus, int ability){

	if( event == EVENT_DECLARE_BLOCKERS && current_turn == player && is_attacking(player, card) ){
		int count = 0;
		while(count < active_cards_count[1-player]){
				if(in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) ){
					card_instance_t *instance = get_card_instance( 1-player, count);
					if( instance->blocking == card ){
						pump_ability_until_eot(player, card, player, card, pow_plus, tou_plus, ability, 0);
					}
				}
				count++;
		}
	}
}

static int exodus_keepers(int player, int card, event_t event, int color){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	int c[6] = {0, 0, 0, 0, 0, 0};
	c[color] = 1;

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_ONLY_TARGET_OPPONENT | GAA_UNTAPPED, c[0], c[1], c[2], c[3], c[4], c[5], 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, c[0], c[1], c[2], c[3], c[4], c[5]) ){
			instance->targets[0].player = 1-player;
			instance->targets[0].card = -1;
			instance->number_of_targets = 1;
		}
	}

	return 0;
}

//---- CARDS ----

int card_allay(int player, int card, event_t event)
{
  /* Allay	|1|W
   * Instant
   * Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Destroy target enchantment. */

  // Buyback |3
  // Destroy target enchantment.

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST || event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_ENCHANTMENT") ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if (instance->info_slot){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_anarchist(int player, int card, event_t event)
{
  /* Anarchist	|4|R
   * Creature - Human Wizard 2/2
   * When this creature enters, you may return target sorcery card from your graveyard to your hand. */

  test_definition_t this_test;
  default_test_definition(&this_test, TYPE_SORCERY);
  this_test.type_flag = F1_NO_CREATURE;
  return generic_type_recycler_creature(player, card, event, &this_test);
}

int card_carnophage(int player, int card, event_t event){
  /* Carnophage	|B
   * Creature - Zombie 2/2
   * At the beginning of your upkeep, tap this creature unless you pay 1 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( ! is_tapped(player, card) ){
			int to_tap = 1;
			if( can_pay_life(player, 1) ){
				int ai_choice = 0;
				if(life[player]-1 < 6){
					ai_choice = 1;
				}

				int choice = do_dialog(player, player, card, -1, -1, " Pay 1 life\n Tap this", ai_choice);
				if( choice == 0){
					lose_life(player, 1);
					to_tap = 0;
				}
			}

			if( to_tap == 1 ){
				tap_card(player, card);
			}
		}
	}

	return 0;
}

int card_cartographer(int player, int card, event_t event)
{
  /* Cartographer	|2|G
   * Creature - Human 2/2
   * When this creature enters, you may return target land card from your graveyard to your hand. */

  test_definition_t this_test;
  default_test_definition(&this_test, TYPE_LAND);
  return generic_type_recycler_creature(player, card, event, &this_test);
}

int not_marked(int iid, int markedptr, int player, int card)
{
  char (*marked)[2][151] = (char(*)[2][151])markedptr;
  return !(*marked)[player][card];
}

int card_cataclysm(int player, int card, event_t event){
  /* Cataclysm	|2|W|W
   * Sorcery
   * Each player chooses from among the permanents they control an artifact, a creature, an enchantment, and a land, then sacrifices the rest. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
			char marked[2][151] = {{0}};
			APNAP(p, {
				target_definition_t td;
				default_target_definition(player, card, &td, 0);
				td.allowed_controller = p;
				td.preferred_controller = p;
				td.who_chooses = p;
				td.allow_cancel = 0;
				td.illegal_abilities = 0;

				type_t types[] = { TYPE_ARTIFACT, TYPE_CREATURE, TYPE_ENCHANTMENT, TYPE_LAND };
				const char* prompts[] = { "TARGET_ARTIFACT", "TARGET_CREATURE", "TARGET_ENCHANTMENT", "TARGET_LAND" };

				int i;
				for (i = 0; i < 4; ++i){
					td.required_type = types[i];
					if (can_target(&td) && pick_target(&td, prompts[i])){
						marked[instance->targets[0].player][instance->targets[0].card] = 1;
						card_instance_t* aff = get_card_instance(instance->targets[0].player, instance->targets[0].card);

						// It's legal to choose the same permanent for more than one type, but don't let the AI
						aff->state |= STATE_TARGETTED;
						if (p == AI || ai_is_speculating == 1){
							aff->state |= STATE_CANNOT_TARGET;
						}
					}
				}

				int c;
				for (c = 0; c < active_cards_count[p]; ++c){
					if (marked[p][c]){
						// In particular, don't leave the cannot-target markers in place while sacrificing, or they interfere with leave-play triggers
						get_card_instance(p, c)->state &= ~(STATE_TARGETTED | STATE_CANNOT_TARGET);
					}
				}
			});
			APNAP(p, {
				if( can_sacrifice(player, p, 1, TYPE_PERMANENT, 0) ){
					test_definition_t this_test;
					default_test_definition(&this_test, TYPE_PERMANENT);
					this_test.special_selection_function = &not_marked;
					this_test.value_for_special_selection_function = (int)(marked);
					new_manipulate_all(player, card, p, &this_test, KILL_SACRIFICE);
				}
			});
			kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_city_of_traitors(int player, int card, event_t event){
  /* City of Traitors	""
   * Land
   * When you play another land, sacrifice this land.
   * |T: Add |C|C. */

	if( ! is_unlocked(player, card, event, 2) ){ return 0; }

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( whenever_a_player_plays_a_land(player, card, event, player, RESOLVE_TRIGGER_MANDATORY) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return two_mana_land(player, card, event, COLOR_COLORLESS, COLOR_COLORLESS);
}

int card_coat_of_arms(int player, int card, event_t event){
  /* Coat of Arms	|5
   * Artifact
   * Each creature gets +1/+1 for each other creature on the battlefield that shares at least one creature type with it. */

	// original code : 004057C0



	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
		in_play(player, card) && in_play(affected_card_controller, affected_card) && !is_humiliated(player, card, AL_STATIC) )
	{
		event_result += count_all_other_creatures_that_shares_a_creature_type_with_me(affected_card_controller, affected_card, ANYBODY);
	}

	return 0;
}

int card_convalescence(int player, int card, event_t event){
  /* Convalescence	|1|W
   * Enchantment
   * At the beginning of your upkeep, if you have 10 or less life, you gain 1 life. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( life[player] < 10 ){
			gain_life(player, 1);
		}
	}

	return global_enchantment(player, card, event);
}

int card_culling_the_weak(int player, int card, event_t event){
  /* Culling the Weak	|B
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * Add |B|B|B|B. */

	if( event == EVENT_RESOLVE_SPELL ){
		produce_mana(player, COLOR_BLACK, 4);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
}

int card_curiosity(int player, int card, event_t event)
{
  /* Curiosity	|U
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature deals damage to an opponent, you may draw a card. */

  // original code : 0x405D70



  int packets;
  if ((packets = attached_creature_deals_damage(player, card, event, DDBM_MUST_DAMAGE_OPPONENT|DDBM_TRIGGER_OPTIONAL)))
	draw_cards(player, packets);

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS)
    return vanilla_aura(player, card, event, player);
  return 0;
}

int card_dauthi_jackal(int player, int card, event_t event){
  /* Dauthi Jackal	|2|B
   * Creature - Dauthi Jackal 2/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * |B|B, Sacrifice this creature: Destroy target blocking creature. */

	shadow(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.required_state = TARGET_STATE_BLOCKING;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME+GAA_CAN_TARGET, MANACOST_B(2), 0, &td1, "TARGET_CREATURE");
}

int card_dauthi_warlord(int player, int card, event_t event)
{
  /* Dauthi Warlord	|1|B
   * Creature - Dauthi Soldier * /1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * ~'s power is equal to the number of creatures on the battlefield with shadow. */

  shadow(player, card, event);
  if (event == EVENT_POWER && card == affected_card && player == affected_card_controller)
  {
    int pt = 0;
    for (int p = 0; p < 2; ++p)
    {
      for (int count = 0; count < active_cards_count[p]; ++count)
      {
        if (in_play(p, count))
          pt += check_for_special_ability(p, count, SP_KEYWORD_SHADOW) != 0;
      }
    }
    set_characteristic_pt(pt);
  }
  return 0;
}

int card_deaths_duet(int player, int card, event_t event)
{
  /* Death's Duet	|2|B
   * Sorcery
   * Return two target creature cards from your graveyard to your hand. */

  if (event == EVENT_CAN_CAST)
	return count_graveyard_by_type(player, TYPE_CREATURE) >= 2 && !graveyard_has_shroud(player);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select two target creature cards.");

	  card_instance_t* instance = get_card_instance(player, card);
	  select_multiple_cards_from_graveyard(player, player, -1, AI_MAX_VALUE, &test, 2, &instance->targets[0]);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int i, num_validated = 0, selected;
	  for (i = 0; i < 2; ++i)
		if ((selected = validate_target_from_grave(player, card, player, i)) != -1)
		  {
			from_grave_to_hand(player, selected, TUTOR_HAND);
			++num_validated;
		  }

	  if (num_validated == 0)
		spell_fizzled = 1;

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_dominating_licid(int player, int card, event_t event)
{
  /* Dominating Licid	|1|U|U
   * Creature - Licid 1/1
   * |1|U|U, |T: This creature loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |U to end this effect.
   * You control enchanted creature. */

  card_data_t *created_type;
  card_instance_t *instance;
  int result;
  card_instance_t *parent;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE && card == affected_card && player == affected_card_controller &&
      (result = instance->targets[2].card, result > 0))
    event_result = result;
  result = is_what(player, card, 2);
  if (result == 0)
  {
    leaves_play(player, card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      result = instance->targets[3].card;
      if (result >= 0)
        kill_card(instance->targets[3].player, result, KILL_EXILE);
      end_of_stealing_aura_effect(player, card);
    }
    bool aura_event = ((event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CAN_MOVE_AURA) ||
                      ((uint32_t)(event - EVENT_POWER) < 3) || event == EVENT_RESOLVE_MOVING_AURA ||
                      event == EVENT_CHANGE_TYPE || event == EVENT_SET_COLOR ||
                      event == EVENT_GET_SELECTED_CARD || event == EVENT_STATIC_EFFECTS ||
                      event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ||
                      event == EVENT_RESOLVE_ACTIVATION || event == EVENT_CLEANUP;
    if (event != EVENT_LEAVES_PLAY_ABILITY &&
        (event == EVENT_CAN_CAST || event == EVENT_CHANGE_TARGET || event == EVENT_RESOLVE_SPELL ||
         event == EVENT_CAN_CHANGE_TARGET ||
         (event == EVENT_CAST_SPELL && card == affected_card &&
          player == affected_card_controller)))
      aura_event = true;
    if (!aura_event)
      return 0;
    default_target_definition(player, card, &td, 2);
    if (instance->damage_target_player >= 0)
    {
      if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
        return generic_activated_ability(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
      if (event == EVENT_RESOLVE_ACTIVATION)
      {
        parent = get_card_instance(instance->parent_controller, instance->parent_card);
        purge_add_force_a_subtype_permanently_legacy(player, card, 1);
        end_of_stealing_aura_effect(instance->parent_controller, instance->parent_card);
        parent->targets[3].card = -1;
        parent->damage_target_player = -1;
        parent->damage_target_card = -1;
        parent->targets[2].card = -1;
        result = instance->targets[3].card;
        if (-1 < result)
        {
          kill_card(instance->targets[3].player, result, KILL_EXILE);
        }
      }
      return generic_stealing_aura(player, card, event, &td, "TARGET_CREATURE");
    }
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    return generic_activated_ability(player, card, event, 5, 1, 0, 2, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  }
  if (event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    parent = get_card_instance(instance->parent_controller, instance->parent_card);
    result = create_a_card_type(parent->internal_card_id);
    created_type = cards_at_7c7000[result];
    created_type->type = '\x04';
    created_type->power = 0;
    created_type->toughness = 0;
    parent->targets[2].card = result;
    add_a_subtype_permanently(player, card, instance->parent_controller, instance->parent_card,
                              SUBTYPE_AURA_CREATURE);
    gain_control_and_attach_as_aura(instance->parent_controller, instance->parent_card,
                                    EVENT_RESOLVE_ACTIVATION, instance->targets[0].player,
                                    instance->targets[0].card);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2643f00; original cards/exodus.c:488. */
int card_elvish_berserker(int player, int card, event_t event)
{
  /* Elvish Berserker	|G
   * Creature - Elf Berserker 1/1
   * Whenever this creature becomes blocked, it gets +1/+1 until end of turn for each creature blocking it. */

  if ((((event == EVENT_CHECK_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((current_turn == player && (current_phase < 0x18))))
  {
    check_abilities_power = check_abilities_power + 1;
    check_abilities_toughness = check_abilities_toughness + 1;
  }
  pump_when_blocked(player, card, event, 1, 1, 0);
  return 0;
}
#endif

int card_ephemeron(int player, int card, event_t event){
  /* Ephemeron	|4|U|U
   * Creature - Illusion 4/4
   * Flying
   * Discard a card: Return this creature to its owner's hand. */

	// Used externally
	card_instance_t *instance = get_card_instance(player, card);
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( in_play(instance->parent_controller, instance->parent_card) ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_equilibrium(int player, int card, event_t event){
  /* Equilibrium	|1|U|U
   * Enchantment
   * Whenever you cast a creature spell, you may pay |1. If you do, return target creature to its owner's hand. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		card_instance_t* instance = get_card_instance(player, card);
		if (has_mana(player, COLOR_ANY, 1) && can_target(&td) && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1) && pick_target(&td, "TARGET_CREATURE")){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

int card_erratic_portal(int player, int card, event_t event){
  /* Erratic Portal	|4
   * Artifact
   * |1, |T: Return target creature to its owner's hand unless its controller pays |1. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int bounce = 1;
			if( has_mana(instance->targets[0].player, COLOR_COLORLESS, 1) ){
				int ai_choice = instance->targets[0].player != player ? 0 : 1;
				int choice = do_dialog(instance->targets[0].player, player, card, -1, -1, " Pay 1\n Pass", ai_choice);
				if( ! choice ){
					charge_mana(instance->targets[0].player, COLOR_COLORLESS, 1);
					if( spell_fizzled != 1 ){
						bounce = 0;
					}
				}
			}
			if( bounce == 1 ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

int card_ertai_wizard_adept(int player, int card, event_t event){
  /* Ertai, Wizard Adept	|2|U
   * Legendary Creature - Human Wizard 1/1
   * |2|U|U, |T: Counter target spell. */

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_NONSICK | GAA_SPELL_ON_STACK, MANACOST_XU(2, 2), 0, &td, NULL);
}

int card_exalted_dragon(int player, int card, event_t event){
  /* Exalted Dragon	|4|W|W
   * Creature - Dragon 5/5
   * Flying
   * This creature can't attack unless you sacrifice a land. (This cost is paid as attackers are declared.) */

	if( trigger_condition == TRIGGER_PAY_TO_ATTACK && current_phase == PHASE_DECLARE_ATTACKERS && ! is_humiliated(player, card, AL_STATIC) &&
		affect_me( player, card ) && reason_for_trigger_controller == player && forbid_attack == 0 &&
		trigger_cause_controller == player && trigger_cause == card )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
				if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					forbid_attack = 1;
				}
		}
	}

	return 0;
}

int card_flowstone_flood(int player, int card, event_t event){
  /* Flowstone Flood	|3|R
   * Sorcery
   * Buyback-Pay 3 life, Discard a card at random. (You may pay 3 life and discard a card at random in addition to any other costs as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Destroy target land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_LAND") ){
			if( ! is_token(player, card) && can_pay_life(player, 3) && hand_count[player] > 0 ){
				int choice = do_dialog(player, player, card, -1, -1, " Play normally\n Buyback", 1);
				if( choice == 1 ){
					lose_life(player, 3);
					discard(player, DISC_RANDOM, player);
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_forbid(int player, int card, event_t event){
  /* Forbid	|1|U|U
   * Instant
   * Buyback-Discard two cards. (You may discard two cards in addition to any other costs as you cast this spell. If you do, put this card into your hand as it resolves.)
   * Counter target spell. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		counterspell(player, card, event, NULL, 0);
		if( spell_fizzled != 1 && ! is_token(player, card) && hand_count[player] >= 2 ){
			int choice = do_dialog(player, player, card, -1, -1, " Play normally\n Buyback", 0);
			instance->info_slot = choice;
			if( choice == 1){
				multidiscard(player, 2, 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_hatred(int player, int card, event_t event){
  /* Hatred	|3|B|B
   * Instant
   * As an additional cost to cast this spell, pay X life.
   * Target creature gets +X/+0 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_pay_life_as_cost_for_spells_or_activated_abilities(player, 1) &&
				generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			int to_pay = 0;
			if( player == AI ){
				to_pay = life[1-player]-get_attack_power(instance->targets[0].player, instance->targets[0].card);
			}
			else{
				 to_pay = choose_a_number(player, "Pay how much life?", life[player]);
			}

			if( to_pay > life[player] ){
				to_pay = life[player]-1;
			}

			if( to_pay < 1 ){
				spell_fizzled = 1;
			}
			else{
				 lose_life(player, to_pay);
				 instance->info_slot = to_pay;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, instance->info_slot, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_high_ground(int player, int card, event_t event)
{
  /* High Ground	|W
   * Enchantment
   * Each creature you control can block an additional creature each combat. */

  if (affected_card_controller == player)
	arbitrary_can_block_additional(event, 1);

  return global_enchantment(player, card, event);
}

int card_jackalope_herd(int player, int card, event_t event)
{
  /* Jackalope Herd	|3|G
   * Creature - Rabbit Beast 4/5
   * When you cast a spell, return this creature to its owner's hand. */

  sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
    bounce_permanent(player, card);
  return 0;
}

int card_keeper_of_the_beasts(int player, int card, event_t event){
  /* Keeper of the Beasts English |G|G
   * 	  Creature - Human Wizard 1/2
   * 	  {G}, {T}: Choose target opponent who controlled more creatures than you did as you activated this ability.
   * 	  Put a 2/2 green Beast creature token onto the battlefield. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_GREEN) && count_subtype(1-player, TYPE_CREATURE, -1) > count_subtype(player, TYPE_CREATURE, -1) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_GREEN);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_BEAST, &token);
			token.pow = 2;
			token.tou = 2;
			generate_token(&token);
		}
	}

	return 0;
}

int card_keeper_of_the_dead(int player, int card, event_t event){
  /* Keeper of the Dead |B|B
   * 	  Creature - Human Wizard 1/2
   * 	  {B}, {T}: Choose target opponent who had at least two fewer creature cards in his or her graveyard than you did as you activated this ability.
   * 	  Destroy target nonblack creature he or she controls. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	td1.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_BLACK) ){
			return count_graveyard_by_type(player, TYPE_CREATURE)-count_graveyard_by_type(1-player, TYPE_CREATURE) > 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_BLACK);
		if( spell_fizzled != 1 ){
			if( new_pick_target(&td1, get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, 1 | GS_LITERAL_PROMPT) ){
				tap_card(player, card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && validate_target(player, card, &td1, 1) ){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_keeper_of_the_flame(int player, int card, event_t event){
  /* Keeper of the Flame |R|R
   * 	  Creature - Human Wizard 1/2
   * 	  {R}, {T}: Choose target opponent who had more life than you did as you activated this ability. Keeper of the Flame deals 2 damage to him or her. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_RED) ){
			return life[1-player] > life[player];
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_RED);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_keeper_of_the_light(int player, int card, event_t event){
  /* Keeper of the Light |W|W
   * 	  Creature - Human Wizard 1/2
   * 	  {W}, {T}: Choose target opponent who had more life than you did as you activated this ability. You gain 3 life. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_WHITE) ){
			return life[player] < life[1-player];
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_WHITE);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			gain_life(player, 3);
		}
	}

	return 0;
}

int card_keeper_of_the_mind(int player, int card, event_t event){
  /* Keeper of the Mind |U|U
   * 	  Creature - Human Wizard 1/2
   * 	  {U}, {T}: Choose target opponent who had at least two more cards in hand than you did as you activated this ability. Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_CAN_ACTIVATE ){
		if( exodus_keepers(player, card, event, COLOR_BLUE) && (hand_count[1-player]-hand_count[player]) > 1){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		exodus_keepers(player, card, event, COLOR_BLUE);
		if( spell_fizzled != 1 ){
			tap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_mana_breach(int player, int card, event_t event){
  /* Mana Breach	|2|U
   * Enchantment
   * Whenever a player casts a spell, that player returns a land they control to its owner's hand. */

	sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.illegal_abilities = 0;
		td.allow_cancel = 0;
		td.allowed_controller = td.preferred_controller = td.who_chooses = trigger_cause_controller;
		card_instance_t* instance = get_card_instance(player, card);
		if (can_target(&td) && new_pick_target(&td, "Select a land you control to return to your hand.", 0, GS_LITERAL_PROMPT)){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

int card_manabond(int player, int card, event_t event){
  /* Manabond	|G
   * Enchantment
   * At the beginning of your end step, you may reveal your hand and put all land cards from it onto the battlefield. If you do, discard your hand. */

	if( ! is_unlocked(player, card, event, 13) ){ return 0; }

	if( eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) ){
		reveal_target_player_hand(player);
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_hand(p, c) && is_what(p, c, TYPE_LAND) ){
										put_into_play(p, c);
									}
								};
		);
	}

	return global_enchantment(player, card, event);
}

int card_memory_crystal(int player, int card, event_t event){
  /* Memory Crystal	|3
   * Artifact
   * Buyback costs cost |2 less. */

	return 0;
}

int card_merfolk_looter(int player, int card, event_t event)
{
  /* Merfolk Looter	|1|U
   * Creature - Merfolk Rogue 1/1
   * |T: Draw a card, then discard a card. */

  // 0x1202CB8


  // Used externally
  if (!IS_GAA_EVENT(event))
	return 0;

  if (event == EVENT_ACTIVATE && IS_AI(player) && player == AI)
	{
	  // Same logic as Jalum Tome.
	  int c, total_lands = 0;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if ((in_play(player, c) || in_hand(player, c)) && is_what(player, c, TYPE_LAND))
		  ++total_lands;

	  if (total_lands <= 2 || total_lands >= 8)
		ai_modifier += 48;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  draw_a_card(player);
	  discard(player, 0, player);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_mind_maggots(int player, int card, event_t event){
  /* Mind Maggots	|3|B
   * Creature - Insect 2/2
   * When this creature enters, discard any number of creature cards. For each card discarded this way, put two +1/+1 counters on this creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( hand_count[player] ){
			char msg[100] = "Select a creature card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, msg);
			this_test.zone = TARGET_ZONE_HAND;

			if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
				int hand_copy[2][hand_count[player]];
				int hcc = 0;
				int discarded[hand_count[player]];
				int dc = 0;
				CYCLE_ALL_CARDS(p, c, 	{
											if( p == player && in_hand(p, c) ){
												hand_copy[0][hcc] = get_original_internal_card_id(p, c);
												hand_copy[1][hcc] = c;
												hcc++;
											}
										}
				);
				while( hcc ){
						int selected = select_card_from_zone(player, player, hand_copy[0], hcc, 0, AI_MIN_VALUE, -1, &this_test);
						if( selected != -1 ){
							discarded[dc] = hand_copy[1][selected];
							dc++;
						}
						else{
							break;
						}
						if( player == AI && dc == 3 ){
							break;
						}
				}
				int i;
				for(i=0; i<dc; i++){
					new_discard_card(player, discarded[i], 0, player);
					add_counters(player, card, player, card, COUNTER_P1_P1, 2);
				}
			}
		}
	}

	return 0;
}

int card_mind_over_matter(int player, int card, event_t event){
  /* Mind Over Matter	|2|U|U|U|U
   * Enchantment
   * Discard a card: You may tap or untap target artifact, creature, or land. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	char msg[100] = "Select a card to discard.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, msg);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_LAND | TYPE_ARTIFACT);

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST0, 0, &td, "TWIDDLE");
		ai_modifier_twiddle(player, card, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( validate_target(player, card, &td, 0) ){
			twiddle(player, card, 0);
		}
	}

	return 0;
}

int card_mindless_automaton(int player, int card, event_t event){
  /* Mindless Automaton |4
   * Artifact Creature — Construct 0/0
   * Mindless Automaton enters the battlefield with two +1/+1 counters on it.
   * {1}, Discard a card: Put a +1/+1 counter on Mindless Automaton.
   * Remove two +1/+1 counters from Mindless Automaton: Draw a card. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int max_pump = hand_count[player];
		while( ! has_mana_for_activated_ability(player, card, MANACOST_X(max_pump)) ){
				max_pump--;
		}
		return max_pump;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_DISCARD = 1,
		CHOICE_DRAW
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL) ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 2), NULL, NULL) ){
			return 1;
		}

	}
	if( event == EVENT_ACTIVATE ){
		int abilities[3] = {
							0,
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL),
							generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 2), NULL, NULL)
		};

		int choice = DIALOG(player, card, EVENT_CAST_SPELL, DLG_RANDOM,
							"Discard & pump", 	abilities[1], (current_phase == PHASE_AFTER_BLOCKING && hand_count[player]) ? 10 : 1,
							"Draw a card", 		abilities[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DISCARD ){
			return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_X(1), 0, NULL, NULL);
		}
		if( choice == CHOICE_DRAW ){
			return generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTERS(COUNTER_P1_P1, 2), NULL, NULL);
	    }
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DISCARD ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 1);
		}
	}


	return 0;
}

int card_mirri_cat_warrior(int player, int card, event_t event)
{
  /* Mirri, Cat Warrior	|1|G|G
   * Legendary Creature - Cat Warrior 2/3
   * First strike, |H2forestwalk, vigilance */

  check_legend_rule(player, card, event);
  vigilance(player, card, event);
  return 0;
}

int card_nausea(int player, int card, event_t event){
  /* Mirri, Cat Warrior	|1|G|G
   * Legendary Creature - Cat Warrior 2/3
   * First strike, |H2forestwalk, vigilance */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, ANYBODY, -1, -1, -1, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_null_brooch(int player, int card, event_t event){
/* CARD_ID_NULL_BROOCH	3521
Null Brooch	|4
Artifact
|2, |T, Discard your hand: Counter target noncreature spell. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SPELL_ON_STACK, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if(	charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			add_state(player, card, STATE_TAPPED);
			tapped_for_mana_color = -1;
			discard_all(player);
			instance->targets[0].player = card_on_stack_controller;
			instance->targets[0].card = card_on_stack;
			instance->number_of_targets = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_oath_of_druids(int player, int card, event_t event){
  /* Oath of Druids |1|G
   * Enchantment
   * At the beginning of each player's upkeep, that player chooses target player who controls more creatures than he or she does and is his
   * or her opponent. The first player may reveal cards from the top of his or her library until he or she reveals a creature card.
   * If he or she does, that player puts that card onto the battlefield and all other cards revealed this way into his or her graveyard. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_subtype(current_turn, TYPE_CREATURE, -1) < count_subtype(1-current_turn, TYPE_CREATURE, -1) ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				int choice = DIALOG(current_turn, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Oath of Druids", 1, count_subtype(current_turn, TYPE_CREATURE, -1) == 0 ? 10 : 1,
									"Decline", 1, count_deck_by_type(current_turn, TYPE_CREATURE) > 0 ? 1 : 100);
				if( choice == 1 ){
					test_definition_t test;
					default_test_definition(&test, TYPE_CREATURE);
					reveal_card_until_you_reveal_test(current_turn, current_turn, &test, RCUYRT_MILL_REST | RCUYRT_PUT_TEST_INTO_PLAY);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_ghouls(int player, int card, event_t event){
/* CARD_ID_OATH_OF_GHOULS	3523
Oath of Ghouls	|1|B
Enchantment
At the beginning of each player's upkeep, that player chooses target player whose graveyard has fewer creature cards in it than
his or her graveyard does and is his or her opponent. The first player may return a creature card from his or her graveyard to his or her hand. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( count_graveyard_by_type(current_turn, TYPE_CREATURE) > count_graveyard_by_type(1-current_turn, TYPE_CREATURE) ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
				new_global_tutor(current_turn, current_turn, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_lieges(int player, int card, event_t event){
/* CARD_ID_OATH_OF_LIEGES	3524
Oath of Lieges	|1|W
Enchantment
At the beginning of each player's upkeep, that player chooses target player who controls more lands than he or she does and is his or her opponent.
The first player may search his or her library for a basic land card, put that card onto the battlefield, then shuffle his or her library. */

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[1].card = 1;
	}

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( landsofcolor_controlled[current_turn][COLOR_ANY] < landsofcolor_controlled[1-current_turn][COLOR_ANY] ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				int prev_found = get_card_instance(player, card)->targets[1].card;
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Oath of Lieges", 1, prev_found >= 0 ? 10 : 1,
									"Decline", 1, 5);
				if( choice == 1 ){
					test_definition_t this_test;
					new_default_test_definition(&this_test, TYPE_LAND, "Select a basic land card.");
					this_test.subtype = SUBTYPE_BASIC;
					int found = new_global_tutor(current_turn, current_turn, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_FIRST_FOUND, &this_test);
					get_card_instance(player, card)->targets[1].card = found;
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_scholars(int player, int card, event_t event){
/* CARD_ID_OATH_OF_SCHOLARS	3526
Oath of Scholars	|3|U
Enchantment
At the beginning of each player's upkeep, that player chooses target player who has more cards in hand than he or she does and is his or her opponent.
The first player may discard his or her hand and draw three cards. */

	upkeep_trigger_ability(player, card, event, ANYBODY);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		if( hand_count[current_turn] < hand_count[1-current_turn] ){
			int trig = 1;
			if( player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF) ){
				trig = 0;
			}
			if( trig ){
				int priority_draw = hand_count[player] < 3 && count_deck(player) >= 20 ? 10 : 1;
				int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Activate Oath of Scholars", 1, priority_draw,
									"Decline", 1, 5);
				if( choice == 1 ){
					new_discard_all(current_turn, player);
					draw_cards(current_turn, 3);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_oath_of_mages(int player, int card, event_t event){
/* CARD_ID_OATH_OF_MAGES	3525
Oath of Mages	|1|R
Enchantment
At the beginning of each player's upkeep, that player chooses target player who has more life than he or she does and is his or her opponent.
The first player may have ~ deal 1 damage to the second player. */

	upkeep_trigger_ability(player, card, event, ANYBODY);
	if (event == EVENT_UPKEEP_TRIGGER_ABILITY && life[current_turn] < life[1-current_turn] && !(player_bits[1-current_turn] & (PB_PLAYER_HAS_SHROUD | PB_PLAYER_HAS_HEXPROOF))){
		if (DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_CANCEL, DLG_NO_STORAGE, DLG_WHO_CHOOSES(current_turn), "Activate Oath of Mages", 1, 50, "Decline", 1, 2) == 1){
			damage_player(1-current_turn, 1, player, card);
		}
	}
	return global_enchantment(player, card, event);
}

int card_ogre_shaman(int player, int card, event_t event){
	/* Ogre Shaman	|3|R|R	0x20036bc
	 * Creature - Ogre Shaman 3/3
	 * |2, Discard a card at random: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) && pick_target(&td, "TARGET_ANY") ){
			discard(player, DISC_RANDOM, player);
		}
	}

   if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
   }

	return 0;
}

int card_onslaught(int player, int card, event_t event){
/* CARD_ID_ONSLAUGHT	3528
Onslaught	|R
Enchantment
Whenever you cast a creature spell, tap target creature. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);
		instance->number_of_targets = 0;

		if( can_target(&td) && new_pick_target(&td, "Select target creature to tap.", 0, GS_LITERAL_PROMPT) ){
			action_on_target(player, card, 0, ACT_TAP);
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_paladin_en_vec(int player, int card, event_t event){
  /* Paladin en-Vec |1|W|W
   * 	  Creature - Human Knight 2/2
   * 	  First strike, protection from black and from red */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		protection_from_black(player, card, event);
		protection_from_red(player, card, event);
	}

	return 0;
}

void pandemonium_effect(int player, int card){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.who_chooses = player;

	card_instance_t* instance = get_card_instance(player, card);
	int pow = get_power(player, card);
	instance->number_of_targets = 0;

	if( pow > 0 && can_target(&td) && pick_target(&td, "TARGET_ANY") ){
		damage_target0(player, card, pow);
	}
	instance->number_of_targets = 0;
}

int card_pandemonium(int player, int card, event_t event){
	/* Pandemonium	|3|R	0x2002ab4
	 * Enchantment
	 * Whenever a creature enters the battlefield, that creature's controller may have it deal damage equal to its power to any target of their choice. */
	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);

	if( event == EVENT_ETB_ABILITY ){
		pandemonium_effect(trigger_cause_controller, trigger_cause);
	}

	return global_enchantment(player, card, event);
}

int card_peace_of_mind(int player, int card, event_t event){
/* CARD_ID_PEACE_OF_MIND	3531
Peace of Mind	|1|W
Enchantment
|W, Discard a card: You gain 3 life. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 3);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_W(1), 0, NULL, NULL);
}

int card_pegasus_stampede(int player, int card, event_t event){
	/* Pegasus Stampede	|1|W
	 * Sorcery
	 * Buyback-Sacrifice a land.
	 * Put a 1/1 |Swhite Pegasus creature token with flying onto the battlefield. */

	 if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_token(player, card) && can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ){
			int choice = DIALOG(player, card, event, DLG_NO_CANCEL, DLG_NO_STORAGE,
								"Pay Buyback cost", 1, count_subtype(player, TYPE_LAND, -1) >= 6 ? 10 : 1,
								"Decline", 1, 5);
			if( choice == 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
				if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		generate_token_by_id(player, card, CARD_ID_PEGASUS);
		if( instance->info_slot == 1 ){
			bounce_permanent(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pit_spawn(int player, int card, event_t event){
/* CARD_ID_PIT_SPAWN	3534
Pit Spawn	|4|B|B|B
Creature - Demon 6/4
First strike
At the beginning of your upkeep, sacrifice ~ unless you pay |B|B.
Whenever ~ deals damage to a creature, exile that creature. */

	if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)){
		get_card_instance(player, card)->destroys_if_blocked |= DIFB_DESTROYS_UNPROTECTED;
	}

	basic_upkeep(player, card, event, MANACOST_B(2));

	rfg_when_damage(player, card, event);

	return 0;
}

static int target_for_plaguebearer(int player, int card, int mode){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance= get_card_instance(player, card);

	int i;
	int trg = -1;
	int par = -1;
	for(i=0; i<2; i++){
		if( i == 1-player || player == HUMAN ){
			int count = 0;
			while( count < active_cards_count[i] ){
					if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
						int cost = get_cmc(i, count)*2;
						if( has_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
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

int card_plaguebearer(int player, int card, event_t event){
  /* Plaguebearer English |1|B
   * 	  Creature - Zombie 1/1
   * 	  {X}{X}{B}: Destroy target nonblack creature with converted mana cost X. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
		return target_for_plaguebearer(player, card, 0);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		int op = -1;
		int oc = -1;
		if( player == HUMAN ){
			if( select_target(player, card, &td, "Select target nonblack creature.", NULL) ){
				op = instance->targets[0].player;
				oc = instance->targets[0].card;
				int cost = 2*get_cmc( instance->targets[0].player, instance->targets[0].card );
				if( ! has_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
					spell_fizzled = 1;
					return 0;
				}
				if( charge_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
					instance->targets[0].player = op;
					instance->targets[0].card = oc;
					instance->number_of_targets = 1;
				}
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		else{
			int result = target_for_plaguebearer(player, card, 1);
			if( result != -1 ){
				int cost = 2*get_cmc( 1-player, result );
				if( charge_mana_for_activated_ability(player, card, MANACOST_XB(cost, 1)) ){
					instance->targets[0].player = 1-player;
					instance->targets[0].card = result;
					instance->number_of_targets = 1;
				}
			}
			else{
				spell_fizzled = 1;
				return 0;
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

int card_price_of_progress(int player, int card, event_t event){
/* CARD_ID_PRICE_OF_PROGRESS	3538
Price of Progress	|1|R
Instant
~ deals damage to each player equal to twice the number of nonbasic lands that player controls. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.subtype = SUBTYPE_BASIC;
		this_test.subtype_flag = DOESNT_MATCH;
		APNAP(p,
				{
					int amount = 2*check_battlefield_for_special_card(player, card, p, CBFSC_GET_COUNT, &this_test);
					if( amount ){
						damage_player(p, amount, player, card);
					}
				}
		);
		kill_card(player, card, KILL_BURY);
	}

	return basic_spell(player, card, event);
}

int card_pygmy_troll(int player, int card, event_t event){
/* CARD_ID_PYGMY_TROLL	3539
Pygmy Troll	|1|G
Creature - Troll 1/1
Whenever ~ becomes blocked by a creature, ~ gets +1/+1 until end of turn.
|G: Regenerate ~. */

	pump_when_blocked(player, card, event, 1, 1, 0);

	return card_wall_of_brambles(player, card, event);
}

int card_ravenous_baboons(int player, int card, event_t event){
/* CARD_ID_RAVENOUS_BABOONS	3541
Ravenous Baboons	|3|R
Creature - Monkey 2/2
When ~ enters the battlefield, destroy target nonbasic land. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.required_subtype = SUBTYPE_BASIC;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target non-Basic land.", 0, GS_LITERAL_PROMPT) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_reaping_the_rewards(int player, int card, event_t event){
/* CARD_ID_REAPING_THE_REWARDS	3542
Reaping the Rewards	|W
Instant
Buyback-Sacrifice a land. (You may sacrifice a land in addition to any other costs as you cast this spell.
If you do, put this card into your hand as it resolves.) You gain 2 life. */

	 if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_token(player, card) && can_sacrifice_type_as_cost(player, 1, TYPE_LAND) ){
			int choice = DIALOG(player, card, event, DLG_NO_CANCEL, DLG_NO_STORAGE,
								"Pay Buyback cost", 1, count_subtype(player, TYPE_LAND, -1) >= 6 ? 10 : 1,
								"Decline", 1, 5);
			if( choice == 1 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
				if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		gain_life(player, 2);
		if( instance->info_slot == 1 ){
			bounce_permanent(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_reclaim(int player, int card, event_t event){
/* CARD_ID_RECLAIM	3544
Reclaim	|G
Instant
Put target card from your graveyard on top of your library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on top.");

	if( event == EVENT_RESOLVE_SPELL){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			const int *grave = get_grave(player);
			int card_added = add_card_to_hand(player, grave[selected]);
			remove_card_from_grave(player, selected);
			put_on_top_of_deck(player, card_added);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

/* ManalinkEh 2022: 0x2647ee0; original cards/exodus.c:1895. */
int card_reconnaissance(int player, int card, event_t event)
{
  /* Reconnaissance	|W
   * Enchantment
   * |0: Remove target attacking creature you control from combat and untap it. (If you activate during end of combat, the creature will untap after it deals combat damage.) */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    td.allowed_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = can_target(&td);
      if (result != 0)
      {
        result = valid_target(&td);
        if (result != 0)
        {
          remove_state(instance->targets[0].player, instance->targets[0].card, 0x44);
          untap_card(instance->targets[0].player, instance->targets[0].card);
        }
      }
    }
  }
  result = generic_activated_ability(player, card, event, 4, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_ATTACKING_CREATURE");
  return result;
}

int card_recurring_nightmare(int player, int card, event_t event){
  /* Recurring Nightmare	|2|B
   * Enchantment
   * Sacrifice a creature, Return this enchantment to its owner's hand: Return target creature card from your graveyard to the battlefield. Activate only as a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	if(event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event,
															GAA_CAN_SORCERY_BE_PLAYED | GAA_BOUNCE_ME | GAA_SACRIFICE_CREATURE, MANACOST0, 0,
															NULL, NULL, 0, &test, GS_GRAVE_RECYCLER);
}

int card_rootwater_mystic(int player, int card, event_t event){
  /* Rootwater Mystic	|U
   * Creature - Merfolk Wizard 1/1
   * |1|U: Look at the top card of target player's library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int *deck = deck_ptr[instance->targets[0].player];
			if( deck[0] != -1 ){
				show_deck( player, deck, 1, "Here's the first card of deck.", 0, gs_done_008b40e0 );
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td, "TARGET_PLAYER");
}

int card_scalding_salamander(int player, int card, event_t event)
{
/* CARD_ID_SCALDING_SALAMANDER	3552
Scalding Salamander	|2|R
Creature - Salamander 2/1
Whenever ~ attacks, you may have it deal 1 damage to each creature without flying defending player controls. */

  dat_ability(player, card, event, duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.keyword = KEYWORD_FLYING;
	  test.keyword_flag = DOESNT_MATCH;
	  new_damage_all(player, card, 1-current_turn, 1, 0, &test);
	}

  return 0;
}

int card_scare_tactics(int player, int card, event_t event){
/* CARD_ID_SCARE_TACTICS	3553
Scare Tactics	|B
Instant
Creatures you control get +1/+0 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL){
		pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, 0);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_school_of_piranha(int player, int card, event_t event){
/* CARD_ID_SCHOOL_OF_PIRANHA	3554
School of Piranha	|1|U
Creature - Fish 3/3
At the beginning of your upkeep, sacrifice ~ unless you pay |1|U. */

	basic_upkeep(player, card, event, MANACOST_XU(1, 1));

	return 0;
}

int card_srivener(int player, int card, event_t event)
{
  /* CARD_ID_SCRIVENER	3555
  Scrivener	|4|U
  Creature - Human Wizard 2/2
  When ~ enters the battlefield, you may return target instant card from your graveyard to your
  hand. */

  test_definition_t this_test;
  default_test_definition(&this_test, TYPE_INSTANT | TYPE_INTERRUPT);
  this_test.type_flag = F1_NO_CREATURE;
  return generic_type_recycler_creature(player, card, event, &this_test);
}

int card_seismic_assault(int player, int card, event_t event){
	/* Seismic Assault	|R|R|R	0x200a192
	 * Enchantment
	 * Discard a land card: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to discard.");
	this_test.zone = TARGET_ZONE_HAND;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td1, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			instance->number_of_targets = 0;
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				if( pick_target(&td1, "TARGET_ANY") ){
					instance->number_of_targets = 1;
					discard_card(player, selected);
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_target0(player, card, 2);
		}
	}

	return 0;
}

int card_shackles(int player, int card, event_t event){
/* CARD_ID_SHACKLES	3556
Shackles	|2|W
Enchantment - Aura
Enchant creature
Enchanted creature doesn't untap during its controller's untap step.
|W: Return ~ to its owner's hand. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);

		if( IS_GAA_EVENT(event) ){
			if( event == EVENT_RESOLVE_ACTIVATION && in_play(instance->parent_controller, instance->parent_card) ){
				bounce_permanent(instance->parent_controller, instance->parent_card);
			}
			return generic_activated_ability(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return disabling_aura(player, card, event);
}

int card_shattering_pulse(int player, int card, event_t event){
/* CARD_ID_SHATTERING_PULSE	1434
Shattering Pulse	|1|R
Instant
Buyback |3 (You may pay an additional |3 as you cast this spell. If you do, put this card into your hand as it resolves.)
Destroy target artifact. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
		if( spell_fizzled != 1 && ! is_token(player, card) ){
			instance->info_slot = buyback(player, card, MANACOST_X(3));
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_skyshaper(int player, int card, event_t event){
/* CARD_ID_SKYSHAPER	3558
Skyshaper	|2
Artifact
Sacrifice ~: Creatures you control gain flying until end of turn. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_subtype_until_eot(player, card, player, -1, 0, 0, KEYWORD_FLYING, 0);
	}
	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_skyshroud_elite(int player, int card, event_t event){
/* CARD_ID_SKYSHROUD_ELITE	3559
Skyshroud Elite	|G
Creature - Elf 1/1
~ gets +1/+2 as long as an opponent controls a nonbasic land. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_POWER && affect_me(player, card) ){
			if( control_nonbasic_land(1-player) ){
				event_result++;
			}
		}

		if( event == EVENT_TOUGHNESS && affect_me(player, card) ){
			if( control_nonbasic_land(1-player) ){
				event_result+=2;
			}
		}
	}

	return 0;
}

int card_skyshroud_warbeast(int player, int card, event_t event)
{
  /* Skyshroud War Beast	|1|G
   * Creature - Beast 100/100
   * Trample
   * As ~ enters the battlefield, choose an opponent.
   * ~'s power and toughness are each equal to the number of nonbasic lands the chosen player controls. */

  card_instance_t *instance;
  int newval;
  if (card != -1)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      instance->targets[0].player = 1 - player;
      return 0;
    }
    if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      newval = count_nonbasic_lands(instance->targets[0].player);
      set_characteristic_pt(newval);
    }
  }
  return 0;
}

int card_slaughter2(int player, int card, event_t event){
/* CARD_ID_SLAUGHTER	3561
Slaughter	|2|B|B
Instant
Buyback-Pay 4 life. (You may pay 4 life in addition to any other costs as you cast this spell. If you do, put this card into your hand as it resolves.)
Destroy target non|Sblack creature. It can't be regenerated. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "TARGET_CREATURE") ){
			if( ! is_token(player, card) && can_pay_life(player, 4) ){
				int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_CANCEL, DLG_NO_STORAGE,
									"Pay Buyback cost", 1, life[player]-4 > 6 ? 10 : 1,
									"Decline", 			1, 5);
				if( choice == 1 ){
					lose_life(player, 4);
					instance->info_slot = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			if( instance->info_slot == 1 ){
				bounce_permanent(player, card);
				return 0;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_soltari_visionary(int player, int card, event_t event){
  /* Soltari Visionary |1|W|W
   * Creature — Soltari Cleric 2/2
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * Whenever Soltari Visionary deals damage to a player, destroy target enchantment that player controls. */

	shadow(player, card, event);

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						target_definition_t td;
						default_target_definition(player, card, &td, TYPE_ENCHANTMENT );
						td.allowed_controller = td.preferred_controller = p;
						td.allow_cancel = 0;

						if( can_target(&td) ){
							char buffer[100];
							if( p == player ){
								scnprintf(buffer, 100, "Select target enchantment you control.");
							}
							else{
								scnprintf(buffer, 100, "Select target enchantment controlled by your opponent.");
							}
							if( new_pick_target(&td, buffer, 0, GS_LITERAL_PROMPT) ){
								kill_card( instance->targets[0].player, instance->targets[0].card, KILL_DESTROY );
							}
						}
						instance->number_of_targets = 0;
					}
				}
		);
	}

	return 0;
}

int card_sonic_burst(int player, int card, event_t event){
	/* Sonic Burst	|1|R	0x20036f8
	 * Instant
	 * As an additional cost to cast this spell, discard a card at random.
	 * ~ deals 4 damage to any target. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST && hand_count[player] > 1 ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			discard(player, DISC_RANDOM, player);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_soul_warden(int player, int card, event_t event){
/* CARD_ID_SOUL_WARDEN	1557
Soul Warden	|W
Creature - Human Cleric 1/1
Whenever another creature enters the battlefield, you gain 1 life. */

    test_definition_t this_test;
	default_test_definition(&this_test, TYPE_CREATURE);
	this_test.not_me = 1;
	etb_ability_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, &this_test, player, card);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	return 0;
}

int card_spellbook(int player, int card, event_t event){
  /* Spellbook |0
   * 	  Artifact
   * 	  You have no maximum hand size. */

	// Used externally
	if( ! is_humiliated(player, card, AL_STATIC) ){
		if (event == EVENT_MAX_HAND_SIZE && current_turn == player ){
			event_result = 1000;
		}
	}

	return 0;
}

int card_spellshock(int player, int card, event_t event)
{
  /* CARD_ID_SPELLSHOCK	3565
Spellshock	|2|R
Enchantment
Whenever a player casts a spell, ~ deals 2 damage to that player. */

  sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
  if (event == EVENT_SPA)
    damage_player(trigger_cause_controller, 2, player, card);
  return global_enchantment(player, card, event);
}

int card_sphere_of_resistance(int player, int card, event_t event){
/* CARD_ID_SPHERE_OF_RESISTANCE	3566
Sphere of Resistance	|2
Artifact
Spells cost |1 more to cast. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_MODIFY_COST_GLOBAL && ! is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			COST_COLORLESS++;
		}
	}

	return 0;
}

int card_spike_cannibal(int player, int card, event_t event)
{
  /* Spike Cannibal	|1|B|B
   * Creature - Spike 0/0
   * ~ enters the battlefield with a +1/+1 counter on it.
   * When ~ enters the battlefield, move all +1/+1 counters from all creatures onto it. */

  int num_counters[2][151];
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 1);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
    return 0;
  memset(num_counters, 0, sizeof(num_counters));
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
      if (in_play(p, c) && is_what(p, c, 2))
        num_counters[p][c] = count_counters(p, c, COUNTER_P1_P1);
  num_counters[player][card] = 0;
  for (int p = 0; p < 2; ++p)
    for (int c = 0; c < active_cards_count[p]; ++c)
      if (num_counters[p][c])
        move_counters(player, card, player, card, p, c, COUNTER_P1_P1, num_counters[p][c]);
  return 0;
}

int card_spike_hatcher(int player, int card, event_t event){
/* CARD_ID_SPIKE_HATCHER	3568
Spike Hatcher	|6|G
Creature - Spike 0/0
~ enters the battlefield with six +1/+1 counters on it.
|2, Remove a +1/+1 counter from ~: Put a +1/+1 counter on target creature.
|1, Remove a +1/+1 counter from ~: Regenerate ~. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 6);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MOVE_COUNTER = 1,
		CHOICE_REGENERATE
	};

	if( event == EVENT_CAN_ACTIVATE  ){
		if( generic_activated_ability(player, card, event, GAA_REGENERATION | GAA_1_1_COUNTER, MANACOST_X(1), 0, NULL, NULL) ){
			return 99;
		}
		return generic_spike(player, card, event, 6);
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = CHOICE_REGENERATE;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_REGENERATION | GAA_1_1_COUNTER, MANACOST_X(1), 0, NULL, NULL) ){
			return generic_activated_ability(player, card, event, GAA_REGENERATION | GAA_1_1_COUNTER, MANACOST_X(1), 0, NULL, NULL);
		}
		instance->info_slot = CHOICE_MOVE_COUNTER;
		return generic_spike(player, card, event, 6);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MOVE_COUNTER && valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == CHOICE_REGENERATE && can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_spike_rogue(int player, int card, event_t event)
{
  /* Spike Rogue	|1|G|G
   * Creature - Spike 0/0
   * ~ enters the battlefield with two +1/+1 counters on it.
   * |2, Remove a +1/+1 counter from ~: Put a +1/+1 counter on target creature.
   * |2, Remove a +1/+1 counter from a creature you control: Put a +1/+1 counter on ~. */

  enum
  {
    CHOICE_MOVE_COUNTERS = 1,
    CHOICE_STEAL_COUNTERS = 2
  };

  int p;
  int c;
  int choice;
  card_instance_t *instance;
  int opponent_mode_or_result;
  int steal_available;
  int creature_counters_or_result;
  int payable_counters;
  bool own_turn;
  event_t event_00;
  target_definition_t td;
  target_definition_t td1;

  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    steal_available = has_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0);
    p = current_turn;
    if (steal_available != 0)
    {
      c = active_cards_count[current_turn] + -1;
      if (c < 0)
      {
        payable_counters = 0;
      }
      else
      {
        own_turn = player == current_turn;
        payable_counters = 0;
        do
        {
          instance = in_play(p, c);
          if (((instance != NULL) && (own_turn)) &&
              (creature_counters_or_result = is_what(p, c, 2), creature_counters_or_result != 0))
          {
            creature_counters_or_result = count_counters(p, c, COUNTER_P1_P1);
            payable_counters = payable_counters + creature_counters_or_result;
          }
          c = c + -1;
        } while (c != -1);
      }
      opponent_mode_or_result = 1 - current_turn;
      c = active_cards_count[opponent_mode_or_result] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(opponent_mode_or_result, c);
          if (((instance != NULL) && (player == opponent_mode_or_result)) &&
              (creature_counters_or_result = is_what(opponent_mode_or_result, c, 2),
               creature_counters_or_result != 0))
          {
            creature_counters_or_result = count_counters(opponent_mode_or_result, c, COUNTER_P1_P1);
            payable_counters = payable_counters + creature_counters_or_result;
          }
          c = c + -1;
        } while (c != -1);
      }
      while (opponent_mode_or_result =
                 has_mana_for_activated_ability(player, card, payable_counters * 2, 0, 0, 0, 0, 0),
             opponent_mode_or_result == 0)
      {
        payable_counters = payable_counters + -1;
      }
      return payable_counters;
    }
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
  }
  else
  {
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 2);
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      base_target_definition(player, card, &td1, 2);
      td1.allowed_controller = player;
      td1.preferred_controller = player;
      td1.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_REQUIRES_COUNTER;
      td1.extra = 0;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x44, 2, 0, 0, 0, 0, 0, 0,
                                      &td1, NULL) != 0)
        {
          return 1;
        }
        event_00 = EVENT_CAN_ACTIVATE;
        return generic_spike(player, card, event_00, 2);
      }
      if (event == EVENT_ACTIVATE)
      {
        instance->number_of_targets = '\0';
        opponent_mode_or_result = generic_spike(player, card, EVENT_CAN_ACTIVATE, 2);
        steal_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x44, 2, 0, 0,
                                                    0, 0, 0, 0, &td1, NULL);
        choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Move a counter",
                             opponent_mode_or_result, 5, "Steal a counter", steal_available,
                             (unsigned int)(current_phase == 0x18) * 9 + 1, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice != CHOICE_MOVE_COUNTERS)
        {
          if (choice != CHOICE_STEAL_COUNTERS)
          {
            return 0;
          }
          generic_activated_ability(player, card, EVENT_ACTIVATE, 0x44, 2, 0, 0, 0, 0, 0, 0, &td1,
                                    "Select target creature you control with a +1/+1 counter.");
          if (spell_fizzled == 1)
          {
            return 0;
          }
          remove_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                          COUNTER_P1_P1, 1);
          instance->number_of_targets = '\0';
          return 0;
        }
        event_00 = EVENT_ACTIVATE;
        return generic_spike(player, card, event_00, 2);
      }
    }
    else
    {
      if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
      {
        return 0;
      }
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      base_target_definition(player, card, &td1, 2);
      td1.allowed_controller = player;
      td1.preferred_controller = player;
      td1.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_REQUIRES_COUNTER;
      td1.extra = 0;
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      opponent_mode_or_result = instance->info_slot;
      if (opponent_mode_or_result == 1)
      {
        add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                     COUNTER_P1_P1, 1);
        opponent_mode_or_result = instance->info_slot;
      }
      if (opponent_mode_or_result == 2)
      {
        add_counters(player, card, instance->parent_controller, instance->parent_card,
                     COUNTER_P1_P1, 1);
        return 0;
      }
    }
  }
  return 0;
}

int card_spike_weaver(int player, int card, event_t event){
/* CARD_ID_SPIKE_WEAVER	3570
Spike Weaver	|2|G|G
Creature - Spike 0/0
~ enters the battlefield with three +1/+1 counters on it.
|2, Remove a +1/+1 counter from ~: Put a +1/+1 counter on target creature.
|1, Remove a +1/+1 counter from ~: Prevent all combat damage that would be dealt this turn. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MOVE_COUNTERS = 1,
		CHOICE_FOG
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( current_turn != player && count_subtype(player, TYPE_CREATURE, -1) < count_subtype(1-player, TYPE_CREATURE, -1) ){
			ai_modifier += 15;
		}
		instance->number_of_targets = 0;
		int abils[3] = {0,
						generic_spike(player, card, EVENT_CAN_ACTIVATE, 3),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL)
		};
		int priority_fog = current_turn != player ? 10 : 1;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Move a counter", 	abils[1], 	5,
							"Fog",				abils[2], 	priority_fog);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_MOVE_COUNTERS ){
			return generic_spike(player, card, event, 3);
		}
		if( choice == CHOICE_FOG ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(1), GVC_COUNTER(COUNTER_P1_P1), NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_MOVE_COUNTERS && valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
		}
		if( instance->info_slot == CHOICE_FOG ){
			fog_effect(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_survival_of_the_fittest(int player, int card, event_t event){
/* CARD_ID_SURVIVAL_OF_THE_FITTEST	3572
Survival of the Fittest	|1|G
Enchantment
|G, Discard a creature card: Search your library for a creature card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( check_battlefield_for_special_card(player, card, player, CBFSC_CHECK_ONLY, &this_test) ){
			return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
		}
	}
	if( event == EVENT_ACTIVATE ){
		if( instance->info_slot == 1 ){
			ai_modifier -= 256;
		}
		else{
			ai_modifier += (current_turn != player ? 48 : -48);
		}
		int found = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_hand(p, c) ){
										// Check for cards that have activated or continuous abilities when in your graveyard
										int iid = get_original_internal_card_id(p, c);
										if( cards_data[iid].cc[2] == 9 || cards_data[iid].cc[2] == 2 ){
											found = 1;
											goto end;
										}
									}
								};
		);
		end:
		if( ! found ){
			ai_modifier -= 24;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_G(1)) ){
			if( spell_fizzled != 1 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_GOOD_TO_PUT_IN_GRAVE, -1, &this_test);
				if( selected != -1 ){
					discard_card(player, selected);
					instance->info_slot = 1;
				}
				else{
					spell_fizzled = 1;
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( IS_AI(player) && current_turn == player ){
			this_test.cmc = has_mana(player, COLOR_ANY, 1) + 1;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		}
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
	}

	if( event == EVENT_CLEANUP ){
		instance->info_slot = 0;
	}

	return 0;
}

int card_thalakos_drifters(int player, int card, event_t event){
/* CARD_ID_THALAKOS_DRIFTERS	3573
Thalakos Drifters	|2|U|U
Creature - Thalakos 3/3
Discard a card: ~ gains shadow until end of turn. (It can block or be blocked by only creatures with shadow.) */

	if( event == EVENT_ACTIVATE ){
		if( current_phase >= PHASE_DECLARE_BLOCKERS ){
			ai_modifier -= 15;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, 0, SP_KEYWORD_SHADOW);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_thalakos_scout(int player, int card, event_t event){
/* CARD_ID_THALAKOS_SCOUT	3574
Thalakos Scout	|2|U
Creature - Thalakos Soldier Scout 2/1
Shadow (This creature can block or be blocked by only creatures with shadow.)
Discard a card: Return ~ to its owner's hand. */

   shadow(player, card, event);
   return card_ephemeron(player, card, event);
}

int card_theft_of_dreams(int player, int card, event_t event){
/* CARD_ID_THEFT_OF_DREAMS	3575
Theft of Dreams	|2|U
Sorcery
Draw a card for each tapped creature target opponent controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0 );
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_CREATURE);
			this_test.state = STATE_TAPPED;

			draw_cards(player, check_battlefield_for_special_card(player, card, instance->targets[0].player, CBFSC_GET_COUNT, &this_test));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_thrull_surgeon(int player, int card, event_t event){
/* CARD_ID_THRULL_SURGEON	3577
Thrull Surgeon	|1|B
Creature - Thrull 1/1
|1|B, Sacrifice ~: Look at target player's hand and choose a card from it. That player discards that card. Activate this ability only any time you could cast a sorcery. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED|GAA_SACRIFICE_ME|GAA_CAN_TARGET, MANACOST_XB(1, 1), 0,
									&td, "TARGET_PLAYER");
}

int card_treasure_hunter(int player, int card, event_t event)
{
  /* CARD_ID_TREASURE_HUNTER	3579
  Treasure Hunter	|2|W
  Creature - Human 2/2
  When ~ enters the battlefield, you may return target artifact card from your graveyard to your
  hand. */

  test_definition_t this_test;
  default_test_definition(&this_test, TYPE_ARTIFACT);
  return generic_type_recycler_creature(player, card, event, &this_test);
}

int card_treasure_trove(int player, int card, event_t event){
  /* Treasure Trove |2|U|U
   * 	  Enchantment
   * 	  {2}{U}{U}: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 2), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int amount = count_subtype(player, TYPE_LAND, -1);
		if( amount <= 2 || amount >= 6 ){
			ai_modifier += 48;
		}
		if( current_turn != player && current_phase >= PHASE_MAIN2 ){
			ai_modifier += 20;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 2), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_trasmogrifying_licid(int player, int card, event_t event)
{
  /* CARD_ID_TRANSMOGRIFYING_LICID	3578
   * Transmogrifying Licid	|3
   * Artifact Creature - Licid 2/2
   * |1, |T: ~ loses this ability and becomes an Aura enchantment with enchant creature. Attach it to target creature. You may pay |1 to end this effect.
   * Enchanted creature gets +1/+1 and is an artifact in addition to its other types. */

  int enchanted_card;
  card_data_t *created_type;
  card_instance_t *instance;
  int type_or_base_stat;
  card_instance_t *parent;
  int attached_player_or_result;
  target_definition_t *td_00;
  const char *prompt;
  int artifact_type_iid;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if (card == affected_card && player == affected_card_controller &&
        (attached_player_or_result = instance->targets[3].player, attached_player_or_result > 0))
      event_result = attached_player_or_result;
    if (instance->targets[3].card != 0x42)
      return 0;
  }
  if (instance->targets[3].card == 0x42)
  {
    bool handle_activation = true;
    attached_player_or_result = (int)instance->damage_target_player;
    if (instance->damage_target_player >= 0)
    {
      enchanted_card = instance->damage_target_card;
      if ((instance->targets[2].player != attached_player_or_result) ||
          (instance->targets[2].card != enchanted_card))
      {
        instance->dummy3 = -1;
        instance->targets[2].player = attached_player_or_result;
        instance->targets[2].card = enchanted_card;
      }
      type_or_base_stat = is_what(
          -1, get_original_internal_card_id(attached_player_or_result, enchanted_card), 0x40);
      if ((event == EVENT_CHANGE_TYPE) && (type_or_base_stat == 0))
      {
        if ((enchanted_card == affected_card) &&
            (attached_player_or_result == affected_card_controller))
        {
          artifact_type_iid = instance->dummy3;
          if (artifact_type_iid == -1)
          {
            artifact_type_iid = create_a_card_type(
                get_original_internal_card_id(attached_player_or_result, enchanted_card));
            type_or_base_stat =
                get_original_internal_card_id(attached_player_or_result, enchanted_card);
            cards_at_7c7000[artifact_type_iid]->type = cards_at_7c7000[artifact_type_iid]->type |
                                                       cards_data[type_or_base_stat].type | 0x42;
            type_or_base_stat = get_base_power(attached_player_or_result, enchanted_card);
            cards_at_7c7000[artifact_type_iid]->power = (int16_t)type_or_base_stat;
            type_or_base_stat = get_base_toughness(attached_player_or_result, enchanted_card);
            cards_at_7c7000[artifact_type_iid]->toughness = (int16_t)type_or_base_stat;
            instance->dummy3 = artifact_type_iid;
          }
          event_result = artifact_type_iid;
        }
        leaves_play(attached_player_or_result, enchanted_card, EVENT_CHANGE_TYPE, 0, 0);
        handle_activation = false;
      }
      else
      {
        leaves_play(attached_player_or_result, enchanted_card, event, 0, 0);
        if (event == EVENT_LEAVES_PLAY_ABILITY)
        {
          kill_card(player, card, KILL_SACRIFICE);
          handle_activation = false;
        }
      }
    }
    if (handle_activation && (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE))
      return generic_activated_ability(player, card, event, 0, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
    if (handle_activation && event == EVENT_RESOLVE_ACTIVATION)
    {
      parent = get_card_instance(instance->parent_controller, instance->parent_card);
      purge_add_force_a_subtype_permanently_legacy(instance->parent_controller,
                                                   instance->parent_card, 2);
      parent->damage_target_player = -1;
      parent->damage_target_card = -1;
      parent->regen_status = parent->regen_status | 0x1f000000;
      parent->dummy3 = -1;
      parent->targets[3].player = -1;
      get_abilities(instance->parent_controller, instance->parent_card, EVENT_CHANGE_TYPE, -1);
    }
    else
    {
      if ((handle_activation || event == EVENT_CHANGE_TYPE) &&
          (((uint32_t)(event - EVENT_POWER) < 2) || event == EVENT_CAN_MOVE_AURA))
        return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
      if (((uint32_t)(event - EVENT_MOVE_AURA) & 0xfffffeff) == 0)
        return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
    }
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    parent = get_card_instance(instance->parent_controller, instance->parent_card);
    if (parent->targets[3].card == 0x42)
    {
      parent->targets[3].card = 0;
    }
    else
    {
      if (valid_target(&td) != 0)
      {
        attached_player_or_result = create_a_card_type(parent->internal_card_id);
        created_type = cards_at_7c7000[attached_player_or_result];
        created_type->type = '\x04';
        created_type->power = 0;
        created_type->toughness = 0;
        parent->targets[3].player = attached_player_or_result;
        add_a_subtype_permanently(player, card, instance->parent_controller, instance->parent_card,
                                  SUBTYPE_AURA_CREATURE);
        attach_aura_to_target(instance->parent_controller, instance->parent_card,
                              EVENT_RESOLVE_ACTIVATION, instance->targets[0].player,
                              instance->targets[0].card);
        parent->regen_status = parent->regen_status | 0x1f000000;
        get_abilities(instance->parent_controller, instance->parent_card, EVENT_CHANGE_TYPE, -1);
        parent->targets[3].card = 0x42;
      }
    }
  }
  td_00 = &td;
  prompt = "TARGET_CREATURE";
  attached_player_or_result = 5;
  return generic_activated_ability(player, card, event, attached_player_or_result, 1, 0, 0, 0, 0, 0,
                                   0, td_00, prompt);
}

int card_vampire_hounds(int player, int card, event_t event){
/* CARD_ID_VAMPIRE_HOUNDS	3580
Vampire Hounds	|2|B
Creature - Vampire Hound 2/2
Discard a creature card: ~ gets +2/+2 until end of turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( has_mana_for_activated_ability(player, card, MANACOST0) ){
			test_definition_t this_test2;
			default_test_definition(&this_test2, TYPE_CREATURE);
			this_test2.zone = TARGET_ZONE_HAND;

			int max_pump = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test2);
			return max_pump * 2;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test2;
	new_default_test_definition(&this_test2, TYPE_CREATURE, "Select a creature card to discard.");
	this_test2.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test2);
		}
	}

	if( event == EVENT_ACTIVATE){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test2);
			if( selected != -1 ){
				discard_card(player, selected);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 2, 2);
	}

	return 0;
}

static int volraths_dungeon_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (result = instance->targets[0].player, -1 < result))
    {
      result = get_id(result, instance->targets[0].card);
      if (result != 0xdfd)
      {
        kill_card(player, card, KILL_EXILE);
      }
      instance = in_play(instance->targets[0].player, instance->targets[0].card);
      if (instance == NULL)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  else if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_use_activated_abilities(instance->targets[0].player, instance->targets[0].card);
    if ((result != 0) && (current_turn == player))
    {
      result = get_cost_mod_for_activated_abilities(instance->targets[0].player,
                                                    instance->targets[0].card, 0, 0, 0, 0, 0, 0);
      result = has_mana(player, 0, result);
      if (result != 0)
      {
        result = can_pay_life(player, 5);
        return result;
      }
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = get_cost_mod_for_activated_abilities(instance->targets[0].player,
                                                  instance->targets[0].card, 0, 0, 0, 0, 0, 0);
    charge_mana(player, COLOR_COLORLESS, result);
    if (spell_fizzled != 1)
    {
      lose_life(player, 5);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  }
  return 0;
}

int card_volraths_dungeon(int player, int card, event_t event)
{
  /* Volrath's Dungeon |2|B|B
   *   Enchantment
   *   Pay 5 life: Destroy Volrath's Dungeon. Any player may activate this ability but only during his or
   *   her turn. Discard a card: Target player puts a card from his or her hand on top of his or her
   *   library. Activate this ability only any time you could cast a sorcery. */

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (affect_me(player, card))
    {
      int found = 0;
      for (int p = 0; p < 2; ++p)
      {
        for (int c = 0; c <= active_cards_count[p]; ++c)
        {
          if (in_play(p, c) && is_what(p, c, TYPE_EFFECT))
          {
            card_instance_t *inst = get_card_instance(p, c);
            if (inst->info_slot == (int)volraths_dungeon_legacy &&
                inst->targets[0].player == player && inst->targets[0].card == card)
            {
              if (p == player)
                kill_card(player, c, KILL_EXILE);
              else
              {
                found = 1;
                if (p == 1)
                  goto dungeon_static_done;
              }
            }
          }
        }
      }
      if (!found)
      {
        int opponent = 1 - player;
        int fake = add_card_to_hand(opponent, instance->internal_card_id);
        int legacy = create_legacy_activate(opponent, fake, volraths_dungeon_legacy);
        card_instance_t *inst = get_card_instance(opponent, legacy);
        inst->targets[0].player = player;
        inst->targets[0].card = card;
        inst->number_of_targets = 1;
        obliterate_card(opponent, fake);
      }
    }
  dungeon_static_done:
    return global_enchantment(player, card, event);
  }

  if (!IS_GAA_EVENT(event))
  {
    return global_enchantment(player, card, event);
  }

  target_definition_t td1;
  default_target_definition(player, card, &td1, 0);
  td1.zone = TARGET_ZONE_PLAYERS;

  enum
  {
    CHOICE_TARGETED_ABILITY = 1,
    CHOICE_KILL_VD
  };

  if (event == EVENT_CAN_ACTIVATE)
  {
    if (generic_activated_ability(player, card, event,
                                  GAA_DISCARD | GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET,
                                  MANACOST0, 0, &td1, NULL))
    {
      return 1;
    }
    return generic_activated_ability(player, card, event, GAA_IN_YOUR_TURN, MANACOST0, 5, NULL,
                                     NULL);
  }

  if (event == EVENT_ACTIVATE)
  {
    int choice = instance->info_slot = 0;
    int abilities[2] = {can_sorcery_be_played(player, event) &&
                            generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                                      GAA_DISCARD | GAA_CAN_TARGET, MANACOST0, 0,
                                                      &td1, NULL),
                        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                                  GAA_IN_YOUR_TURN, MANACOST0, 5, NULL, NULL)};
    choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE, "Targeted ability",
                    abilities[0], 2, "Destroy Volrath's Dungeon", abilities[1], 1);

    if (!choice)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_TARGETED_ABILITY)
    {
      generic_activated_ability(player, card, event,
                                GAA_DISCARD | GAA_CAN_SORCERY_BE_PLAYED | GAA_CAN_TARGET, MANACOST0,
                                0, &td1, "TARGET_PLAYER");
      if (spell_fizzled != 1)
      {
        instance->info_slot = choice;
      }
    }
    if (choice == CHOICE_KILL_VD)
    {
      generic_activated_ability(player, card, event, GAA_IN_YOUR_TURN, MANACOST0, 5, NULL, NULL);
      if (spell_fizzled != 1)
      {
        instance->info_slot = choice;
      }
    }
  }

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == CHOICE_TARGETED_ABILITY)
    {
      if (validate_target(player, card, &td1, 0) && hand_count[instance->targets[0].player] > 0)
      {
        test_definition_t this_test;
        new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on top.");
        new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND,
                         TUTOR_DECK, 1, AI_MIN_VALUE, &this_test);
      }
    }
    if (instance->info_slot == CHOICE_KILL_VD)
    {
      kill_card(instance->parent_controller, instance->parent_card, KILL_DESTROY);
    }
  }

  return 0;
}

int card_wall_of_nets(int player, int card, event_t event){
  /* Wall of Nets |1|W|W
   * Creature — Wall 0/7
   * Defender (This creature can't attack.)
   * At end of combat, exile all creatures blocked by Wall of Nets.
   * When Wall of Nets leaves the battlefield, return all cards exiled with Wall of Nets to the battlefield under their owners' control. */

	if( trigger_condition == TRIGGER_LEAVE_PLAY && affect_me(player, card) && reason_for_trigger_controller == player ){
		if( trigger_cause_controller == player && trigger_cause == card ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->targets[1].card = exiledby_detach(player, card);
			leaves_play(player, card, event, 1, 0);
		}
	}

	if (event == EVENT_LEAVES_PLAY_ABILITY){
		get_back_cards_stored_in_exiledby(player, player-2, get_card_instance(player, card)->targets[1].card, GBC_PUT_PERMANENTS_INTO_BF);
	}
	if( trigger_condition == TRIGGER_END_COMBAT && affect_me(player, card) && reason_for_trigger_controller == player ){
		int trig = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->blocking < 255 ){
			trig = 1;
		}
		if( trig == 1 && end_of_combat_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			if( ! is_token(1-player, instance->blocking) ){
				create_card_name_legacy(player, card, cards_data[get_original_internal_card_id(1-player, instance->blocking)].id);
				exile_card_and_remember_it_on_exiledby(player, card, 1-player, instance->blocking);
			}
			else{
				kill_card(1-player, instance->blocking, KILL_EXILE);
			}
		}
	}

	return 0;
}

int card_welkin_hawk(int player, int card, event_t event){
/* CARD_ID_WELKIN_HAWK	3584
Welkin Hawk	|1|W
Creature - Bird 1/1
Flying
When ~ dies, you may search your library for a card named ~, reveal that card, put it into your hand, then shuffle your library. */

	if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_AI(player))){
		int id = get_card_instance(player, card)->display_pic_csv_id;
		char prompt[100];
		if (ai_is_speculating == 1){
			prompt[0] = 0;
		}
		else{
			sprintf(prompt, "Select a card named %s.", cards_ptr[id]->full_name);
		}
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, prompt);
		test.id = id;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);
	}
	return 0;
}

int card_workhorse(int player, int card, event_t event){
/* CARD_ID_WORKHORSE	3586
Workhorse	|6
Artifact Creature - Horse 0/0
~ enters the battlefield with four +1/+1 counters on it.
Remove a +1/+1 counter from ~: Add |C to your mana pool. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);

	if( event == EVENT_CAN_ACTIVATE && count_1_1_counters(player, card) > 0 ){
	   return can_produce_mana(player, card);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && count_1_1_counters(player, card) != 0 && can_produce_mana(player, card)){
		declare_mana_available(player, COLOR_COLORLESS, count_1_1_counters(player, card));
	}

	if( event == EVENT_ACTIVATE){
		remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
		produce_mana(player, COLOR_COLORLESS, 1);
		tapped_for_mana_color = -2;
	}

	return 0;
}

int card_zealots_en_dal(int player, int card, event_t event){
/*
Zealots en-Dal |3|W	0x200f516
Creature — Human Soldier 2/4
At the beginning of your upkeep, if all nonland permanents you control are white, you gain 1 life.
*/

	test_definition_t test, test2;
	default_test_definition(&test, TYPE_LAND);
	test.type_flag = DOESNT_MATCH;
	default_test_definition(&test2, TYPE_PERMANENT);
	test2.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK | COLOR_TEST_BLUE | COLOR_TEST_GREEN | COLOR_TEST_RED);
	upkeep_trigger_ability(player, card, event, player);
	if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
		if (check_battlefield_for_special_card(player, card, player, 0, &test) && check_battlefield_for_special_card(player, card, player, 0, &test2)){
			return 0;
		}
		gain_life(player, 1);
	}
	return 0;
}

// ---- DOES NOT WORK

int card_limited_resources(int player, int card, event_t event)
{
  /* Limited Resources	|W
   * Enchantment
   * When this enchantment enters, each player chooses five lands they control and sacrifices the rest.
   * Players can't play lands as long as ten or more lands are on the battlefield. */

  unsigned int what_player;
  int result;
  card_instance_t *instance;
  unsigned int player_pass;
  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      player_bits[player] = player_bits[player] | 8;
      player_bits[(1 - player)] = player_bits[(1 - player)] | 8;
      if (((land_can_be_played & 1U) == 0) &&
          (9 < landsofcolor_controlled[1][7] + landsofcolor_controlled[0][7]))
      {
        land_can_be_played = land_can_be_played | 1;
      }
    }
    leaves_play(player, card, EVENT_STATIC_EFFECTS, 0, 0);
    etb_ability(player, card, EVENT_STATIC_EFFECTS, RESOLVE_TRIGGER_MANDATORY);
  }
  else
  {
    leaves_play(player, card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      result = total_playable_lands(current_turn);
      if (lands_played < result)
      {
        land_can_be_played = land_can_be_played & 0xfffffffe;
        player_bits[player] = player_bits[player] & 0xfffffff7;
        player_bits[(1 - player)] = player_bits[(1 - player)] & 0xfffffff7;
      }
      etb_ability(player, card, EVENT_LEAVES_PLAY_ABILITY, RESOLVE_TRIGGER_MANDATORY);
    }
    else
    {
      etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
      if (event == EVENT_ETB)
      {
        player_pass = 0;
        do
        {
          what_player = player_pass ^ current_turn;
          result = count_subtype(what_player, 1, -1);
          if (5 < result)
          {
            impose_sacrifice(player, card, what_player, result + -5, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0);
          }
          player_pass = player_pass + 1;
        } while (player_pass != 2);
      }
    }
  }
  return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264c8d0; original cards/exodus.c:3314. */
int card_aether_tide(int player, int card, event_t event)
{
  /* Aether Tide	|X|U
   * Sorcery
   * As an additional cost to cast this spell, discard X creature cards.
   * Return X target creatures to their owners' hands. */

  target_t *target_cursor;
  target_t *other_target_cursor;
  card_instance_t *instance;
  unsigned int spell_copy;
  int32_t selection_limit;
  target_definition_t *creature_target;
  int target_index_or_result;
  target_definition_t *target_definition;
  target_definition_t td;
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    new_default_test_definition(&this_test, 2, "Select a creature card to discard.");
    this_test.zone = 0x100;
    get_card_instance(player, card);
    if (generic_spell(player, card, EVENT_CAN_CAST, 5, &td, NULL, 1, NULL) == 0)
    {
      return 0;
    }
    if (check_hand_for_type(player, card, 2) == 0)
    {
      return 0;
    }
    return check_battlefield_for_special_card(player, card, player, 0, &this_test);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if (((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET)) &&
        (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    target_definition = &td;
    default_target_definition(player, card, target_definition, 2);
    new_default_test_definition(&this_test, 2, "Select a creature card to discard.");
    this_test.zone = 0x100;
    instance = get_card_instance(player, card);
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }
    for (target_index_or_result = 0;
         target_index_or_result < (unsigned int)instance->number_of_targets;
         ++target_index_or_result)
      if (validate_target(player, card, target_definition, target_index_or_result))
      {
        other_target_cursor = instance->targets + target_index_or_result;
        target_cursor = instance->targets + target_index_or_result;
        bounce_permanent(target_cursor->player, other_target_cursor->card);
      }
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  target_definition = &td;
  default_target_definition(player, card, target_definition, 2);
  new_default_test_definition(&this_test, 2, "Select a creature card to discard.");
  this_test.zone = 0x100;
  instance = get_card_instance(player, card);
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  set_special_flags2(player, card, 0x20000);
  instance->number_of_targets = '\0';
  if (played_for_free(player, card) != 0 || is_token(player, card) != 0)
  {
    if (check_special_flags2(player, card, -0x80000000) == 0)
    {
      return 0;
    }
  }

  if (check_special_flags2(player, card, -0x80000000) == 0)
  {
    instance->info_slot = x_value;
    target_index_or_result =
        check_battlefield_for_special_card(player, card, player, 4, &this_test);
    if (target_index_or_result < x_value)
    {
      selection_limit = check_battlefield_for_special_card(player, card, player, 4, &this_test);
      instance->info_slot = selection_limit;
    }
  }
  selection_limit = instance->info_slot;
  creature_target = target_definition;
  target_index_or_result = 0;
  if (0 < selection_limit)
  {
    do
    {
      if (can_target(creature_target) == 0)
        break;
      if (check_special_flags2(player, card, -0x80000000) == 0)
      {
        if (has_mana(player, 0, target_index_or_result + 1) == 0)
          break;
      }
      if (new_pick_target(creature_target, "TARGET_CREATURE", target_index_or_result, 0) == 0)
        break;
      other_target_cursor = instance->targets + target_index_or_result;
      target_cursor = instance->targets + target_index_or_result;
      target_index_or_result = target_index_or_result + 1;
      state_untargettable(target_cursor->player, other_target_cursor->card, 1);
    } while (target_index_or_result < instance->info_slot);
  }
  if (instance->number_of_targets != '\0')
  {
    target_index_or_result = 0;
    do
    {
      other_target_cursor = instance->targets + target_index_or_result;
      target_cursor = instance->targets + target_index_or_result;
      target_index_or_result = target_index_or_result + 1;
      state_untargettable(target_cursor->player, other_target_cursor->card, 0);
    } while (target_index_or_result < (int)(unsigned int)instance->number_of_targets);
  }
  spell_copy = check_special_flags2(player, card, -0x80000000);
  if (spell_copy == 0)
  {
    int hand_slots = hand_count[player];
    int original_hand[2][hand_slots];
    int ohc = 0;
    int discard_slots = hand_slots;
    if (active_cards_count[player] > 0)
    {
      for (int c = 0; c < active_cards_count[player]; ++c)
      {
        if (in_hand(player, c) != NULL)
        {
          original_hand[0][ohc] = get_original_internal_card_id(player, c);
          original_hand[1][ohc] = c;
          ++ohc;
        }
      }
      discard_slots = hand_count[player];
    }
    int to_discard[discard_slots];
    int tdc = 0;
    while (tdc < (unsigned int)instance->number_of_targets)
    {
      int selected =
          select_card_from_zone(player, player, original_hand[0], ohc, 0, 3, -1, &this_test);
      if (selected == -1)
      {
        if (tdc < (unsigned int)instance->number_of_targets)
        {
          spell_fizzled = 1;
          return 0;
        }
        break;
      }
      to_discard[tdc++] = original_hand[1][selected];
      for (int k = selected; k < ohc; ++k)
      {
        original_hand[0][k] = original_hand[0][k + 1];
        original_hand[1][k] = original_hand[1][k + 1];
      }
      --ohc;
    }
    charge_mana(player, 0,
                (int)((uint32_t)instance->number_of_targets - (uint32_t)instance->info_slot));
    if (spell_fizzled != 1)
    {
      for (int i = 0; i < (unsigned int)instance->number_of_targets; ++i)
        discard_card(player, to_discard[i]);
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x264cfe0; original cards/exodus.c:3422. */
int card_avenging_druid(int player, int card, event_t event)
{
  /* Avenging Druid	|2|G
   * Creature - Human Druid 1/3
   * Whenever this creature deals damage to an opponent, you may reveal cards from the top of your library until you reveal a land card. If you do, put that card onto the battlefield and put all other cards revealed this way into your graveyard. */

  unsigned int packed_damage_triggers;
  int times_damage[2];
  test_definition_t test;

  packed_damage_triggers = whenever_i_deal_damage_to_a_player(
      player, card, event, DDBM_MUST_DAMAGE_OPPONENT | DDBM_TRIGGER_OPTIONAL);
  if (packed_damage_triggers == 0)
  {
    return 0;
  }
  times_damage[0] = packed_damage_triggers & 0xff;
  times_damage[1] = packed_damage_triggers >> 8 & 0xff;
  default_test_definition(&test, 1);
  test.qty = (unsigned short)times_damage[1 - player];
  reveal_card_until_you_reveal_test(player, player, &test, 0x41);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264d090; original cards/exodus.c:3440. */
int card_bequeathal(int player, int card, event_t event)
{
  /* Bequeathal	|G
   * Enchantment - Aura
   * Enchant creature
   * When enchanted creature dies, you draw two cards. */

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = attached_creature_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
      if (result != 0)
      {
        draw_cards(player, 2);
      }
    }
  }
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x264d130; original cards/exodus.c:3456. */
int card_cat_burglar(int player, int card, event_t event)
{
  /* Cat Burglar	|3|B
   * Creature - Kor Rogue Minion 2/2
   * |2|B, |T: Target player discards a card. Activate only as a sorcery. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    discard(instance->targets[0].player, 0, player);
  }
  result = generic_activated_ability(player, card, event, 0x15, 2, 1, 0, 0, 0, 0, 0, &td,
                                     "TARGET_PLAYER");
  return result;
}

/* ManalinkEh 2022: 0x264d270; original cards/exodus.c:3478. */
int card_cinder_crawler(int player, int card, event_t event)
{
  /* Cinder Crawler	|1|R
   * Creature - Salamander 1/2
   * |R: This creature gets +1/+0 until end of turn. Activate only if this creature is blocked. */

  int result;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = count_my_blockers(player, card);
    if (result < 1)
    {
      return 0;
    }
  }
  else if (((uint32_t)(event - EVENT_POW_BOOST) < 2) && (current_turn != player))
  {
    return 0;
  }
  return generic_shade_merge_pt(player, card, event, 0, 0, 0, 0, 0, 1, 0, 1, 0);
}

/* ManalinkEh 2022: 0x264d320; original cards/exodus.c:3495. */
int card_crashing_boars(int player, int card, event_t event)
{
  /* Crashing Boars	|3|G|G
   * Creature - Boar 4/4
   * Whenever this creature attacks, defending player chooses an untapped creature they control. That creature blocks this creature this turn if able. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  base_target_definition(player, card, &td, 2);
  td.who_chooses = 1 - current_turn;
  td.illegal_state = TARGET_STATE_TAPPED;
  td.allow_cancel = 0;
  td.allowed_controller = td.who_chooses;
  td.preferred_controller = td.who_chooses;
  result = can_target(&td);
  if (result != 0)
  {
    result = pick_target(&td, "TARGET_UNTAPPED_CREATURE");
    if (result != 0)
    {
      target_must_block_me(player, card, instance->targets[0].player, instance->targets[0].card, 1);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x2642920; original cards/exodus.c:3521. */
static int cunning_effect(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264d420; original cards/exodus.c:3530. */
int card_cunning(int player, int card, event_t event)
{
  /* Cunning	|1|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +3/+3.
   * When enchanted creature attacks or blocks, sacrifice this Aura at the beginning of the next cleanup step. */

  char attached_player;
  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if ((instance != NULL) && (-1 < instance->damage_target_player))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {

      attached_player = instance->damage_target_player;
      result = instance->damage_target_card;
      store_attackers(player, card, event, 0, (int)attached_player, result, NULL);
      dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
      if (event != EVENT_DAT_ABILITY)
      {
        result = blocking((int)attached_player, result, event);
        if (result == 0)
        {
          attached_gets_pt(player, card, event, 3, 3);
          return aura(player, card, event, NULL, NULL);
        }
      }

      result = create_targetted_legacy_effect(player, card, cunning_effect, player, card);
      instance = get_card_instance(player, result);
      instance->targets[0].player = player;
      instance->targets[0].card = card;
    }
  }
  attached_gets_pt(player, card, event, 3, 3);
  return aura(player, card, event, NULL, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264d570; original cards/exodus.c:3556. */
int card_cursed_flesh(int player, int card, event_t event)
{
  /* Cursed Flesh	|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -1/-1 and has fear. (It can't be blocked except by artifact creatures and/or black creatures.) */

  int result;

  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_FEAR);
  attached_gets_pt(player, card, event, -1, -1);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x264d5f0; original cards/exodus.c:3569. */
int card_dauthi_cutthroat(int player, int card, event_t event)
{
  /* Dauthi Cutthroat	|1|B
   * Creature - Dauthi Minion 1/1
   * Shadow (This creature can block or be blocked by only creatures with shadow.)
   * |1|B, |T: Destroy target creature with shadow. */

  shadow(player, card, event);
  if (!IS_GAA_EVENT(event))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.extra = (int32_t)with_shadow;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE)
    return generic_activated_ability(player, card, event, 7, 1, 1, 0, 0, 0, 0, 0, &td,
                                     "Select target creature with shadow.");
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  return generic_activated_ability(player, card, event, 5, 1, 1, 0, 0, 0, 0, 0, &td, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264d7e0; original cards/exodus.c:3603. */
int card_dizzying_gaze(int player, int card, event_t event)
{
  /* Dizzying Gaze	|R
   * Enchantment - Aura
   * Enchant creature you control
   * |R: Enchanted creature deals 1 damage to target creature with flying. */

  card_instance_t *instance;
  int t_card;
  int t_player;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    if (is_humiliated(player, card, AL_STATIC) == 0)
    {

      instance = get_card_instance(player, card);
      t_player = (int)instance->damage_target_player;
      t_card = instance->damage_target_card;
      default_target_definition(t_player, t_card, &td, 2);
      td.required_abilities = 0x20;
      if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
      {
        return granted_generic_activated_ability(player, card, t_player, t_card, event, 6, 0,
                                                       0, 0, 0, 1, 0, 0, &td,
                                                       "Select target creature with flying.");
      }
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          (validate_target(player, card, &td, 0) != 0))
      {
        damage_creature(instance->targets[0].player, instance->targets[0].card, 1, t_player,
                        t_card);
      }
    }
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.allowed_controller = player;
  return aura(player, card, event, &td, "TARGET_CREATURE_YOU_CONTROL");
}
#endif

/* ManalinkEh 2022: 0x264d9a0; original cards/exodus.c:3639. */
int card_elven_palisade(int player, int card, event_t event)
{
  /* Elven Palisade	|G
   * Enchantment
   * Sacrifice a Forest: Target attacking creature gets -3/-0 until end of turn. */

  const char *sacrifice_prompt;
  card_instance_t *instance;
  target_definition_t td;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_test_definition(&test, 1);
    test.subtype = get_hacked_subtype(player, card, 0x5001);
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    if (event == EVENT_ACTIVATE)
    {
      sacrifice_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice", 0x5001);
      scnprintf(test.message, 100, "%s", sacrifice_prompt);
      return generic_activated_ability_with_sacrifice_as_cost(
          player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, 0, 0, 0, 0, 0, 0, 0, &td,
          "Select target attacking creature.", TYPE_NONE, &test, SARV_0);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return global_enchantment(player, card, event);
    }
    default_test_definition(&test, 1);
    test.subtype = get_hacked_subtype(player, card, 0x5001);
    default_target_definition(player, card, &td, 2);
  }
  td.required_state = TARGET_STATE_ATTACKING;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (valid_target(&td))
    {
      instance = get_card_instance(player, card);
      pump_until_eot(instance->parent_controller, instance->parent_card,
                     instance->targets[0].player, instance->targets[0].card, -3, 0);
    }
  }
  return generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, 0, 0, 0, 0, 0, 0, 0, &td,
      "Select target attacking creature.", TYPE_NONE, &test, SARV_0);
}

/* ManalinkEh 2022: 0x264dc00; original cards/exodus.c:3672. */
int card_entropic_specter(int player, int card, event_t event)
{
  /* Entropic Specter	|3|B|B
   * Creature - Specter Spirit * / *
   * Flying
   * As this creature enters, choose an opponent.
   * ~'s power and toughness are each equal to the number of cards in the chosen player's hand.
   * Whenever this creature deals damage to a player, that player discards a card. */

  card_instance_t *instance;

  if (card != -1)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      instance->targets[0].player = 1 - player;
    }
    else if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
             (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      set_characteristic_pt(hand_count[instance->targets[0].player]);
    }
    whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, DDBM_0, 0);
  }
  return 0;
}

/* ManalinkEh 2022: 0x264dca0; original cards/exodus.c:3696. */
int card_fade_away(int player, int card, event_t event)
{
  /* Fade Away	|2|U
   * Sorcery
   * For each creature, its controller sacrifices a permanent of their choice unless they pay |1. */

  int mana_available;
  int scan_or_sacrifice_available;
  card_instance_t *instance;
  unsigned int argument1;
  unsigned int player_pass;
  test_definition_t test;

  if (event == EVENT_RESOLVE_SPELL)
  {

    player_pass = 0;
    do
    {

      argument1 = current_turn ^ player_pass;
      mana_available = has_mana(argument1, 7, 1);
      scan_or_sacrifice_available =
          can_sacrifice(player, argument1, 1, TYPE_PERMANENT, SUBTYPE_INVALID);

      if ((scan_or_sacrifice_available != 0) &&
          (scan_or_sacrifice_available = active_cards_count[argument1] + -1,
           -1 < scan_or_sacrifice_available))
      {
        do
        {
          instance = in_play(argument1, scan_or_sacrifice_available);
          if ((instance != NULL) && (is_what(argument1, scan_or_sacrifice_available, 2) != 0))
          {
            if ((mana_available < 1) ||
                (do_dialog(argument1, argument1, scan_or_sacrifice_available, -1, -1,
                           " Pay 1 mana\n Sacrifice a permanent", 0) != 0))
            {

              new_default_test_definition(&test, 0x47, "Select a permanent to sacrifice.");
              new_sacrifice(player, card, argument1, SAC_CAUSED | SAC_NO_CANCEL, &test);
            }
            else
            {
              charge_mana(argument1, COLOR_ANY, 1);
            }
          }
          scan_or_sacrifice_available = scan_or_sacrifice_available + -1;
        } while (scan_or_sacrifice_available != -1);
      }
      player_pass = player_pass + 1;
    } while (player_pass != 2);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x264de70; original cards/exodus.c:3733. */
int card_fighting_chance(int player, int card, event_t event)
{
  /* Fighting Chance	|R
   * Instant
   * For each blocking creature, flip a coin. If you win the flip, prevent all combat damage that would be dealt by that creature this turn. */

  int result;
  card_instance_t *instance;
  int card_00;

  if (event == EVENT_RESOLVE_SPELL)
  {

    result = 1 - current_turn;
    if (0 < active_cards_count[result])
    {
      card_00 = 0;
      do
      {
        instance = in_play(result, card_00);
        if (instance != NULL)
        {
          result = is_what(1 - current_turn, card_00, 2);
          if (result != 0)
          {
            result = check_state(1 - current_turn, card_00, 8);
            if (result != 0)
            {
              result = flip_a_coin(player, card_00);
              if (result != 0)
              {
                negate_combat_damage_this_turn(player, card, 1 - current_turn, card_00, 0);
              }
            }
          }
        }
        result = 1 - current_turn;
        card_00 = card_00 + 1;
      } while (card_00 < active_cards_count[result]);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x264dfa0; original cards/exodus.c:3753. */
int card_fugue(int player, int card, event_t event)
{
  /* Fugue	|3|B|B
   * Sorcery
   * Target player discards three cards. */

  card_instance_t *instance;
  target_definition_t td1;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td1, 0);
  td1.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td1))
    {
      new_multidiscard(instance->targets[0].player, 3, 0, player);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td1, "TARGET_PLAYER", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264e100; original cards/exodus.c:3779. */
int card_furnace_brood(int player, int card, event_t event)
{
  /* Furnace Brood	|3|R
   * Creature - Elemental 3/3
   * |R: Target creature can't be regenerated this turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event != EVENT_ACTIVATE && 1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION) && event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  if (player == 1)
    td.required_state = TARGET_STATE_DESTROYED;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    cannot_regenerate_until_eot(instance->parent_controller, instance->parent_card,
                                instance->targets[0].player, instance->targets[0].card);
  result = player == 1 ? (GAA_REGENERATION | GAA_CAN_TARGET | GAA_UNTAPPED) : 5;
  return generic_activated_ability(player, card, event, result, 0, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}
#endif

/* ManalinkEh 2022: 0x264e2b0; original cards/exodus.c:3807. */
int card_grollub(int player, int card, event_t event)
{
  /* Grollub	|2|B
   * Creature - Beast 3/3
   * Whenever this creature is dealt damage, each opponent gains that much life. */

  int result;
  card_instance_t *instance;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {
    result = damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      gain_life(1 - player, instance->targets[7].card);
      instance->targets[7].card = 0;
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x264e350; original cards/exodus.c:3822. */
int card_mage_il_vec(int player, int card, event_t event)
{
  /* Mage il-Vec	|2|R
   * Creature - Human Wizard 2/2
   * |T, Discard a card at random: This creature deals 1 damage to any target. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x25, 0, 0, 0, 0, 0, 0,
                                         0, &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
      if (result == 0)
      {
        return 0;
      }
      result = pick_target(&td, "TARGET_ANY");
      if (result == 0)
      {
        return 0;
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      add_state(player, card, 0x10);
      discard(player, 1, player);
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
    get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    damage_target0(player, card, 1);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264e540; original cards/exodus.c:3861. */
int card_maniacal_rage(int player, int card, event_t event)
{
  /* Maniacal Rage	|1|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and can't block. */

  int result;

  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_CANNOT_BLOCK);
  attached_gets_pt(player, card, event, 2, 2);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264e5c0; original cards/exodus.c:3873. */
int card_medicine_bag(int player, int card, event_t event)
{
  /* Medicine Bag	|3
   * Artifact
   * |1, |T, Discard a card: Regenerate target creature. */

  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_ACTIVATE && 1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION) && event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  td.required_state = TARGET_STATE_DESTROYED;
  if (player == 1)
    td.special = TARGET_SPECIAL_REGENERATION;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) &&
      can_be_regenerated(instance->targets[0].player, instance->targets[0].card))
    regenerate_target(instance->targets[0].player, instance->targets[0].card);
  return generic_activated_ability(player, card, event,
                                   (GAA_REGENERATION | GAA_DISCARD | GAA_CAN_TARGET | GAA_UNTAPPED),
                                   1, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}
#endif

/* ManalinkEh 2022: 0x264e750; original cards/exodus.c:3901. */
int card_mirozel(int player, int card, event_t event)
{
  /* Mirozel	|3|U
   * Creature - Illusion 2/3
   * Flying
   * When this creature becomes the target of a spell or ability, return this creature to its owner's hand. */

  card_instance_t *instance;
  const target_t *targeting_source;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      targeting_source = becomes_target_of_spell_or_effect(player, card, event, player, card, 2);
      if (targeting_source != NULL)
      {
        bounce_permanent(player, card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x264e7d0; original cards/exodus.c:3915. */
int card_mogg_assassin(int player, int card, event_t event)
{
  /* Mogg Assassin	|2|R
   * Creature - Goblin Assassin 2/1
   * |T: You choose target creature an opponent controls, and that opponent chooses target creature. Flip a coin. If you win the flip, destroy the creature you chose. If you lose the flip, destroy the creature your opponent chose. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  target_definition_t td2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    result = 1 - player;
    td.allowed_controller = result;
    td.preferred_controller = result;
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    td2.who_chooses = result;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_target(&td2);
      if (result == 0)
      {
        return 0;
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0,
                                         &td, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      result = can_target(&td2);
      if (result == 0)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                "Select target creature an opponent controls.");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      new_pick_target(&td2, "Select target creature.", 1, 0x81);
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
    result = 1 - player;
    td.allowed_controller = result;
    td.preferred_controller = result;
    default_target_definition(player, card, &td2, 2);
    td2.preferred_controller = player;
    td2.who_chooses = result;
    instance = get_card_instance(player, card);
  }
  if (((event == EVENT_RESOLVE_ACTIVATION) &&
       (result = validate_target(player, card, &td, 0), result != 0)) &&
      (result = validate_target(player, card, &td2, 1), result != 0))
  {
    result = flip_a_coin(player, card);
    if (result == 0)
    {
      kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
      return 0;
    }
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x264eaf0; original cards/exodus.c:3962. */
int card_monstrous_hound(int player, int card, event_t event)
{
  /* Monstrous Hound	|3|R
   * Creature - Dog 4/4
   * This creature can't attack unless you control more lands than defending player.
   * This creature can't block unless you control more lands than attacking player. */

  int own_lands_or_result;
  int opponent_lands;

  own_lands_or_result = is_humiliated(player, card, AL_STATIC);
  if (((own_lands_or_result == 0) &&
       (((event == EVENT_ATTACK_LEGALITY || (event == EVENT_BLOCK_LEGALITY)) &&
         (card == affected_card)))) &&
      (player == affected_card_controller))
  {
    own_lands_or_result = count_subtype(player, 1, -1);
    opponent_lands = count_subtype(1 - player, 1, -1);
    if (own_lands_or_result <= opponent_lands)
    {
      event_result = 1;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x264eb90; original cards/exodus.c:3986. */
int card_necrologia(int player, int card, event_t event)
{
  /* Necrologia	|3|B|B
   * Instant
   * Cast this spell only during your end step.
   * As an additional cost to cast this spell, pay X life.
   * Draw X cards. */

  card_instance_t *instance;
  int life_payment_or_result;
  int opponent_attack_power;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    if (((current_phase == 0x1f) && (current_turn == player)) &&
        (life_payment_or_result = can_pay_life_as_cost_for_spells_or_activated_abilities(player, 1),
         life_payment_or_result != 0))
    {
      life_payment_or_result = basic_spell(player, card, EVENT_CAN_CAST);
      return (unsigned int)(life_payment_or_result != 0);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        ((instance = get_card_instance(player, card), life_payment_or_result = life[1],
          card == affected_card && (player == affected_card_controller))))
    {

      if (player == 1)
      {
        opponent_attack_power = get_attack_power(0, card);
        life_payment_or_result = life_payment_or_result - opponent_attack_power;
      }
      else
      {
        life_payment_or_result = choose_a_number(player, "Pay how much life?", life[player]);
      }
      if (life[player] < life_payment_or_result)
      {
        life_payment_or_result = life[player] + -1;
      }
      if (life_payment_or_result < 1)
      {
        spell_fizzled = 1;
      }
      else
      {
        lose_life(player, life_payment_or_result);
        instance->info_slot = life_payment_or_result;
      }
    }
  }
  else if ((((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
            (event == EVENT_CHANGE_TARGET)) &&
           (instance = get_card_instance(player, card), event == EVENT_RESOLVE_SPELL))
  {
    draw_cards(player, instance->info_slot);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264ed40; original cards/exodus.c:4034. */
int card_paroxysm(int player, int card, event_t event)
{
  /* Paroxysm	|1|R
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of the upkeep of enchanted creature's controller, that player reveals the top card of their library. If that card is a land card, destroy that creature. Otherwise, it gets +3/+3 until end of turn. */

  int *deck;
  int revealed_iid;
  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player != -1)
    {

      instance = get_card_instance(player, card);
      upkeep_trigger_ability(player, card, event, (int)instance->damage_target_player);
      if ((event == EVENT_UPKEEP_TRIGGER_ABILITY) &&
          (result = (int)instance->damage_target_player, result == current_turn))
      {

        deck = deck_ptr[result];
        revealed_iid = *deck;
        if (revealed_iid != -1)
        {
          reveal_card_iid(result, card, revealed_iid);
          result = is_what(-1, *deck, 1);
          if (result == 0)
          {
            pump_until_eot(player, card, current_turn, instance->damage_target_card, 3, 3);
          }
          else
          {
            kill_card(current_turn, instance->damage_target_card, KILL_DESTROY);
          }
        }
      }
    }
  }
  return aura(player, card, event, NULL, NULL);
}
#endif

/* ManalinkEh 2022: 0x2642860; original cards/exodus.c:4065. */
static int source_damage_prevented_if_black_red_damage_source_controller(int player, int card,
                                                                         event_t event)
{
  card_instance_t *damage;
  card_instance_t *instance;
  unsigned int source_colors;
  unsigned int required_colors;

  damage = damage_being_dealt(event);
  instance = get_card_instance(player, card);
  if (((damage != NULL) && ((int)damage->damage_source_player == instance->targets[0].player)) &&
      (damage->damage_source_card == instance->targets[0].card))
  {
    source_colors = get_color((int)damage->damage_source_player, damage->damage_source_card);
    required_colors = get_sleighted_color_test(player, card, 0x12);
    if (((source_colors & required_colors) != 0) && (-1 < damage->info_slot))
    {
      damage->info_slot = 0;
    }
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264ee80; original cards/exodus.c:4081. */
int card_penance(int player, int card, event_t event)
{
  /* Penance	|2|W
   * Enchantment
   * Put a card from your hand on top of your library: The next time a black or red source of your choice would deal damage this turn, prevent that damage. */

  int target_card;
  card_instance_t *instance;
  int result;
  int legacy_card;
  target_definition_t td;
  test_definition_t this_test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0xff);
    td.preferred_controller = 1 - player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (hand_count[player] < 1)
      {
        return 0;
      }
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x400, 0, 0, 0, 0,
                                              0, 0, 0, NULL, NULL);
      if (result == 0)
      {
        result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 0, 0, 0, 0, 0,
                                                0, 0, &td, NULL);
        if (result == 0)
        {
          return 0;
        }
        return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
      }
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      if (hand_count[player] < 1)
      {
        return 0;
      }
      instance->info_slot = 0;
      new_default_test_definition(&this_test, 0xff,
                                       "Select a card to put on top of your library");
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x400, 0, 0, 0, 0, 0, 0,
                                              0, NULL, NULL);
      if (result == 0)
      {
        result = new_global_tutor(player, player, 4, 10, 0, 3, &this_test);
        if (result == -2)
        {
          return 0;
        }
        result = generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 0, 0, 0, 0, 0, 0,
                                                0, &td, "TARGET_ANY");
        return result;
      }
      if (spell_fizzled == 1)
      {
        return 0;
      }
      result = new_global_tutor(player, player, 4, 10, 0, 3, &this_test);
      if (result == -2)
      {
        return 0;
      }
      counterspell(player, card, EVENT_ACTIVATE, NULL, 0);
      ((unsigned char *)&instance->info_slot)[0] = 1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    default_target_definition(player, card, &td, 0xff);
    td.preferred_controller = 1 - player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    if (instance->info_slot == 1)
    {
      result = counterspell_validate(player, card, NULL, 0);
    }
    else
    {
      result = valid_target(&td);
    }
    if (result != 0)
    {
      result = instance->targets[0].player;
      target_card = instance->targets[0].card;
      if (result != -1)
      {
        if (instance->info_slot == 1)
        {
          legacy_card = create_legacy_effect(
              player, card, source_damage_prevented_if_black_red_damage_source_controller);
          instance = get_card_instance(player, legacy_card);
          instance->targets[0].player = result;
          instance->targets[0].card = target_card;
          return 0;
        }
        create_targetted_legacy_effect(
            player, card, source_damage_prevented_if_black_red_damage_source_controller, result,
            target_card);
        return 0;
      }
    }
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x264f330; original cards/exodus.c:4159. */
int card_plated_rootwalla(int player, int card, event_t event)
{
  /* Plated Rootwalla	|4|G
   * Creature - Lizard 3/3
   * |2|G: This creature gets +3/+3 until end of turn. Activate only once each turn. */

  int result;
  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 2, 0, 0, 1, 0, 0,
                                       0, NULL, NULL);
    if (result != 0)
    {
      return 2;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = generic_shade(player, card, EVENT_RESOLVE_ACTIVATION, 0, 1, 0, 0, 1, 0, 0, 3, 3, 0, 0);
    return result;
  }
  return generic_activated_ability(player, card, event, 0x2000, 2, 0, 0, 1, 0, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x264f4b0; original cards/exodus.c:4178. */
int card_predatory_hunger(int player, int card, event_t event)
{
  /* Predatory Hunger	|G
   * Enchantment - Aura
   * Enchant creature
   * Whenever an opponent casts a creature spell, put a +1/+1 counter on enchanted creature. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player != -1)
    {
      sp_ability_trigger(1 - player, event, 2, TYPE_CREATURE, NULL, player, card);
      if (event == EVENT_SPA)
      {

        instance = get_card_instance(player, card);
        add_counters(player, card, (int)instance->damage_target_player,
                                 instance->damage_target_card, COUNTER_P1_P1, 1);
      }
    }
  }
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x264f590; original cards/exodus.c:4198. */
int card_rabid_wolverines(int player, int card, event_t event)
{
  /* Rabid Wolverines	|3|G|G
   * Creature - Wolverine 4/4
   * Whenever this creature becomes blocked by a creature, this creature gets +1/+1 until end of turn. */

  pump_when_blocked(player, card, event, 1, 1, 0);
  return 0;
}

/* ManalinkEh 2022: 0x264f5d0; original cards/exodus.c:4212. */
int card_reckless_ogre(int player, int card, event_t event)
{
  /* Reckless Ogre	|3|R
   * Creature - Ogre 3/2
   * Whenever this creature attacks alone, it gets +3/+0 until end of turn. */

  dat_ability(player, card, event, DAT_ATTACKS_ALONE | DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }
  pump_until_eot_merge_previous(player, card, player, card, 3, 0);
  return 0;
}

/* ManalinkEh 2022: 0x264f650; original cards/exodus.c:4231. */
int card_rootwater_alligator(int player, int card, event_t event)
{
  /* Rootwater Alligator	|3|G
   * Creature - Crocodile 3/2
   * Sacrifice a Forest: Regenerate this creature. */

  const char *sacrifice_prompt;
  test_definition_t test;

  if (event == EVENT_ACTIVATE)
  {
    sacrifice_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice", 0x5001);
    new_default_test_definition(&test, 0x47, sacrifice_prompt);
    test.subtype = get_hacked_subtype(player, card, 0x5001);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    sacrifice_prompt = get_hacked_land_text(player, card, "Select a %s to sacrifice", 0x5001);
    new_default_test_definition(&test, 0x47, sacrifice_prompt);
    test.subtype = get_hacked_subtype(player, card, 0x5001);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      return regeneration(player, card, EVENT_RESOLVE_ACTIVATION, 0, 0, 0, 0, 0, 0);
    }
  }
  return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_REGENERATION, 0,
                                                          0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                          &test, SARV_0);
}

/* ManalinkEh 2022: 0x264f810; original cards/exodus.c:4252. */
int card_shield_mate(int player, int card, event_t event)
{
  /* Shield Mate	|W
   * Creature - Human Soldier 1/1
   * Sacrifice this creature: Target creature gets +0/+4 until end of turn. */

  if (event == EVENT_CHECK_PUMP)
  {
    if (card_shield_mate(player, card, EVENT_CAN_ACTIVATE))
      pumpable_toughness[player] += 4;
  }
  else if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = player;
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 4);
    return generic_activated_ability(player, card, event, 0xc, 0, 0, 0, 0, 0, 0, 0, &td,
                                       "TARGET_CREATURE");
  }
  return 0;
}

/* ManalinkEh 2022: 0x264f970; original cards/exodus.c:4283. */
int card_song_of_serenity(int player, int card, event_t event)
{
  /* Song of Serenity	|1|G
   * Enchantment
   * Creatures that are enchanted can't attack or block. */

  int result;

  if (((((uint32_t)(event - EVENT_BLOCK_LEGALITY) < 2) || (event == EVENT_ABILITIES)) &&
       (result = is_enchanted(affected_card_controller, affected_card), result != 0)) &&
      ((result = is_humiliated(player, card, AL_STATIC),
        result == 0 &&
            ((1 < (uint32_t)(event - EVENT_BLOCK_LEGALITY) || (event_result = 1, event == EVENT_ABILITIES))))))
  {
    cannot_attack(affected_card_controller, affected_card, EVENT_ABILITIES);
    cannot_block(affected_card_controller, affected_card, EVENT_ABILITIES);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x264fa40; original cards/exodus.c:4302. */
int card_thopter_squadron(int player, int card, event_t event)
{
  /* Thopter Squadron	|5
   * Artifact Creature - Thopter 0/0
   * Flying
   * This creature enters with three +1/+1 counters on it.
   * |1, Remove a +1/+1 counter from this creature: Create a 1/1 colorless Thopter artifact creature token with flying. Activate only as a sorcery.
   * |1, Sacrifice another Thopter: Put a +1/+1 counter on this creature. Activate only as a sorcery. */

  enum
  {
    CHOICE_CREATE_THOPTER = 1,
    CHOICE_SAC_THOPTER = 2
  };

  int choice;
  int create_available_or_mode;
  int sacrifice_available;
  unsigned int thopter_sacrifice_available;
  card_instance_t *instance;
  test_definition_t test;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    new_default_test_definition(&test, 0x47, "Select another Thopter to sacrifice.");
    test.subtype = 0xc6;
    test.not_me = 1;
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x10, 1, 0, 0, 0, 0, 0,
                                    0x1000100, NULL, NULL) == 0)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x10, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          return 0;
        }
        if (new_can_sacrifice_as_cost(player, card, &test) == 0)
        {
          return 0;
        }
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      if (can_sorcery_be_played(player, EVENT_ACTIVATE) == 0)
      {
        return 0;
      }

      create_available_or_mode = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1,
                                                           0, 0, 0, 0, 0, 0x1000100, NULL, NULL);
      sacrifice_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0,
                                                      0, 0, 0, 0, NULL, NULL);
      thopter_sacrifice_available = 0;
      if (sacrifice_available != 0)
      {
        sacrifice_available = new_can_sacrifice_as_cost(player, card, &test);
        thopter_sacrifice_available = (unsigned int)(sacrifice_available != 0);
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Create Thopter",
                           create_available_or_mode, 2, "Sac Thopter add counter on ~",
                           thopter_sacrifice_available,
                           (unsigned int)(current_phase == 0x18) * 9 + 1, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = choice;
      if (choice == CHOICE_CREATE_THOPTER)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0x1000100,
                                  NULL, NULL);
        return 0;
      }
      if (choice != CHOICE_SAC_THOPTER)
      {
        return 0;
      }
      charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      new_sacrifice(player, card, player, SAC_AS_COST, &test);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    new_default_test_definition(&test, 0x47, "Select another Thopter to sacrifice.");
  }
  test.not_me = 1;
  test.subtype = 0xc6;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    create_available_or_mode = instance->info_slot;
    if (create_available_or_mode == CHOICE_CREATE_THOPTER)
    {
      generate_token_by_id(player, card, CARD_ID_THOPTER);
      create_available_or_mode = instance->info_slot;
    }
    if (create_available_or_mode == CHOICE_SAC_THOPTER)
    {
      add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_P1_P1,
                   1);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x264ff50; original cards/exodus.c:4378. */
int card_wayward_soul(int player, int card, event_t event)
{
  /* Wayward Soul	|2|U|U
   * Creature - Spirit 3/2
   * Flying
   * |U: Put this creature on top of its owner's library. */

  card_instance_t *instance;
  card_instance_t *parent_in_play;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    parent_in_play = in_play(instance->parent_controller, instance->parent_card);
    if (parent_in_play != NULL)
    {
      put_on_top_of_deck(instance->parent_controller, instance->parent_card);
    }
  }
  return generic_activated_ability(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
}
