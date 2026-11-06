#include "manalink.h"
int card_horizon_stone(int,int,event_t);

// Unimplemented: Worst Fears; the interesting part of Battlefield Thaumaturge

/****************
* Set mechanics *
****************/

void constellation(int player, int card, event_t event)
{
  // Constellation - Whenever ~ or another enchantment enters the battlefield under your control, ...

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		test.not_me = 1;
		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}
}

static int strive(int player, int card, event_t event, target_definition_t *td, const char *prompt,
                  int cless, int black, int blue, int green, int red, int white)
{
  int i;
  target_t *target_player_entry;
  target_t *target_card_entry;
  uint8_t selected_target_count;
  card_instance_t *instance;
  int next_target = 0;
  int result;
  int ret_location;
  td->allow_cancel = 3;
  instance = get_card_instance(player, card);
  instance->info_slot = 0;
  instance->number_of_targets = '\0';
  while (true)
  {
    ret_location = next_target;
    load_text(NULL, (char *)prompt);
    if (new_pick_target(td, (char *)prompt, ret_location, 0) == 0)
    {
      selected_target_count = instance->number_of_targets;
      if (instance->targets[ret_location].card == -1)
        spell_fizzled = 1;
      break;
    }
    if (ret_location < 1)
    {
      add_state(instance->targets[ret_location].player, instance->targets[ret_location].card,
                0x300000);
      next_target = ret_location + 1;
      if (ret_location + 1 < 1)
        continue;
    }
    else
    {
      if (charge_mana_multi(player, (int)cless, black, blue, green, red, white) == 0)
      {
        selected_target_count = instance->number_of_targets;
        break;
      }
      add_state(instance->targets[ret_location].player, instance->targets[ret_location].card,
                0x300000);
    }
    ret_location = ret_location + 1;
    result = is_token(player, card);
    if (result != 0 ||
        (result = has_mana_multi(player, (int)cless, black, blue, green, red, white),
         result == 0) ||
        (result = can_target(td), result == 0))
    {
      selected_target_count = instance->number_of_targets;
      break;
    }
    next_target = ret_location;
  }
  if (selected_target_count != 0)
  {
    i = 0;
    do
    {
      target_card_entry = instance->targets + i;
      target_player_entry = instance->targets + i;
      i = i + 1;
      remove_state(target_player_entry->player, target_card_entry->card, 0x300000);
    } while (i < (int)(unsigned int)instance->number_of_targets);
  }
  if (spell_fizzled != 1)
  {
    if (player == 1 && ret_location == 0)
      ai_modifier = ai_modifier - 0x30;
    return ret_location;
  }
  instance->number_of_targets = '\0';
  return 0;
}

/********
* Cards *
********/

/********
* White *
********/

/* Aegis of the Gods	|1|W => Leyline of Sanctity
 * Enchantment Creature - Human Soldier 2/1
 * You have hexproof. */

int card_ajanis_presence(int player, int card, event_t event){

	/* Ajani's Presence	|W
	 * Instant
	 * Strive - ~ costs |2|W more to cast for each target beyond the first.
	 * Any number of target creatures each get +1/+1 and gain indestructible until end of turn. */

	if (event == EVENT_CHECK_PUMP && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id)){
		pumpable_power[player] += 1;
		pumpable_toughness[player] += 99;
	}

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", 2, 0, 0, 0, 0, 1);
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 1, 1, 0, SP_KEYWORD_INDESTRUCTIBLE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* Akroan Mastiff	|3|W => Master Decoy
 * Creature - Hound 2/2
 * |W, |T: Tap target creature. */

int card_armament_of_nyx(int player, int card, event_t event)
{
  /* Armament of Nyx	|2|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has double strike as long as it's an enchantment. Otherwise, prevent all damage that would be dealt by enchanted creature. */

  if (event == EVENT_CHECK_ABILITIES){
	card_instance_t* instance = get_card_instance(player, card);
	if (affect_me(instance->damage_target_player, instance->damage_target_card) && in_play(player, card) && !is_what(instance->damage_target_player, instance->damage_target_card, TYPE_ENCHANTMENT)){
		check_abilities_power -= get_power(instance->damage_target_player, instance->damage_target_card);
	}
  }
  if (event == EVENT_SHOULD_AI_PLAY || event == EVENT_ABILITIES || event == EVENT_PREVENT_DAMAGE)
	{
	  card_instance_t* instance = in_play(player, card);
	  if (!instance)
		return 0;
	  int p = instance->damage_target_player, c = instance->damage_target_card;
	  if (p < 0 || c < 0 || is_humiliated(player, card, AL_STATIC))
		return 0;

	  if (is_what(p, c, TYPE_ENCHANTMENT))
		{
		  if (event == EVENT_SHOULD_AI_PLAY)
			ai_modifier += (p == AI) ? 96 : -96;

		  if (event == EVENT_ABILITIES && affect_me(p, c))
			event_result |= KEYWORD_DOUBLE_STRIKE;
		}
	  else
		{
		  if (event == EVENT_SHOULD_AI_PLAY)
			ai_modifier += (p == AI) ? -48 : 48;

		  prevent_all_my_damage(p, c, event, 0);
		}
	}

  if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

  return vanilla_aura(player, card, event, ANYBODY);
}

int card_banishing_light(int player, int card, event_t event){

	/* Banishing Light	|2|W
	 * Enchantment
	 * When ~ enters the battlefield, exile target nonland permanent an opponent controls until ~ leaves the battlefield. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	return_from_oblivion(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;
		td.illegal_type = TYPE_LAND;

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			card_instance_t* instance = get_card_instance(player, card);
			obliviation(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		else if (player == AI) {
			ai_modifier -= 48;
		}
	}

	return 0;
}

int card_dawnbringer_charioteers(int player, int card, event_t event){

	/* Dawnbringer Charioteers	|2|W|W
	 * Creature - Human Soldier 2/4
	 * Flying, lifelink
	 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

	lifelink(player, card, event);

	if( heroic(player, card, event) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	return 0;
}

int card_deicide(int player, int card, event_t event)
{
  /* Deicide	|1|W
   * Instant
   * Exile target enchantment. If the exiled card is a God card, search its controller's graveyard, hand, and library for any number of cards with the same
   * name as that card and exile them, then that player shuffles his or her library. */

  if (!IS_GS_EVENT(player, card, event))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      card_instance_t *instance = get_card_instance(player, card);
      int find_more = has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_GOD);
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      if (find_more)
      {
        int id = get_id(instance->targets[0].player, instance->targets[0].card);
        lobotomy_effect(player, instance->targets[0].player, id, -1, 0);
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

int card_dictate_of_heliod(int player, int card, event_t event)
{
  /* Dictate of Heliod	|3|W|W
   * Enchantment
   * Flash
   * Creatures you control get +2/+2. */

  if (event == EVENT_CHECK_PUMP && !in_play(player, card) && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id))
	{
	  pumpable_power[player] += 2;
	  pumpable_toughness[player] += 2;
	}

  boost_creature_type(player, card, event, -1, 2, 2, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
  return flash(player, card, event);
}

/* Eagle of the Watch	|2|W => Serra Angel
 * Creature - Bird 2/1
 * Flying, vigilance */

/* Eidolon of Rhetoric	|2|W => Arcane Laboratory
 * Enchantment Creature - Spirit 1/4
 * Each player can't cast more than one spell each turn. */

int card_font_of_vigor(int player, int card, event_t event){
	/* Font of Vigor	|1|W
	 * Enchantment
	 * |2|W, Sacrifice ~: You gain 7 life. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 7);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XW(2, 1), 0, NULL, NULL);
}

static const char* is_blocking_me(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  card_instance_t* godsend = get_card_instance(targeting_player, targeting_card);
  if (is_blocking(card, godsend->damage_target_card))
	return NULL;
  else
	return "blocking equipped creature";
}

static const char* im_blocking_him(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  card_instance_t* godsend = get_card_instance(targeting_player, targeting_card);
  if (is_blocking(godsend->damage_target_card, card))
	return NULL;
  else
	return "blocked by equipped creature";
}

int card_godsend(int player, int card, event_t event)
{
	/* Godsend	|1|W|W
	 * Legendary Artifact - Equipment
	 * Equipped creature gets +3/+3.
	 * Whenever equipped creature blocks or becomes blocked by one or more creatures, you may exile one of those creatures.
	 * Opponents can't cast cards with the same name as cards exiled with Godsend.
	 * Equip |3 */

	// (Card number is 12, among the white cards, not the artifacts.  Peculiar.)

	check_legend_rule(player, card, event);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == 1-player ){
		if( exiledby_find(player, card, get_card_instance(affected_card_controller, affected_card)->original_internal_card_id, NULL, NULL) &&
			!is_what(affected_card_controller, affected_card, TYPE_LAND) )
		{
			infinite_casting_cost();
		}
	}

	if (event == EVENT_DECLARE_BLOCKERS && is_equipping(player, card)){
		card_instance_t *instance = get_card_instance(player, card);
		// Annoyingly, STATE_ISBLOCKED isn't set until after EVENT_DECLARE_BLOCKERS.
		int p = instance->damage_target_player;

		const char* prompt;

		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = 1-p;
		td.allowed_controller = 1-p;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		if (current_turn == p){
			td.extra = target_predicate_extra(is_blocking_me);
			prompt = "Select a creature blocking creature equipped by Godsend.";
		}
		else{
			td.extra = target_predicate_extra(im_blocking_him);
			prompt = "Select a creature blocked by creature equipped by Godsend.";
		}

		instance->number_of_targets = 0;
		if( can_target(&td) && pick_next_target_noload(&td, prompt) ){
			instance->number_of_targets = 0;
			int iid = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
			exiledby_remember(player, card, 0, iid, NULL, NULL);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			/* STATE_ISBLOCKED isn't set until after EVENT_DECLARE_BLOCKERS, so exiling the only blocker would allow this creature to
			 * deal combat damage if we don't set it manually - see #14389 */
			if (current_turn == p){
				get_card_instance(p, instance->damage_target_card)->state |= STATE_ISBLOCKED;
			}
		}
	}

	return vanilla_equipment(player, card, event, 3, 3, 3, 0, 0);
}

int card_harvestguard_alseids(int player, int card, event_t event){

	/* Harvestguard Alseids	|2|W
	 * Enchantment Creature - Nymph 2/3
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, prevent all damage that would be dealt to target creature
	 * this turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE")){
			prevent_all_damage_to_target(player, card, instance->targets[0].player, instance->targets[0].card, 1);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

/* Lagonna-Band Trailblazer	|W => Fabled Hero
 * Creature - Centaur Scout 0/4
 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

static int launch_the_fleet_legacy(int player, int card, event_t event)
{
  if (get_card_instance(player, card)->damage_target_player >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    store_attackers(player, card, event, DAT_DONT_REPORT_VALUE, instance->damage_target_player,
                    instance->damage_target_card, NULL);
    dat_ability(player, card, event, DAT_DONT_REPORT_VALUE | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      token_generation_t token;
      default_token_definition(instance->damage_target_player, instance->damage_target_card,
                               CARD_ID_SOLDIER, &token);
      token.action = TOKEN_ACTION_ATTACKING;
      generate_token(&token);
    }
  }
  if (event == EVENT_CLEANUP)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_launch_the_fleet(int player, int card, event_t event){

	/* Launch the Fleet	|W
	 * Sorcery
	 * Strive - ~ costs |W more to cast for each target beyond the first.
	 * Until end of turn, any number of target creatures each gain "Whenever this creature attacks, put a 1/1 white Soldier creature token onto the battlefield
	 * tapped and attacking." */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event, &td, "TARGET_CREATURE", MANACOST_X(1));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				create_targetted_legacy_effect(player, card, &launch_the_fleet_legacy, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static const char* target_is_enchantment(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return is_what(player, card, TYPE_ENCHANTMENT) ? NULL : gs_target_error_type;	// ",type"
}
int card_leonin_iconoclast(int player, int card, event_t event){
	/* Leonin Iconoclast	|3|W
	 * Creature - Cat Monk 3/2
	 * Heroic - Whenever you cast a spell that targets ~, destroy target enchantment creature an opponent controls. */

	if( heroic(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1 - player;
		td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		td.extra = target_predicate_extra(target_is_enchantment);	// in addition to creature
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_next_target_noload(&td, "Select target enchantment creature an opponent controls.") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static int common_mortal_obstinacy_flamespeakers_will(int player, int card, event_t event,
                                                      type_t type, const char *prompt)
{
  /* Enchantment - Aura
   * Enchant creature you control
   * Enchanted creature gets +1/+1.
   * Whenever enchanted creature deals combat damage to a player, you may sacrifice ~. If you do,
   * destroy target [type]. */

  card_instance_t *instance = in_play(player, card);
  if (instance)
  {
    int p = instance->damage_target_player;
    int c = instance->damage_target_card;

    if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c))
    {
      ++event_result;
    }

    if (damage_dealt_by_me_arbitrary(p, c, event,
                                     DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_MUST_DAMAGE_PLAYER |
                                         DDBM_TRIGGER_OPTIONAL,
                                     player, card))
    {
      target_definition_t td1;
      default_target_definition(player, card, &td1, type);
      /* The AI always activates optional triggers; here, that's a very bad idea if there's nothing
       * the AI would want to target.  So make the targeting optional, and if the AI decides not to
       * target, pretend it decided not to trigger in the first place. */
      if (!IS_AI(player))
      {
        td1.allow_cancel = 0;
      }

      instance->number_of_targets = 0;
      if (can_target(&td1) && pick_target(&td1, prompt))
      {
        kill_card(player, card, KILL_SACRIFICE);
        if (valid_target(&td1))
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      else if (!IS_AI(player))
      {
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
  }

  if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS)
  {
    return 0;
  }

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.allowed_controller = player;

  return targeted_aura_custom_prompt(player, card, event, &td,
                                     "Select target creature you control.");
}

int card_mortal_obstinacy(int player, int card, event_t event){

	/* Mortal Obstinacy	|W
	 * Enchantment - Aura
	 * Enchant creature you control
	 * Enchanted creature gets +1/+1.
	 * Whenever enchanted creature deals combat damage to a player, you may sacrifice ~. If you do, destroy target enchantment. */

	return common_mortal_obstinacy_flamespeakers_will(player, card, event, TYPE_ENCHANTMENT, "TARGET_ENCHANTMENT");
}

int card_nyx_fleece_ram(int player, int card, event_t event)
{
  /* Nyx-Fleece Ram	|1|W ==> Ajani's Mantra
   * Enchantment Creature - Sheep 0/5
   * At the beginning of your upkeep, you gain 1 life. */

  upkeep_trigger_ability(player, card, event, player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	gain_life(player, 1);

  return global_enchantment(player, card, event);
}

int card_oppressive_rays(int player, int card, event_t event)
{
  /* Oppressive Rays	|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack or block unless its controller pays |3.
   * Activated abilities of enchanted creature cost |3 more to activate. */

  int32_t attached_card;
  int card_00;
  int aura_result;
  card_instance_t *instance;
  int attached_player_or_result;

  aura_result = vanilla_aura(player, card, event, 1 - player);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (spell_fizzled != 1)
    {
      instance = get_card_instance(player, card);
      set_cost_mod_for_activated_abilities((int)instance->damage_target_player,
                                           instance->damage_target_card, 4, NULL);
      attached_card = instance->targets[0].card;
      instance->targets[1].player = instance->targets[0].player;
      instance->targets[1].card = attached_card;
    }
  }
  else if ((((event == EVENT_ABILITIES) && (card == affected_card)) &&
            (player == affected_card_controller)) &&
           (instance = get_card_instance(player, card), -1 < instance->targets[1].player))
  {
    instance = get_card_instance(player, card);
    attached_player_or_result = instance->targets[1].player;
    if ((instance->damage_target_player != attached_player_or_result) &&
        (card_00 = instance->targets[1].card, instance->damage_target_player != card_00))
    {
      remove_cost_mod_for_activated_abilities(attached_player_or_result, card_00, 4, NULL);
      set_cost_mod_for_activated_abilities((int)instance->damage_target_player,
                                           instance->damage_target_card, 4, NULL);
      instance->targets[1].player = (int)instance->damage_target_player;
      instance->targets[1].card = instance->damage_target_card;
    }
  }
  bool tax_context = trigger_condition == TRIGGER_PAY_TO_ATTACK
                         ? current_phase == 0x15 && reason_for_trigger_controller == current_turn
                         : trigger_condition == TRIGGER_PAY_TO_BLOCK && current_phase == 0x17 &&
                               reason_for_trigger_controller != current_turn;
  if (tax_context && card == affected_card && player == affected_card_controller &&
      (instance = in_play(player, card), instance != NULL) && instance->info_slot == 0 &&
      instance->damage_target_card == trigger_cause &&
      instance->damage_target_player == trigger_cause_controller)
  {
    bool prevent_combat = false;
    if (has_mana(reason_for_trigger_controller, 7, 3) == 0)
    {
      forbid_attack = 1;
      prevent_combat = true;
    }
    else if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
      prevent_combat = forbid_attack != 0;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      if (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                      reason_for_trigger_controller, COLOR_COLORLESS, 3) == 0)
      {
        forbid_attack = 1;
        spell_fizzled = 0;
        prevent_combat = true;
      }
      else
      {
        forbid_attack = 0;
        instance->info_slot = 1;
      }
    }
    else
      prevent_combat = forbid_attack != 0;
    if (prevent_combat)
    {
      remove_state((int)instance->damage_target_player, instance->damage_target_card, 0x8000);
      instance->blocking = 0xff;
    }
  }
  if ((uint32_t)(event - EVENT_BLOCK_LEGALITY) < 2)
  {
    instance = get_card_instance(player, card);
    if (((instance->damage_target_card == affected_card) &&
         (instance->damage_target_player == affected_card_controller)) &&
        (instance->info_slot == 0))
    {
      attached_player_or_result = current_turn;
      if (event != EVENT_ATTACK_LEGALITY)
      {
        attached_player_or_result = 1 - current_turn;
      }
      if (has_mana(attached_player_or_result, 7, 3) == 0)
      {
        event_result = 1;
      }
    }
    leaves_play(player, card, event, 0, 0);
    return aura_result;
  }
  if ((event == EVENT_CLEANUP) || (event == EVENT_SHOULD_AI_PLAY))
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  leaves_play(player, card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    instance = get_card_instance(player, card);
    remove_cost_mod_for_activated_abilities((int)instance->damage_target_player,
                                            instance->damage_target_card, 4, NULL);
    return aura_result;
  }
  return aura_result;
}

/* Oreskos Swiftclaw	|1|W => vanilla
 * Creature - Cat Warrior 3/1 */

int card_phalanx_formation(int player, int card, event_t event){

	/* Phalanx Formation	|2|W
	 * Instant
	 * Strive - ~ costs |1|W more to cast for each target beyond the first.
	 * Any number of target creatures each gain double strike until end of turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", 1, 0, 0, 0, 0, 1);
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 0, 0, KEYWORD_DOUBLE_STRIKE, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_quarry_colossus(int player, int card, event_t event){
	/* Quarry Colossus	|5|W|W
	 * Creature - Giant 5/6
	 * When ~ enters the battlefield, put target creature into its owner's library just beneath the top X cards of that library, where X is the number of
	 * |H2Plains you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			int amount = basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_WHITE)];
			put_permanent_under_the_first_x_card_of_its_owners_library(instance->targets[0].player, instance->targets[0].card, amount);
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

/* Reprisal	|1|W => alliances.c
 * Instant
 * Destroy target creature with power 4 or greater. It can't be regenerated. */

int card_sightless_brawler(int player, int card, event_t event){

	/* Sightless Brawler	|1|W
	 * Enchantment Creature - Human Warrior 3/2
	 * Bestow |4|W
	 * ~ can't attack alone.
	 * Enchanted creature gets +3/+2 and can't attack alone. */

	if( get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		cannot_attack_alone(instance->damage_target_player, instance->damage_target_card, event);
	}

	return generic_creature_with_bestow(player, card, event, MANACOST_XW(4, 1), 3, 2, 0, 0);
}

int card_skybind(int player, int card, event_t event){
	/* Skybind	|3|W|W
	 * Enchantment
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, exile target nonenchantment permanent. Return that card to the
	 * battlefield under its owner's control at the beginning of the next end step. */

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_ENCHANTMENT;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && select_target(player, card, &td, "Select target non-enchantment permanent.", &(instance->targets[0]))){
			remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

/* Skyspear Cavalry	|3|W|W => vanilla
 * Creature - Human Soldier 2/2
 * Flying
 * Double strike */

int card_stonewise_fortifier(int player, int card, event_t event){
	/* Stonewise Fortifier	|1|W
	 * Creature - Human Wizard 2/2
	 * |4|W: Prevent all damage that would be dealt to ~ by target creature this turn. */

	if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) && card_stonewise_fortifier(player, card, EVENT_CAN_ACTIVATE)){
		if (current_turn != 1-player){
			int available = has_mana(player, COLOR_ANY, 1);
			if (available / 5 < count_subtype(1-player, TYPE_CREATURE, -1) || available < 5){ return 0; }
		}
		check_abilities_toughness += 0x3fff;
	}
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy = prevent_all_damage_to_target(player, instance->parent_card, player, instance->parent_card, 1);
			get_card_instance(player, legacy)->targets[1] = instance->targets[0];
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(4, 1), 0, &td, "TARGET_CREATURE");
}

/* Supply-Line Cranes	|3|W|W => Timberland Guide
 * Creature - Bird 2/4
 * Flying
 * When ~ enters the battlefield, put a +1/+1 counter on target creature. */

int card_tethmos_high_priest(int player, int card, event_t event){
	/* Tethmos High Priest	|2|W
	 * Creature - Cat Cleric 2/3
	 * Heroic - Whenever you cast a spell that targets ~, return target creature card with converted mana cost 2 or less from your graveyard to the
	 * battlefield. */

	if( heroic(player, card, event) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card with CMC 2 or less.");
		this_test.cmc = 3;
		this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		if( new_special_count_grave(player, &this_test) && ! graveyard_has_shroud(2) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

/*******
* Blue *
*******/

int card_aerial_formation(int player, int card, event_t event){

	/* Aerial Formation	|U
	 * Instant
	 * Strive - ~ costs |2|U more to cast for each target beyond the first.
	 * Any number of target creatures each get +1/+1 and gain flying until end of turn. */

	if (event == EVENT_CHECK_PUMP && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id)){
		pumpable_power[player] += 1;
		pumpable_toughness[player] += 1;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", 2, 0, 1, 0, 0, 0);
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 1, 1, KEYWORD_FLYING, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_battlefield_thaumaturge(int player, int card, event_t event){

	/* Battlefield Thaumaturge	|1|U
	 * Creature - Human Wizard 2/1
	 * Each instant and sorcery spell you cast costs |1 less to cast for each creature it targets.
	 * Heroic - Whenever you cast a spell that targets ~, ~ gains hexproof until end of turn. */

	if( heroic(player, card, event) ){
		pump_ability_until_eot(player, card, player, card, 0, 0, 0, SP_KEYWORD_HEXPROOF);
	}

	// Does the other ability is codeble at all ?

	// Maybe for just strive cards, at least.  Or maybe refund mana after a spell's put on the stack?

	return 0;
}

/* Cloaked Siren	|3|U => Ashcoat Bear
 * Creature - Siren 3/2
 * Flash
 * Flying */

int card_countermand(int player, int card, event_t event){

	/* Countermand	|2|U|U
	 * Instant
	 * Counter target spell. Its controller puts the top four cards of his or her library into his or her graveyard. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			mill(instance->targets[0].player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_crystalline_nautilus(int player, int card, event_t event){

	/* Crystalline Nautilus	|2|U
	 * Enchantment Creature - Nautilus 4/4
	 * Bestow |3|U|U
	 * When ~ becomes the target of a spell or ability, sacrifice it.
	 * Enchanted creature gets +4/+4 and has "When this creature becomes the target of a spell or ability, sacrifice it." */

	if( becomes_target_of_spell_or_effect(player, card, event, player, card , ANYBODY) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	attached_creature_gains_sacrifice_when_becomes_target(player, card, event);

	return generic_creature_with_bestow(player, card, event, MANACOST_XU(3, 2), 4, 4, 0, 0);
}

int card_dakra_mystic(int player, int card, event_t event){

	/* Dakra Mystic	|U
	 * Creature - Merfolk Wizard 1/1
	 * |U, |T: Each player reveals the top card of his or her library. You may put the revealed cards into their owners' graveyards.
	 * 		If you don't, each player draws a card. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int *deck_human = deck_ptr[HUMAN];
		int *deck_ai = deck_ptr[AI];

		if (deck_human[0] != -1){
			reveal_card_iid(player, card, deck_human[0]);
		}

		if (deck_ai[0] != -1){
			load_text(0, "REVEALS");
			char opponent_reveals[150];
			scnprintf(opponent_reveals, 150, text_lines[1], opponent_name);

			look_at_iid(player, card, deck_ai[0], opponent_reveals);
		}

		int choice;
		if (deck_ai[0] == -1){
			choice = 0;
		} else if (deck_human[0] == -1){
			choice = 1;
		} else {
			choice = get_base_value(-1, deck_ai[0]) < get_base_value(-1, deck_human[0]) ? 0 : 1;
		}
		choice = do_dialog(player, player, card, -1, -1, " Both player mill\n Both player draw", choice);

		if( choice == 0 ){
			mill(player, 1);
			mill(1-player, 1);
		}
		else{
			draw_cards(player, 1);
			draw_cards(1-player, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_U(1), 0, NULL, NULL);
}

static const char* must_share_type(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  card_instance_t* targeter = get_card_instance(targeting_player, targeting_card);
  if (get_type(player, card) & get_type(targeter->targets[0].player, targeter->targets[0].card))
	return NULL;

  return gs_target_error_type;	// ",type"
}
int card_daring_thief(int player, int card, event_t event){

	/* Daring Thief	|2|U
	 * Creature - Human Rogue 2/3
	 * Inspired - Whenever ~ becomes untapped, you may exchange control of target nonland permanent you control and target permanent an
	 * 		opponent controls that shares a card type with it. */

	if( inspired(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.illegal_type = TYPE_LAND;
		td.allowed_controller = player;
		td.preferred_controller = player;

		if (can_target(&td) && pick_target(&td, "TARGET_NONLAND_PERMANENT") ){
			card_instance_t *instance = get_card_instance(player, card);

			instance->number_of_targets = 1;

			target_definition_t td1;
			default_target_definition(player, card, &td1, TYPE_PERMANENT);
			td1.allowed_controller = 1-player;
			td1.preferred_controller = 1-player;
			td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
			td1.extra = target_predicate_extra(must_share_type);

			if( can_target(&td1) && new_pick_target(&td1, "TARGET_PERMANENT", 1, 0) ){
				instance->number_of_targets = 2;
				exchange_control_of_target_permanents(player, card, instance->targets[0].player, instance->targets[0].card,
													  instance->targets[1].player, instance->targets[1].card);
			} else {
			  instance->number_of_targets = 0;
			}
		}
		cancel = 0;
	}

	return 0;
}

int card_dictate_of_kruphix(int player, int card, event_t event){

	/* Dictate of Kruphix	|1|U|U
	 * Enchantment
	 * Flash
	 * At the beginning of each player's draw step, that player draws an additional card. */

	if( event == EVENT_DRAW_PHASE && ! is_humiliated(player, card, AL_TRIG) ){
		event_result++;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) &&
		player == AI && (current_turn == player || current_phase <= PHASE_DRAW) )
	{
		ai_modifier -= 64;
	}

	return flash(player, card, event);
}

int card_font_of_fortune(int player, int card, event_t event){
	/* Font of Fortune	|1|U
	 * Enchantment
	 * |1|U, Sacrifice ~: Draw two cards. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XU(1, 1), 0, NULL, NULL);
}

int card_godhunter_octopus(int player, int card, event_t event)
{
  /* Godhunter Octopus	|5|U
   * Creature - Octopus 5/5
   * ~ can't attack unless defending player controls an enchantment or an enchanted permanent. */

  if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) )
	{
	  int c;
	  for (c = 0; c < active_cards_count[1-player]; ++c)
		if (in_play(1-player, c) && is_what(1-player, c, TYPE_ENCHANTMENT))
		  return 0;

	  card_instance_t* inst;
	  for (c = 0; c < active_cards_count[player]; ++c)
		if ((inst = in_play(player, c)) && is_what(player, c, TYPE_ENCHANTMENT)
			&& inst->damage_target_player == 1-player && inst->damage_target_card != -1
			&& has_subtype(player, c, SUBTYPE_AURA) && is_what(inst->damage_target_player, inst->damage_target_card, TYPE_PERMANENT))
		  return 0;

	  event_result = 1;
	}

  return 0;
}

int card_hour_of_need(int player, int card, event_t event){

	/* Hour of Need	|2|U
	 * Instant
	 * Strive - ~ costs |1|U more to cast for each target beyond the first.
	 * Exile any number of target creatures. For each creature exiled this way, its controller puts a 4/4 |Sblue Sphinx creature token with flying onto the
	 * battlefield. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", 1, 0, 1, 0, 0, 0);
		int i;
		if (cancel != 1 && IS_AI(player)){
			int mod = 0;
			for (i = 0; i < instance->number_of_targets; ++i){
				if (instance->targets[i].player == player){
					mod += 24 - 6 * get_power(instance->targets[i].player, instance->targets[i].card);
					mod += 12 - 3 * get_toughness(instance->targets[i].player, instance->targets[i].card);
					if (!check_for_ability(instance->targets[i].player, instance->targets[i].card, KEYWORD_FLYING)){
						mod += 12;
					}
				} else {
					mod -= 24 - 6 * get_power(instance->targets[i].player, instance->targets[i].card);
					mod -= 12 - 3 * get_toughness(instance->targets[i].player, instance->targets[i].card);
					if (!check_for_ability(instance->targets[i].player, instance->targets[i].card, KEYWORD_FLYING)){
						mod -= 12;
					}
				}
			}
			ai_modifier += player == AI ? mod : -mod;
		}
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_SPHINX, &token);
				token.t_player = instance->targets[i].player;
				token.key_plus = KEYWORD_FLYING;
				generate_token(&token);

			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_hubris(int player, int card, event_t event){
	/* Hubris	|1|U
	 * Instant
	 * Return target creature and all Auras attached to it to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL  ){
		if( valid_target(&td) ){
			manipulate_auras_enchanting_target(player, card, instance->targets[0].player, instance->targets[0].card, NULL, ACT_BOUNCE);
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_hypnotic_siren(int player, int card, event_t event){
	/* Hypnotic Siren	|U
	 * Enchantment Creature - Siren 1/1
	 * Bestow |5|U|U
	 * Flying
	 * You control enchanted creature.
	 * Enchanted creature gets +1/+1 and has flying. */

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		if( event == EVENT_CAN_MOVE_AURA || event == EVENT_MOVE_AURA || event == EVENT_RESOLVE_MOVING_AURA ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			return generic_stealing_aura(player, card, event, &td, "TARGET_CREATURE");
		}
		leaves_play(player, card, event, 0, 0);
		if (event == EVENT_LEAVES_PLAY_ABILITY){ end_of_stealing_aura_effect(player, card); }
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return generic_creature_with_bestow(player, card, event, MANACOST_XU(5,2), 1, 1, KEYWORD_FLYING, 0);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance= get_card_instance(player, card);
		if( instance->number_of_targets > 0 ){
			if( valid_target(&td) ){
				gain_control_and_attach_as_aura(player, card, event, instance->targets[0].player, instance->targets[0].card);
			}
			else{
				instance->number_of_targets = 0;
				instance->damage_target_player = instance->damage_target_card = -1;
				true_transform(player, card);
			}
		}
		else{
			generic_creature_with_bestow(player, card, event, MANACOST_XU(5,2), 1, 1, KEYWORD_FLYING, 0);
		}
		return 0;
	}

	return creature_with_targeted_bestow(player, card, event, &td, MANACOST_XU(5,2), 1, 1, KEYWORD_FLYING,0);
}

int card_interpret_the_signs(int player, int card, event_t event){

	/* Interpret the Signs	|5|U
	 * Sorcery
	 * Scry 3, then reveal the top card of your library. Draw cards equal to that card's converted mana cost.*/

	if( event == EVENT_RESOLVE_SPELL ){
		scry(player, 3);
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			reveal_card_iid(player, card, deck[0]);
			draw_cards(player, get_cmc_by_internal_id(deck[0]));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_kioras_dismissal(int player, int card, event_t event){

	/* Kiora's Dismissal	|U
	 * Instant
	 * Strive - ~ costs |U more to cast for each target beyond the first.
	 * Return any number of target enchantments to their owners' hands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_ENCHANTMENT", MANACOST_U(1));
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pin_to_the_earth(int player, int card, event_t event){
	/* Pin to the Earth	|1|U
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature gets -6/-0. */
	return generic_aura(player, card, event, 1-player, -6, -0, 0, 0, 0, 0, 0);
}

int card_polymorphous_rush(int player, int card, event_t event){

	/* Polymorphous Rush	|2|U
	 * Instant
	 * Strive - ~ costs |1|U more to cast for each target beyond the first.
	 * Choose a creature on the battlefield. Any number of target creatures you control each become a copy of that creature until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", MANACOST_XU(1, 1));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td_choose;
		base_target_definition(player, card, &td_choose, TYPE_CREATURE);
		td_choose.allow_cancel = 0;

		target_t chosen;
		if (can_target(&td_choose) && select_target(player, card-1000, &td_choose, "Choose a creature.", &chosen)){
			card_instance_t* instance = get_card_instance(player, card);
			int i;
			for (i = 0; i < instance->number_of_targets; i++){
				if (validate_target(player, card, &td, i)){
					shapeshift_target(player, card, instance->targets[i].player, instance->targets[i].card, chosen.player, chosen.card,
										SHAPESHIFT_UNTIL_EOT);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pull_from_the_deep(int player, int card, event_t event){
	/* Pull from the Deep	|2|U|U
	 * Sorcery
	 * Return up to one target instant card and up to one target sorcery card from your graveyard to your hand. Exile ~. */

	if( event == EVENT_CAN_CAST ){
		if (!IS_AI(player)){
			return 1;
		}
		return any_in_graveyard_by_type(player, TYPE_SORCERY | TYPE_INSTANT) && !graveyard_has_shroud(player);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		instance->targets[0].player = instance->targets[0].card = -1;
		instance->targets[1].player = instance->targets[1].card = -1;

		if (graveyard_has_shroud(player)){
			return 0;
		}
		if( any_in_graveyard_by_type(player, TYPE_INSTANT) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_INSTANT, "Select target instant card.");
			select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &this_test, 0);
		}
		if( any_in_graveyard_by_type(player, TYPE_SORCERY) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_SORCERY, "Select target sorcery card.");
			select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &this_test, 1);
		}
		cancel = 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->targets[0].card != -1 || instance->targets[1].card != -1){
			int any_validated = 0;
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++any_validated;
			}
			selected = validate_target_from_grave_source(player, card, player, 1);
			if( selected != -1 ){
				from_grave_to_hand(player, selected, TUTOR_HAND);
				++any_validated;
			}
			if (!any_validated){
				kill_card(player, card, KILL_DESTROY);
				return 0;
			}
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_riptide_chimera(int player, int card, event_t event)
{
  /* Riptide Chimera	|2|U
   * Enchantment Creature - Chimera 3/4
   * Flying
   * At the beginning of your upkeep, return an enchantment you control to its owner's hand. */

	if( event == EVENT_CAN_CAST )
		return basic_spell(player, card, event);

	upkeep_trigger_ability(player, card, event, player);

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
		target_definition_t td;
		base_target_definition(player, card, &td, TYPE_ENCHANTMENT);
		td.allowed_controller = td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "SELECT_AN_ENCHANTMENT"))
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_rise_of_eagles(int player, int card, event_t event){
	/* Rise of Eagles	|4|U|U
	 * Sorcery
	 * Put two 2/2 |Sblue Bird enchantment creature tokens with flying onto the battlefield. Scry 1. */

	if ( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BIRD, &token);
		token.qty = 2;
		token.pow = token.tou = 2;
		token.color_forced = COLOR_TEST_BLUE;
		token.special_flags2 = SF2_ENCHANTED_EVENING;
		generate_token(&token);

		scry(player, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_sage_of_hours(int player, int card, event_t event)
{
  /* Sage of Hours	|1|U
   * Creature - Human Wizard 1/1
   * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~.
   * Remove all +1/+1 counters from ~: For each five counters removed this way, take an extra turn after this one. */

  int result;
  card_instance_t *instance;
  result = heroic(player, card, event);
  if (result != 0)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  if ((event == EVENT_CAN_ACTIVATE) && (player == 1))
  {
    result = count_counters(1, card, COUNTER_P1_P1);
    if (result == 0)
    {
      return 0;
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    result = count_counters(player, card, COUNTER_P1_P1);
    if (result < 5)
    {
      ai_modifier = ai_modifier + -0x32;
    }
    else
    {
      ai_modifier = ai_modifier + 0x32;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = result;
    remove_all_counters(player, card, player, card, COUNTER_P1_P1);
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result =
        (instance->info_slot / 5 + (instance->info_slot >> 0x1f)) - (instance->info_slot >> 0x1f);
    if (4 < instance->info_slot)
    {
      do
      {
        result = result + -1;
        time_walk_effect(instance->parent_controller, instance->parent_card);
      } while (0 < result);
    }
  }
  result = generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

int card_scourge_of_fleets(int player, int card, event_t event){

	/* Scourge of Fleets	|5|U|U
	 * Creature - Kraken 6/6
	 * When ~ enters the battlefield, return each creature your opponents control with toughness X or less to its owner's hand,
	 * 		where X is the number of |H1Islands you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");
		test.toughness = basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_BLUE)] + 1;
		test.toughness_flag = F5_TOUGHNESS_LESSER_THAN_VALUE;
		new_manipulate_all(player, card, 1-player, &test, ACT_BOUNCE);
	}

	return 0;
}

int card_sigiled_starfish(int player, int card, event_t event)
{
  /* Sigiled Starfish	|1|U
   * Creature - Starfish 0/3
   * |T: Scry 1. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	scry(player, 1);

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(0), 0, NULL, NULL);
}

int card_thassas_devourer(int player, int card, event_t event)
{
	/* Thassa's Devourer	|4|U
	 * Enchantment Creature - Elemental 2/6
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, target player puts the top two cards of his or her library
	 * into his or her graveyard. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_PLAYER")){
			mill(instance->targets[0].player, 2);
		}
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_thassas_ire(int player, int card, event_t event)
{
	/* Thassa's Ire	|U
	 * Enchantment
	 * |3|U: You may tap or untap target creature. */

	if( !IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
		twiddle(player, card, 0);

	int rval = generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(3, 1), 0, &td, "TARGET_CREATURE");

	if (event == EVENT_ACTIVATE && player == AI && cancel != 1)
		ai_modifier_twiddle(player, card, 0);

	return rval;
}

int card_triton_cavalry(int player, int card, event_t event){
	/* Triton Cavalry	|3|U
	 * Creature - Merfolk Soldier 2/4
	 * Heroic - Whenever you cast a spell that targets ~, you may return target enchantment to its owner's hand. */

	if( heroic(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ENCHANTMENT") ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

/* Triton Shorestalker	|U => Phantom Warrior
 * Creature - Merfolk Rogue 1/1
 * ~ can't be blocked. */

/* War-Wing Siren	|2|U => Fabled Hero
 * Creature - Siren Soldier 1/3
 * Flying
 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

int card_whitewater_naiads(int player, int card, event_t event)
{
  /* Whitewater Naiads	|3|U|U
   * Enchantment Creature - Nymph 4/4
   * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, target creature can't be blocked this turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if ( event == EVENT_ETB_ABILITY )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0,0, 0,SP_KEYWORD_UNBLOCKABLE);
		instance->number_of_targets = 0;
	}

	return 0;
}

/********
* Black *
********/

int card_agent_of_erebos(int player, int card, event_t event)
{
	/* Agent of Erebos	|3|B
	 * Enchantment Creature - Zombie 2/2
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, exile all cards from target player's graveyard. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		instance->number_of_targets = 0;
		if (can_target(&td) && pick_target(&td, "TARGET_PLAYER")){
			rfg_whole_graveyard(instance->targets[0].player);
		}
	}

	return global_enchantment(player, card, event);
}

int card_aspect_of_gorgon(int player, int card, event_t event)
{
	/* Aspect of Gorgon	|2|B
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature gets +1/+3 and has deathtouch. */
	return generic_aura(player, card, event, player, 1, 3, 0, SP_KEYWORD_DEATHTOUCH, 0, 0, 0);
}

static void bloodcrazed_hoplite_effect(int player, int card, int n)
{
  if (n <= 0)
    return;
  int opponent = 1 - player;
  for (int remaining = n; remaining > 0; --remaining)
  {
    card_instance_t *instance = get_card_instance(player, card);
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.allow_cancel = 0;
    td.allowed_controller = td.preferred_controller = opponent;
    if (IS_AI(player))
    {
      td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
      td.extra = 0;
    }
    if (!can_target(&td))
      return;
    if (pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS"))
    {
      instance->number_of_targets = 0;
      remove_counters(player, card, instance->targets[0].player,
                      instance->targets[0].card, COUNTER_P1_P1, 1);
    }
  }
}
int card_bloodcrazed_hoplite(int player, int card, event_t event)
{
  /* Bloodcrazed Hoplite	|1|B	0x200dba8
   * Creature - Human Soldier 2/1
   * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on it.
   * Whenever a +1/+1 counter is placed on ~, remove a +1/+1 counter from target creature an opponent controls. */

  int player_or_trigger_result;
  xtrigger_t current_trigger;
  int counter_count_or_result;

  enable_xtrigger_flags = enable_xtrigger_flags | ENABLE_XTRIGGER_1_1_COUNTERS;
  player_or_trigger_result = heroic(player, card, event);
  if (player_or_trigger_result != 0)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  }
  current_trigger = trigger_condition;
  if (trigger_condition == TRIGGER_XTRIGGER)
  {
    current_trigger = xtrigger_impl_value_dont_use_directly;
  }
  if (((((current_trigger == XTRIGGER_1_1_COUNTERS) && (card == affected_card)) &&
        (player == affected_card_controller)) &&
       ((reason_for_trigger_controller == player && (trigger_cause == card)))) &&
      ((trigger_cause_controller == player &&
        (player_or_trigger_result = player,
         counter_count_or_result = is_humiliated(player, card, AL_TRIG),
         counter_count_or_result == 0))))
  {
    if (event == EVENT_TRIGGER)
    {
      player_or_trigger_result = duh_mode(player);
      event_result = event_result | (player_or_trigger_result != 0) + 1;
      return 0;
    }
    if (event == EVENT_RESOLVE_TRIGGER)
    {
      bloodcrazed_hoplite_effect(player, card, player_or_trigger_result);
      return 0;
    }
  }
  if ((event == EVENT_RESOLVE_SPELL) &&
      (player_or_trigger_result = player,
       counter_count_or_result = count_counters(player, card, COUNTER_P1_P1),
       0 < counter_count_or_result))
  {
    bloodcrazed_hoplite_effect(player, card, player_or_trigger_result);
  }
  return 0;
}

int card_mesmeric_fiend(int player, int card, event_t event);
int card_brain_maggot(int player, int card, event_t event){
	/* Brain Maggot	|1|B
	 * Enchantment Creature - Insect 1/1
	 * When ~ enters the battlefield, target opponent reveals his or her hand and you choose a nonland card from it. Exile that card until ~ leaves the
	 * battlefield. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}
	return card_mesmeric_fiend(player, card, event);
}

int card_cast_into_darkness(int player, int card, event_t event)
{
  /* Cast into Darkness	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -2/-0 and can't block. */
	return generic_aura(player, card, event, 1-player, -2, 0, 0, SP_KEYWORD_CANNOT_BLOCK, 0, 0, 0);
}

int card_cruel_feeding(int player, int card, event_t event){

	/* Cruel Feeding	|B
	 * Instant
	 * Strive - ~ costs |2|B more to cast for each target beyond the first.
	 * Any number of target creatures each get +1/+0 and gain lifelink until end of turn. */

	if (event == EVENT_CHECK_PUMP && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id)){
		pumpable_power[player] += 1;
	}

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", MANACOST_XB(2, 1));
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 1, 0, 0, SP_KEYWORD_LIFELINK);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_grave_pact(int player, int card, event_t event);
int card_dictate_of_erebos(int player, int card, event_t event){
	/* Dictate of Erebos	|3|B|B
	 * Enchantment
	 * Flash
	 * Whenever a creature you control dies, each opponent sacrifices a creature. */
	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		ai_modifier -= 16;
	}
	return card_grave_pact(player, card, event);
}

int card_doomwake_giant(int player, int card, event_t event){

	/* Doomwake Giant	|4|B
	 * Enchantment Creature - Giant 4/6
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, creatures your opponents control get -1/-1 until end of
	 * turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		pump_subtype_until_eot(player, card, 1-player, -1, -1, -1, 0, 0);
	}

	return 0;
}

int card_dreadbringer_lampads(int player, int card, event_t event)
{
  /* Dreadbringer Lampads	|4|B
   * Enchantment Creature - Nymph 4/2
   * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, target creature gains intimidate until
   * 		end of turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_INTIMIDATE);
		instance->number_of_targets = 0;
	}

  return 0;
}

static int test_is_not_enchantment(int iid, intptr_t unused, int player, int card)
{
  return !is_what(player, card, TYPE_ENCHANTMENT);
}

int card_extinguish_all_hope(int player, int card, event_t event)
{
  /* Extinguish All Hope	|4|B|B
   * Sorcery
   * Destroy all nonenchantment creatures. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.special_selection_function = test_is_not_enchantment;

	  new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static const char* is_enchanted_or_enchantment_creature(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
	if( is_enchanted(player, card) || is_what(player, card, TYPE_ENCHANTMENT) ){
		return NULL;
	}
	return "must be enchanted or an enchantment creature";
}
int card_feast_of_dreams(int player, int card, event_t event){
	/* Feast of Dreams	|1|B
	 * Instant
	 * Destroy target enchanted creature or enchantment creature. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_enchanted_or_enchantment_creature);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target enchanted creature or an enchantment creature.", 1, NULL);
}

int card_felhide_petrifier(int player, int card, event_t event){
	/* Felhide Petrifier	|2|B
	 * Creature - Minotaur Warrior 2/3
	 * Deathtouch
	 * Other Minotaur creatures you control have deathtouch. */

	deathtouch(player, card, event);

	boost_subtype(player, card, event, SUBTYPE_MINOTAUR, 0, 0, 0, SP_KEYWORD_DEATHTOUCH, BCT_CONTROLLER_ONLY);

	return 0;
}

int card_font_of_return(int player, int card, event_t event){
	/* Font of Return	|1|B
	 * Enchantment
	 * |3|B, Sacrifice ~: Return up to three target creature cards from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XB(3, 1), 0, NULL, NULL);
	}
	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_XB(3, 1)) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select up to three target creature cards.");
			select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, &test, 3, &instance->targets[0]);
			if( cancel != 1 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}
	if (event == EVENT_RESOLVE_ACTIVATION){
		int i = 0;
		for (i = 0; i < 3; i++){
			if (instance->targets[i].player != -1)
			{
				int selected = validate_target_from_grave(player, card, player, i);
				if (selected != -1)
				{
					from_grave_to_hand(player, selected, TUTOR_HAND);
				}
			}
		}
	}

	return 0;
}

int card_gnarled_scarhide(int player, int card, event_t event)
{
  /* Gnarled Scarhide	|B
   * Enchantment Creature - Minotaur 2/1
   * Bestow |3|B
   * ~ can't block.
   * Enchanted creature gets +2/+1 and can't block. */

  cannot_block(player, card, event);

  return generic_creature_with_bestow(player, card, event, MANACOST_XB(3,1), 2,1, 0,SP_KEYWORD_CANNOT_BLOCK);
}

int card_grim_guardian(int player, int card, event_t event)
{
	/* Grim Guardian	|2|B
	 * Enchantment Creature - Zombie 1/4
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, each opponent loses 1 life. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		lose_life(1-player, 1);
	}

	return 0;
}

int card_king_macar_the_gold_cursed(int player, int card, event_t event){
	/* King Macar, the Gold-Cursed	|2|B|B
	 * Legendary Creature - Human 2/3
	 * Inspired - Whenever ~ becomes untapped, you may exile target creature. If you do, put a colorless artifact token named Gold onto the battlefield. It has
	 * "Sacrifice this artifact: Add one mana of any color to your mana pool." */

	check_legend_rule(player, card, event);

	if( inspired(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			generate_token_by_id(player, card, CARD_ID_GOLD);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_master_of_the_feast(int player, int card, event_t event){
	/* Master of the Feast	|1|B|B
	 * Enchantment Creature - Demon 5/5
	 * Flying
	 * At the beginning of your upkeep, each opponent draws a card. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		draw_cards(1-player, 1);
	}

	upkeep_trigger_ability(player, card, event, player);

	return 0;
}

int card_nightmarish_end(int player, int card, event_t event)
{
  /* Nightmarish End	|2|B => Warped Physique
   * Instant
   * Target creature gets -X/-X until end of turn, where X is the number of cards in your hand. */
  return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -hand_count[player], -hand_count[player], 0, 0);
}

int card_nyx_infusion(int player, int card, event_t event)
{
  /* Nyx Infusion	|2|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 as long as it's an enchantment. Otherwise, it gets -2/-2. */

  if (event == EVENT_POWER || event == EVENT_TOUGHNESS || event == EVENT_SHOULD_AI_PLAY)
	{
	  card_instance_t* instance;
	  if (!(instance = in_play(player, card))
		  || instance->damage_target_player < 0
		  || (event != EVENT_SHOULD_AI_PLAY && !affect_me(instance->damage_target_player, instance->damage_target_card)))
		return 0;

	  int ench = is_what(instance->damage_target_player, instance->damage_target_card, TYPE_ENCHANTMENT);
	  if (event == EVENT_SHOULD_AI_PLAY)
		ai_modifier += (!ench == !(instance->damage_target_player == AI)) ? 24 : -24;
	  else
		event_result += ench ? 2 : -2;
	}

  if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

  return vanilla_aura(player, card, event, ANYBODY);
}

/* Pharika's Chosen	|B => Deadly Recluse
 * Creature - Snake 1/1
 * Deathtouch */

int card_returned_reveler(int player, int card, event_t event){
	/* Returned Reveler	|1|B
	 * Creature - Zombie Satyr 1/3
	 * When ~ dies, each player puts the top three cards of his or her library into his or her graveyard. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		mill(player, 3);
		mill(1-player, 3);
	}

	return 0;
}

int card_ritual_of_the_returned(int player, int card, event_t event){

	/* Ritual of the Returned	|3|B
	 * Instant
	 * Exile target creature card from your graveyard.  Put a |Sblack Zombie creature token onto the battlefield. Its power is equal to that card's power and
	 * its toughness is equal to that card's toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
	this_test.ai_selection_mode = AI_MAX_CMC;

	if( event == EVENT_RESOLVE_SPELL  ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			const int *grave = get_grave(player);
			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
			token.pow = get_base_power_iid(player, grave[selected]);
			token.tou = get_base_toughness_iid(player, grave[selected]);
			rfg_card_from_grave(player, selected);
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
}

/* Rotted Hulk	|3|B => vanilla
 * Creature - Elemental 2/5 */

int card_silence_the_believers(int player, int card, event_t event){

	/* Silence the Believers	|2|B|B
	 * Instant
	 * Strive - ~ costs |2|B more to cast for each target beyond the first.
	 * Exile any number of target creatures and all Auras attached to them. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", MANACOST_XB(2, 1));
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		// First : remove the non-legal targets
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( ! validate_target(player, card, &td, i) ){
				instance->targets[i].player = instance->targets[i].card = -1;
			}
		}
		for(i=0; i<instance->number_of_targets; i++){
			if( instance->targets[i].player != -1 && in_play(instance->targets[i].player, instance->targets[i].card) ){
				exile_permanent_and_auras_attached(player, card, instance->targets[i].player, instance->targets[i].card, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_spiteful_blow(int player, int card, event_t event){
	/* Spiteful Blow	|4|B|B
	 * Sorcery
	 * Destroy target creature and target land. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td1) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td1, "TARGET_LAND", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( validate_target(player, card, &td1, 1) ){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* Squelching Leeches	|2|B|B => Nightmare
 * Creature - Leech 100/100
 * ~'s power and toughness are each equal to the number of |H1Swamps you control. */

int card_thoughtrender_lamia(int player, int card, event_t event)
{
	/* Thoughtrender Lamia	|4|B|B
	 * Enchantment Creature - Lamia 5/3
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, each opponent discards a card. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		discard(1-player, 0, player);
	}

	return 0;
}

int card_tormented_thoughts(int player, int card, event_t event)
{
  /* Tormented Thoughts	|2|B
   * Sorcery
   * As an additional cost to cast ~, sacrifice a creature.
   * Target player discards a number of cards equal to the sacrificed creature's power. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.allow_cancel = ! is_token(player, card); // Could not cancel if this is copied from stack

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET)
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);

	if (event == EVENT_CAN_CAST)
		return can_target(&td) && (is_token(player, card) || can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE));

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		int sac = 0;
		if( ! is_token(player, card) ){ // AKA is not copid from stack
			sac = controller_sacrifices_a_permanent(player, card, TYPE_CREATURE, SAC_AS_COST|SAC_JUST_MARK|SAC_RETURN_CHOICE);
			if( ! sac ){
				spell_fizzled = 1;
				return 0;
			}
		}

		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_PLAYER") ){
			if( ! is_token(player, card) ){
				int tp = BYTE2(sac), tc = BYTE3(sac);
				instance->info_slot = get_power(tp, tc);
				kill_card(tp, tc, KILL_SACRIFICE);
			}
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		new_multidiscard(instance->targets[0].player, instance->info_slot, 0, player);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

/* Worst Fears	|7|B
 * Sorcery
 * You control target player during that player's net turn. Exile ~. */

/******
* Red *
******/

int card_akroan_line_breaker(int player, int card, event_t event)
{
  /* Akroan Line Breaker	|2|R
   * Creature - Human Warrior 2/1
   * Heroic - Whenever you cast a spell that targets ~, ~ gets +2/+0 and gains intimidate until end of turn. */

  if (heroic(player, card, event))
	pump_ability_until_eot(player, card, player, card, 2,0, 0, SP_KEYWORD_INTIMIDATE);

  return 0;
}

static int bearer_of_the_heavens_legacy(int player, int card, event_t event)
{
  if (eot_trigger(player, card, event))
  {
    test_definition_t test;
    new_default_test_definition(&test, TYPE_PERMANENT, "");
    new_manipulate_all(player, card, 2, &test, ACT_KILL_DESTROY);
    kill_card(player, card, KILL_DESTROY);
  }

  return 0;
}

int card_bearer_of_the_heavens(int player, int card, event_t event)
{
	/* Bearer of the Heavens	|7|R
	 * Creature - Giant 10/10
	 * When ~ dies, destroy all permanents at the beginning of the next end step.*/

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		create_legacy_effect(player, card, &bearer_of_the_heavens_legacy);
	}

	return 0;
}

/* Bladetusk Boar	|3|R => zendikar.c
 * Creature - Boar 3/2
 * Intimidate */

int card_blinding_flare(int player, int card, event_t event)
{
  /* Blinding Flare	|R
   * Sorcery
   * Strive - ~ costs |R more to cast for each target beyond the first.
   * Any number of target creatures can't block this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if (event == EVENT_CAN_CAST)
		return can_target(&td);

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
		strive(player, card, event, &td, "TARGET_CREATURE", MANACOST_R(1));

	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* instance = get_card_instance(player, card);

		int i;
		for (i = 0; i < instance->number_of_targets; ++i)
			if (validate_target(player, card, &td, i))
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card,
										0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cyclops_of_eternal_fury(int player, int card, event_t event)
{
  /* Cyclops of Eternal Fury	|4|R|R
   * Enchantment Creature - Cyclops 5/3
   * Creatures you control have haste. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	boost_subtype(player, card, event, -1, 0, 0, 0,SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

	return 0;
}

int card_dictate_of_the_twin_gods(int player, int card, event_t event){

	/* Dictate of the Twin Gods	|3|R|R
	 * Enchantment
	 * Flash
	 * If a source would deal damage to a permanent or player, it deals double that damage to that permanent or player instead. */

	card_instance_t* damage = damage_being_dealt(event);
	if (damage){
		damage->info_slot *= 2;
	}

	return flash(player, card, event);
}

int card_eidolon_of_the_great_revel(int player, int card, event_t event){
	/* Eidolon of the Great Revel	|R|R
	 * Enchantment Creature - Spirit 2/2
	 * Whenever a player casts a spell with converted mana cost 3 or less, ~ deals 2 damage to that player. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.cmc = 4;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		damage_player(trigger_cause_controller, 2, player, card);
	}
	return basic_spell(player, card, event);
}

int card_flamespeakers_will(int player, int card, event_t event)
{
  /* Flamespeaker's Will	|R
   * Enchantment - Aura
   * Enchant creature you control
   * Enchanted creature gets +1/+1.
   * Whenever enchanted creature deals combat damage to a player, you may sacrifice ~. If you do, destroy target artifact. */

  return common_mortal_obstinacy_flamespeakers_will(player, card, event, TYPE_ARTIFACT, "TARGET_ARTIFACT");
}

int card_flurry_of_horns(int player, int card, event_t event)
{
  /* Flurry of Horns	|4|R
   * Sorcery
   * Put two 2/3 |Sred Minotaur creature tokens with haste onto the battlefield. */

	if (event == EVENT_RESOLVE_SPELL)
	{
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_MINOTAUR, &token);
		token.qty = 2;
		token.s_key_plus = SP_KEYWORD_HASTE;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_font_of_ire(int player, int card, event_t event){
	/* Font of Ire	|1|R	0x200bcb3
	 * Enchantment
	 * |3|R, Sacrifice ~: It deals 5 damage to target player or planeswalker. */
	if( !IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 5);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XR(3, 1), 0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
}

int card_forgeborn_oreads(int player, int card, event_t event)
{
	/* Forgeborn Oreads	|2|R|R	0x200bc45
	 * Enchantment Creature - Nymph 4/2
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, ~ deals 1 damage to any target. */
	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
			damage_target0(player, card, 1);
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_gluttonous_cyclops(int player, int card, event_t event)
{
  /* Gluttonous Cyclops	|5|R
   * Creature - Cyclops 5/4
   * |5|R|R: Monstrosity 3. */

  return monstrosity(player, card, event, MANACOST_XR(5, 2), 3);
}

int card_harness_by_force(int player, int card, event_t event){

	/* Harness by Force	|1|R|R
	 * Sorcery
	 * Strive - ~ costs |2|R more to cast for each target beyond the first.
	 * Gain control of any number of target creatures until end of turn. Untap those creatures. They gain haste until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if( IS_AI(player) ){
		td.preferred_controller = 1-player;
		td.allowed_controller = 1-player;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", MANACOST_XR(2, 1));
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				effect_act_of_treason(player, card, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_knowledge_and_power(int player, int card, event_t event)
{
	/* Knowledge and Power	|4|R	0x200bd3a
	 * Enchantment
	 * Whenever you scry, you may pay |2. If you do, ~ deals 2 damage to any target. */
  if (xtrigger_condition() == XTRIGGER_SCRY && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == player && !is_humiliated(player, card, AL_TRIG) && has_mana(player, COLOR_ANY, 2))
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	  if (!can_target(&td))
		return 0;

	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_AI(player);

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  instance->number_of_targets = 0;

		  if (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_COLORLESS, 2)
			  && can_target(&td)	// Might no longer be true, e.g. if both players have shroud and the only creature was sacrificed for mana
			  && pick_target(&td, "TARGET_ANY"))
			damage_target0(player, card, 2);
		  else
			cancel = 1;
		}
	}

  return global_enchantment(player, card, event);
}

int card_lightning_diadem(int player, int card, event_t event)
{
	/* Lightning Diadem	|5|R	0x200bd3f
	 * Enchantment - Aura
	 * Enchant creature
	 * When ~ enters the battlefield, it deals 2 damage to any target.
	 * Enchanted creature gets +2/+2. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);
		if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
			damage_target0(player, card, 2);
		instance->number_of_targets = 0;
	}

	return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
}

/* Magma Spray	|R => shards_of_alara.c
 * Instant
 * ~ deals 2 damage to target creature. If that creature would die this turn, exile it instead. */

int card_mogiss_warhound(int player, int card, event_t event){

	/* Mogis's Warhound	|1|R
	 * Enchantment Creature - Hound 2/2
	 * Bestow |2|R
	 * ~ attacks each turn if able.
	 * Enchanted creature gets +2/+2 and attacks each turn if able. */

	attack_if_able(player, card, event);

	return generic_creature_with_bestow(player, card, event, MANACOST_XR(2, 1), 2, 2, 0, SP_KEYWORD_MUST_ATTACK);
}

/* Pensive Minotaur	|2|R => vanilla
 * Creature - Minotaur Warrior */

int card_prophetic_flamespeaker(int player, int card, event_t event){

	/* Prophetic Flamespeaker	|1|R|R
	 * Creature - Human Shaman 1/3
	 * Double strike, trample
	 * Whenever ~ deals combat damage to a player, exile the top card of your library. You may play it this turn. */

	return nightveil_specter_like_ability(player, card, event, NSLA_EXILE_FROM_PLAYERS_DECK | NSLA_MUST_PAY_MANACOST_OF_EXILED_CARDS |
			NSLA_EXILE_ONLY_WITH_COMBAT_DAMAGE | NSLA_EXILE_ONLY_WHEN_DAMAGING_PLAYER | NSLA_REMOVE_STORED_CARDS_AT_EOT, 1,
			get_id(player, card));
}

/* Riddle of Lightning	|3|R|R => future_sight.c
 * Instant
 * Choose target creature or player. Scry 3, then reveal the top card of your library. ~ deals damage equal to that card's converted mana cost to that creature
 * or player. */

int card_rollick_of_abandon(int player, int card, event_t event)
{
  /* Rollick of Abandon	|3|R|R
   * Sorcery
   * All creatures get +2/-2 until end of turn. */

	if( event == EVENT_RESOLVE_SPELL )
	{
		pump_creatures_until_eot(player, card, ANYBODY, 0, 2,-2, 0,0, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_rouse_the_mob(int player, int card, event_t event)
{
  /* Rouse the Mob	|R
   * Instant
   * Strive - ~ costs |2|R more to cast for each target beyond the first.
   * Any number of target creatures each get +2/+0 and gain trample until end of turn. */

	if (event == EVENT_CHECK_PUMP && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id))
		pumpable_power[player] += 2;

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
		strive(player, card, event, &td, "TARGET_CREATURE", MANACOST_XR(2,1));

	if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 2,0, KEYWORD_TRAMPLE,0);

	  kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* Satyr Hoplite	|R => Fabled Hero
 * Creature - Satyr Soldier 1/1
 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

int card_sigiled_skink(int player, int card, event_t event)
{
  /* Sigiled Skink	|1|R
   * Creature - Lizard 2/1
   * Whenever ~ attacks, scry 1. */

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		scry(player, 1);
	}
	return 0;
}

int card_spawn_of_thraxes(int player, int card, event_t event)
{
	/* Spawn of Thraxes	|5|R|R	0x200bbc3
	 * Creature - Dragon 5/5
	 * Flying
	 * When ~ enters the battlefield, it deals damage to any target equal to the number of |H1Mountains you control. */
	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allowed_controller = 1-player;
		int amt = basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_RED)];
		if( ! can_target(&td) ){
			ai_modifier -= (amt * 2);
		}
	}

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		int amt = basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_RED)];
		return cip_damage_creature(player, card, event, &td, "TARGET_ANY", amt);
	}

	return 0;
}

int card_spite_of_mogis(int player, int card, event_t event){
	/* Spite of Mogis	|R
	 * Sorcery
	 * ~ deals damage to target creature equal to the number of instant and sorcery cards in your graveyard. Scry 1. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, count_graveyard_by_type(player, TYPE_SPELL), player, card);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_starfall(int player, int card, event_t event)
{
  /* Starfall	|4|R
   * Instant
   * ~ deals 3 damage to target creature. If that creature is an enchantment, ~ deals 3 damage to that creature's controller. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  int is_enchantment = is_what(instance->targets[0].player, instance->targets[0].card, TYPE_ENCHANTMENT);

		  damage_target0(player, card, 3);
		  if (is_enchantment)
			damage_player(instance->targets[0].player, 3, player, card);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_twinflame(int player, int card, event_t event){

	/* Twinflame	|1|R
	 * Sorcery
	 * Strive - ~ costs |2|R more to cast for each target beyond the first.
	 * Choose any number of target creatures you control. For each of them, put a token that's a copy of that creature onto the battlefield. Those tokens have
	 * haste. Exile them at the beginning of the next end step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", MANACOST_XR(2, 1));
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				token_generation_t token;
				copy_token_definition(player, card, &token, instance->targets[i].player, instance->targets[i].card);
				token.special_code_on_generation = &haste_and_remove_token_at_end_of_next_turn;
				generate_token(&token);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_wildfire_cerberus(int player, int card, event_t event)
{
  /* Wildfire Cerberus	|4|R
   * Creature - Hound 4/3
   * |5|R|R: Monstrosity 1.
   * When ~ becomes monstrous, it deals 2 damage to each opponent and each creature your opponents control. */

  if (event == EVENT_BECAME_MONSTROUS)
	new_damage_all(player, card, 1-player, 2, NDA_PLAYER_TOO, NULL);

  return monstrosity(player, card, event, MANACOST_XR(5, 2), 1);
}

/********
* Green *
********/

/* Bassara Tower Archer	|G|G => Aven Fleetwing
 * Creature - Human Archer 2/1
 * Hexproof, reach */

int card_colossal_heroics(int player, int card, event_t event)
{
  /* Colossal Heroics	|2|G
   * Instant
   * Strive - ~ costs |1|G more to cast for each target beyond the first.
   * Any number of target creatures each get +2/+2 until end of turn. Untap those creatures. */

  if (event == EVENT_CHECK_PUMP && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id))
	{
	  pumpable_power[player] += 2;
	  pumpable_toughness[player] += 2;
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;

  if (event == EVENT_CAN_CAST)
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	strive(player, card, event, &td, "TARGET_CREATURE", MANACOST_XG(1,1));

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  // Pump all first, then untap, so all untap triggers at least see all the cards in pumped state, even if they don't all see all as untapped.
	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  pump_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 2,2);
		else
		  instance->targets[i].player = -1;

	  for (i = 0; i < instance->number_of_targets; ++i)
		if (instance->targets[i].player != -1 && in_play(instance->targets[i].player, instance->targets[i].card))
		  untap_card(instance->targets[i].player, instance->targets[i].card);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_consign_to_dust(int player, int card, event_t event)
{
  /* Consign to Dust	|2|G
   * Instant
   * Strive - ~ costs |2|G more to cast for each target beyond the first.
   * Destroy any number of target artifacts and/or enchantments. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

  if (event == EVENT_CAN_CAST)
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	strive(player, card, event, &td, "TARGET_ARTIFACT_ENCHANTMENT", MANACOST_XG(2,1));

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_desecration_plague(int player, int card, event_t event)
{
  /* Desecration Plague	|3|G
   * Sorcery
   * Destroy target enchantment or land. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ENCHANTMENT | TYPE_LAND);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  if (valid_target(&td))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT_LAND", 1, NULL);
}

int card_dictate_of_karametra(int player, int card, event_t event){

	/* Dictate of Karametra	|3|G|G
	 * Enchantment
	 * Flash
	 * Whenever a player taps a land for mana, that player adds one mana to his or her mana pool of any type that land produced. */

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		ai_modifier -= 16;	// per flash() - discourages AI from playing w/o further advantage, and encourages AI to counter if human plays.  Note though that card_mana_flare always increases by 48.
		if (player == AI && !(current_turn == 1-player && current_phase >= PHASE_DISCARD)){
			ai_modifier -= 64;	// discourage AI from playing except during opponent's discard phase
		}
	}

	return card_mana_flare(player, card, event);
}

int card_eidolon_of_blossoms(int player, int card, event_t event)
{
  /* Eidolon of Blossoms	|2|G|G
   * Enchantment Creature - Spirit 2/2
   * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, draw a card. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		draw_a_card(player);
	}

	return 0;
}

int card_font_of_fertility(int player, int card, event_t event){
	/* Font of Fertility	|G
	 * Enchantment
	 * |1|G, Sacrifice ~: Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_land(player, 1, 1);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(1, 1), 0, NULL, NULL);
}

/* Golden Hind	|1|G => Elvish Mystic
 * Creature - Elk 2/1
 * |T: Add |G to your mana pool */

int card_goldenhide_ox(int player, int card, event_t event)
{
  /* Goldenhide Ox	|5|G
   * Enchantment Creature - Ox 5/4
   * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, target creature must be blocked this turn if able. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0,0, 0,SP_KEYWORD_MUST_BE_BLOCKED);
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_heroes_bane(int player, int card, event_t event)
{
  /* Heroes' Bane	|3|G|G
   * Creature - Hydra 0/0
   * ~ enters the battlefield with four +1/+1 counters.
   * |2|G|G: Put X +1/+1 counters on ~, where X is its power. */

  int pc;
  int result;
  card_instance_t *instance;
  int num;
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 4);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = instance->parent_controller;
    pc = instance->parent_card;
    instance = in_play(result, pc);
    if (instance != NULL)
    {
      num = get_power(result, pc);
      add_counters(player, card, result, pc, COUNTER_P1_P1, num);
    }
  }
  else if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    result = card_yew_spirit(player, card, event);
    return result;
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 2, 0, 0, 0, NULL, NULL);
}

int card_humbler_of_mortals(int player, int card, event_t event)
{
  /* Humbler of Mortals	|4|G|G
   * Enchantment Creature - Elemental 5/5
   * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, creatures you control gain trample until end of turn. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY )
		pump_creatures_until_eot(player, card, player, 0, 0,0, KEYWORD_TRAMPLE,0, NULL);

	return 0;
}

int card_hydra_broodmaster(int player, int card, event_t event)
{
	/* Hydra Broodmaster	|4|G|G
	 * Creature - Hydra 7/7
	 * |X|X|G: Monstrosity X.
	 * When ~ becomes monstrous, put X X/X green Hydra creature tokens onto the battlefield. */

	if (event == EVENT_RESOLVE_ACTIVATION){
		int amount = get_card_instance(player, card)->info_slot;
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HYDRA, &token);
		token.qty = amount;
		token.pow = amount;
		token.tou = amount;
		generate_token(&token);
	}

	return monstrosity(player, card, event, MANACOST_XG(-2, 1), get_card_instance(player, card)->info_slot);
}

int card_kruphixs_insight(int player, int card, event_t event)
{
	/* Kruphix's Insight	|2|G
	 * Sorcery
	 * Reveal the top six cards of your library. Put up to three enchantment cards from among them into your hand and the rest of the revealed cards into your
	 * graveyard. */

	if(event == EVENT_RESOLVE_SPELL){
		int amount = count_deck(player) >= 6 ? 6 : count_deck(player);
		show_deck(1-player, deck_ptr[player], amount, "Cards revealed by Kruphix's Insight", 0, gs_done_008b40e0 );

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an enchantment card.");
		this_test.create_minideck = amount;
		this_test.no_shuffle = 1;

		int chosen = 0;
		while( amount > 0 && chosen < 3 && new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test) >= 0 ){
			amount--;
			chosen++;
			this_test.create_minideck = amount;
		}
		mill(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_market_festival(int player, int card, event_t event)
{
  /* Dawn's Reflection	|3|G
   * Enchantment - Aura
   * Enchant land
   * Whenever enchanted land is tapped for mana, its controller adds two mana in any combination of colors to his or her mana pool. */

  /* Market Festival	|3|G
   * Enchantment - Aura
   * Enchant land
   * Whenever enchanted land is tapped for mana, its controller adds two mana in any combination of colors to his or her mana pool. */

  return wild_growth_aura_any_combination_of_colors(player, card, event, 0, COLOR_TEST_ANY_COLORED, 2);
}

int card_natures_panoply(int player, int card, event_t event)
{
  /* Nature's Panoply	|G
   * Instant
   * Strive - ~ costs |2|G more to cast for each target beyond the first.
   * Choose any number of target creatures. Put a +1/+1 counter on each of them. */

  target_t *target_player_entry;
  target_t *target_card_entry;
  card_instance_t *instance;
  int i;

  target_definition_t td;
  if (event == EVENT_CHECK_PUMP)
  {
    instance = get_card_instance(player, card);
    if (has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id) != 0)
    {
      pumpable_power[player] = pumpable_power[player] + 1;
      pumpable_toughness[player] = pumpable_toughness[player] + 1;
    }
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, "TARGET_CREATURE", 1, NULL);
    }
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        if ((card == affected_card) && (player == affected_card_controller))
        {
          strive(player, card, event, &td, "TARGET_CREATURE", 2, 0, 0, 1, 0, 0);
        }
      }
    }
    else if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      if (event == EVENT_RESOLVE_SPELL)
      {
        instance = get_card_instance(player, card);
        if (instance->number_of_targets != '\0')
        {
          i = 0;
          do
          {
            if (validate_target(player, card, &td, i) != 0)
            {
              target_card_entry = instance->targets + i;
              target_player_entry = instance->targets + i;
              i = i + 1;
              add_counters(player, card, target_player_entry->player, target_card_entry->card,
                           COUNTER_P1_P1, 1);
            }
            else
              i = i + 1;
          } while (i < (int)(unsigned int)instance->number_of_targets);
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
    }
    else if (event == EVENT_CHANGE_TARGET)
    {
      default_target_definition(player, card, &td, 2);
    }
  }
  return 0;
}

int card_nessian_game_warden(int player, int card, event_t event)
{
  /* Nessian Game Warden	|3|G|G
   * Creature - Beast 4/5
   * When ~ enters the battlefield, look at the top X cards of your library, where X is the number of |H1Forests you control. You may reveal a creature card
   * from among them and put it into your hand. Put the rest on the bottom of your library in any order. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		reveal_top_cards_of_library_and_choose_type(player, card, player,
													basiclandtypes_controlled[player][get_hacked_color(player, card, COLOR_GREEN)],
													0, TUTOR_HAND, 1, TUTOR_BOTTOM_OF_DECK, 0, TYPE_CREATURE);

  return 0;
}

int card_oakheart_dryads(int player, int card, event_t event)
{
  /* Oakheart Dryads	|2|G
   * Enchantment Creature - Nymph Dryad 2/3
   * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, target creature gets +1/+1 until end of turn. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
			pump_until_eot_merge_previous(player, card, instance->targets[0].player, instance->targets[0].card, 1,1);
		instance->number_of_targets = 0;
	}

	return 0;
}

/* Pheres-Band Thunderhoof	|4|G => Staunch-Hearted Warrior
 * Creature - Centaur Warrior 3/4
 * Heroic - Whenever you cast a spell that targets ~, put two +1/+1 counters on ~. */

int card_pheres_band_warchief(int player, int card, event_t event)
{
	/* Pheres-Band Warchief	|3|G
	 * Creature - Centaur Warrior 3/3
	 * Vigilance, trample
	 * Other Centaur creatures you control get +1/+1 and have vigilance and trample. */

	vigilance(player, card, event);

	boost_subtype(player, card, event, SUBTYPE_CENTAUR, 1, 1, KEYWORD_TRAMPLE, SP_KEYWORD_VIGILANCE, BCT_CONTROLLER_ONLY);

	return 0;
}

int card_ravenous_leucrocota(int player, int card, event_t event)
{
  /* Ravenous Leucrocota	|3|G
   * Creature - Beast 2/4
   * Vigilance
   * |6|G: Monstrosity 3. */

  vigilance(player, card, event);

  return monstrosity(player, card, event, MANACOST_XG(6,1), 3);
}

int card_renowned_weaver(int player, int card, event_t event){
	/* Renowned Weaver	|G
	 * Creature - Human Shaman 1/1
	 * |1|G, Sacrifice ~: Put a 1/3 Green Spider enchantment creature token with reach onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SPIDER, &token);
		token.pow = 1;
		token.tou = 3;
		token.key_plus = KEYWORD_REACH;
		token.special_flags2 = SF2_ENCHANTED_EVENING;
		generate_token(&token);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_XG(1, 1), 0, NULL, NULL);
}

int card_reviving_melody(int player, int card, event_t event)
{
  /* Reviving Melody	|2|G
   * Sorcery
   * Choose one or both - Return target creature card from your graveyard to your hand; and/or return target enchantment card from your graveyard to your
   * hand. */

  return spell_return_one_or_two_cards_from_gy_to_hand(player, card, event, TYPE_CREATURE, TYPE_ENCHANTMENT);
}

/* Satyr Grovedancer	|1|G => Timberland Guide
 * Creature - Satyr Shaman 1/1
 * When ~ enters the battlefield, put a +1/+1 counter on target creature. */

static int setessan_tactics_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->damage_target_player > -1 ){
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		modify_pt_and_abilities(p, c, event, 1, 1, 0);

		target_definition_t td;
		default_target_definition(p, c, &td, TYPE_CREATURE);
		td.special = TARGET_SPECIAL_NOT_ME;

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return attachment_granting_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
		}

		if( event == EVENT_RESOLVE_ACTIVATION && validate_arbitrary_target(&td, instance->targets[0].player, instance->targets[0].card) && in_play(p, c) ){
			fight(p, c, instance->targets[0].player, instance->targets[0].card);
		}
	}

	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}
int card_setessan_tactics(int player, int card, event_t event){

	/* Setessan Tactics	|1|G
	 * Instant
	 * Strive - ~ costs |G more to cast for each target beyond the first.
	 * Until end of turn, any number of target creatures each get +1/+1 and gain "|T: This creature fights another target creature." */

	if (event == EVENT_CHECK_PUMP && has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id)){
		pumpable_power[player] += 1;
		pumpable_toughness[player] += 1;
	}

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", MANACOST_G(1));
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				create_targetted_legacy_activate(player, card, &setessan_tactics_legacy, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_solidarity_of_heroes(int player, int card, event_t event)
{
  /* Solidarity of Heroes	|1|G
   * Instant
   * Strive - ~ costs |1|G more to cast for each target beyond the first.
   * Choose any number of target creatures. Double the number of +1/+1 counters on each of them. */

  target_t *target_player_entry;
  target_t *target_card_entry;
  card_instance_t *instance;
  int counter_count_or_valid;
  int i;
  target_definition_t *creature_target_definition;
  target_definition_t td;
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_CHANGE_TARGET && ai_is_speculating == 1)
    return 0;
  if ((ai_is_speculating == 1 && (event == EVENT_CAN_CAST || event == EVENT_CAST_SPELL)) ||
      ((ai_is_speculating == 1 || (player == 1 && ((unsigned char)trace_mode & 2) == 0)) &&
       ((uint32_t)(event - EVENT_CAST_SPELL) & 0xfffffff7) == 0))
  {
    td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
    td.extra = 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    creature_target_definition = &td;
    td.preferred_controller = player;
    return generic_spell(player, card, EVENT_CAN_CAST, 1, creature_target_definition,
                         "TARGET_CREATURE", 1, NULL);
  }
  creature_target_definition = &td;
  td.preferred_controller = player;
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
    strive(player, card, event, creature_target_definition, "TARGET_CREATURE", 1, 0, 0, 1, 0, 0);
    return 0;
  }
  creature_target_definition = &td;
  td.preferred_controller = player;
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (instance->number_of_targets != '\0')
  {
    i = 0;
    do
    {
      counter_count_or_valid = validate_target(player, card, creature_target_definition, i);
      if (counter_count_or_valid != 0)
      {
        counter_count_or_valid =
            count_counters(instance->targets[i].player, instance->targets[i].card, COUNTER_P1_P1);
        target_card_entry = instance->targets + i;
        target_player_entry = instance->targets + i;
        i = i + 1;
        add_counters(player, card, target_player_entry->player, target_card_entry->card,
                     COUNTER_P1_P1, counter_count_or_valid);
      }
      else
        i = i + 1;
    } while (i < (int)(unsigned int)instance->number_of_targets);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

int card_spirespine(int player, int card, event_t event){

	/* Spirespine	|2|G
	 * Enchantment Creature - Beast 4/1
	 * Bestow |4|G
	 * ~ blocks each turn if able.
	 * Enchanted creature gets +4/+1 and blocks each turn if able. */

	block_if_able(player, card, event);

	return generic_creature_with_bestow(player, card, event, MANACOST_XG(4, 1), 4, 1, 0, SP_KEYWORD_MUST_BLOCK);
}

int card_strength_from_the_fallen(int player, int card, event_t event){

	/* Strength from the Fallen	|1|G
	 * Enchantment
	 * Constellation - whenever ~ or another enchantment enters the battlefield under your control, target creature gets +X/+X until end of turn, where X is the
	 * number of creature cards in your graveyard. */

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			int amount = count_graveyard_by_type(player, TYPE_CREATURE);
			if (amount > 0){
				pump_until_eot_merge_previous(player, card, instance->targets[0].player, instance->targets[0].card, amount, amount);
			}
		}
		instance->number_of_targets = 0;
	}

	return global_enchantment(player, card, event);
}

int card_swarmborn_giant(int player, int card, event_t event)
{
  /* Swarmborn Giant	|2|G|G
   * Creature - Giant 6/6
   * When you're dealt combat damage, sacrifice ~.
   * |4|G|G: Monstrosity 2.
   * As long as ~ is monstrous, it has reach. */

  // info_slot set to CARD_ID_SWARMBORN_GIANT instead of something prosaic like 1 to lessen the chance of mistriggering if something becomes a copy of this

  card_instance_t* damage = combat_damage_being_dealt(event);
  if (damage && damage->damage_target_card == -1 && damage->damage_target_player == player && !damage_is_to_planeswalker(damage))
	get_card_instance(player, card)->info_slot = CARD_ID_SWARMBORN_GIANT;

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->info_slot != CARD_ID_SWARMBORN_GIANT)
		return 0;

	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER)
		kill_card(player, card, KILL_SACRIFICE);

	  // EVENT_END_TRIGGER won't be seen unless the trigger is countered.  (Once countering triggers works, anyway.  And hopefully it *will* be seen.)
	  if (event == EVENT_END_TRIGGER)
		instance->info_slot = 0;
	}

  if (event == EVENT_ABILITIES && affect_me(player, card) && is_monstrous(player, card))
	event_result |= KEYWORD_REACH;

  return monstrosity(player, card, event, MANACOST_XG(4,2), 2);
}

/*************
* Multicolor *
*************/

static int is_creature_planeswalker_aura(int iid, intptr_t unused, int player, int card)
{
  if (is_what(-1, iid, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER))
    return 1;
  if (!is_what(-1, iid, TYPE_ENCHANTMENT))
    return 0;
  return has_subtype_by_id(player, (unsigned int)cards_data[iid].id, SUBTYPE_AURA) != 0;
}

int card_ajani_mentor_of_heroes(int player, int card, event_t event)
{
	/* Ajani, Mentor of Heroes	|3|G|W
	 * Planeswalker - Ajani (4)
	 * +1: Distribute three +1/+1 counters among one, two, or three target creatures you control.
	 * +1: Look at the top four cards of your library. You may reveal an Aura, creature or planeswalker card from among them and put it into your hand. Put the
	 * rest on the bottom of your library in any order.
	 * -8: You gain 100 life. */
	check_legend_rule(player, card, event);

  if (IS_ACTIVATING(event))
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = player;
	  td.preferred_controller = player;

	  enum
	  {
		CHOICE_COUNTERS = 1,
		CHOICE_REVEAL = 2,
		CHOICE_LIFE = 3
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Distribute 3 counters", can_target(&td),	10,	1,
						"Reveal 4", 1,					5+(10 * ! can_target(&td)),	1,
						"Gain 100 life", 1,							20,	-8);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_ACTIVATE)
		{
			instance->number_of_targets = 0;
			if (choice == CHOICE_COUNTERS){
				if( target_available(player, card, &td) == 1 ){
					pick_target(&td, "ASHNODS_BATTLEGEAR");	// "Select target creature you control."
				}
				else{
					int max_targ = 3;
					int trgs = 0;
					while( trgs < max_targ && new_pick_target(&td, "ASHNODS_BATTLEGEAR", trgs, 0) ){
						trgs++;
					}
					if( trgs < 3 ){
						spell_fizzled = 1;
					}
				}
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		{
		  switch (choice)
			{
			  case CHOICE_COUNTERS:
				{
					if( instance->number_of_targets == 1 ){
						if( valid_target(&td) ){
							add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 3);
						}
					}
					else{
						int i;
						for(i=0; i<3; i++){
							if( validate_target(player, card, &td, i) ){
								add_counters(player, card, instance->targets[i].player, instance->targets[i].card, COUNTER_P1_P1, 1);
							}
						}
					}
				}
				break;

			  case CHOICE_REVEAL:
				{
					test_definition_t this_test;
					new_default_test_definition(&this_test, 0, "Select an Aura, creature, or planeswalker card.");
					this_test.special_selection_function = is_creature_planeswalker_aura;
					reveal_top_cards_of_library_and_choose(player, card, player, 4, 0, TUTOR_HAND, 1, TUTOR_BOTTOM_OF_DECK, 0, &this_test);
				}
				break;

			  case CHOICE_LIFE:
					gain_life(player, 100);
					break;
			}
		}
	}

  return planeswalker(player, card, event, 4);
}

int card_athreos_god_of_passage(int player, int card, event_t event)
{
	/* Athreos, God of Passage	|1|W|B
	 * Legendary Enchantment Creature - God 5/4
	 * Indestructible
	 * As long as your devotion to |Swhite and |Sblack is less than seven, ~ isn't a creature.
	 * Whenever another creature you own dies, return it to your hand unless target opponent pays 3 life. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_WHITE, COLOR_BLACK, 7);

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(player, card) && !is_humiliated(player, card, AL_TRIG) &&
		!check_for_death_trigger_removal(affected_card_controller, affected_card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( get_owner(affected_card_controller, affected_card) == player &&
			! is_token(affected_card_controller, affected_card) &&
			! affect_me(player, card) &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
			in_play(affected_card_controller, affected_card) )
		{
			if( is_this_dying(affected_card_controller, affected_card) ){
				int pos;
				for (pos = 0; pos <= 10; ++pos){
						if( instance->targets[pos].player == -1 ){
							SET_BYTE1(instance->targets[pos].player) = affected_card_controller;
							SET_BYTE0(instance->targets[pos].player) = affected_card;
							break;
						}
						if( instance->targets[pos].card == -1 ){
							SET_BYTE1(instance->targets[pos].card) = affected_card_controller;
							SET_BYTE0(instance->targets[pos].card) = affected_card;
							break;
						}
				}
				instance->targets[11].player = 66;
			}
		}
	}

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[11].player = 0;
		int i;
		target_t dead_array[22];
		int dac = 0;
		for (i = 0; i <= 10; i++){
			if( instance->targets[i].player != -1 ){
				dead_array[dac].player = BYTE1(instance->targets[i].player);
				dead_array[dac].card = BYTE0(instance->targets[i].player);
				instance->targets[i].player = -1;
				dac++;
			}
			if( instance->targets[i].card != -1 ){
				dead_array[dac].player = BYTE1(instance->targets[i].card);
				dead_array[dac].card = BYTE0(instance->targets[i].card);
				instance->targets[i].card = -1;
				dac++;
			}
		}

		int owner, position;
		const int *grave = get_grave(player);
		dac--;
		for (; dac >= 0; --dac){
			if (find_in_owners_graveyard(dead_array[dac].player, dead_array[dac].card, &owner, &position)){
				ASSERT(owner == player);
				// Correct to check opponent_is_valid_target() every iteration; there's scenarios where its value changes as a result of
				// 		paying life.
				if( can_pay_life(1-player, 3) &&
					opponent_is_valid_target(player, card) &&
					DIALOG(player, card, EVENT_ACTIVATE,
						   DLG_RANDOM, DLG_WHO_CHOOSES(1-player), DLG_SMALLCARD_ID(grave[position]), DLG_NO_STORAGE, DLG_NO_CANCEL,
						   "Return to opponent's hand", 1, 1,
						   "Pay 3 life", 1, 1) == 2
				  ){
					lose_life(1-player, 3);
				}
				else {
					if (player == HUMAN){
						ai_modifier -= calc_initial_attack_rating_by_iid(player, grave[position]) / 4;
					}
					from_grave_to_hand(player, position, TUTOR_HAND);
				}
			}
		}
	}

	return 0;
}

int card_desperate_stand(int player, int card, event_t event){

	/* Desperate Stand	|R|W
	 * Sorcery
	 * Strive - ~ costs |R|W more to cast for each target beyond the first.
	 * Any number of target creatures each get +2/+0 and gain first strike and vigilance until end of turn. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		strive(player, card, event,  &td, "TARGET_CREATURE", MANACOST_RW(1, 1));
	}

	if( event == EVENT_RESOLVE_SPELL  ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 2, 0, KEYWORD_FIRST_STRIKE, SP_KEYWORD_VIGILANCE);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_disciple_of_deceit(int player, int card, event_t event){
	/* Disciple of Deceit	|U|B
	 * Creature - Human Rogue 1/1
	 * Inspired - Whenever ~ becomes untapped, you may discard a nonland card. If you do, search your library for a card with the same converted mana cost as
	 * that card, reveal it, put it into your hand, then shuffle your library. */

	if( inspired(player, card, event) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, "Select a nonland card.");
		this_test.type_flag = DOESNT_MATCH;
		this_test.zone = TARGET_ZONE_HAND;

		if ( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				int cmc = get_cmc(player, selected);
				discard_card(player, selected);

				test_definition_t this_test2;
				new_default_test_definition(&this_test2, TYPE_ANY, "");
				scnprintf(this_test2.message, 100, "Select a card with CMC %d.", cmc);
				this_test2.cmc = cmc;
				new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test2);
			}
		}
	}
	return 0;
}

int card_fleetfeather_cockatrice(int player, int card, event_t event){
	/* Fleetfeather Cockatrice	|3|G|U
	 * Creature - Cockatrice 3/3
	 * Flash
	 * Flying, deathtouch
	 * |5|G|U: Monstrosity 3. */

	if (flash(player, card, event)){
		return 1;
	}

	deathtouch(player, card, event);

	return monstrosity(player, card, event, MANACOST_XUG(5, 1, 1), 3);
}

int card_iroas_god_of_victory(int player, int card, event_t event)
{
	/* Iroas, God of Victory	|2|R|W
	 * Legendary Enchantment Creature - God 0/0
	 * Indestructible
	 * As long as your devotion to |Sred and |Swhite is less than seven, Iroas isn't a creature.
	 * Creatures you control have menace.
	 * Prevent all damage that would be dealt to attacking creatures you control. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_RED, COLOR_WHITE, 7);

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if ((event == EVENT_DECLARE_BLOCKERS || event == EVENT_ABILITIES) && !is_humiliated(player, card, AL_STATIC)){
		int c;
		for (c = 0; c < active_cards_count[player]; ++c){
			if (in_play(player, c) && is_what(player, c, TYPE_CREATURE) ){
				menace(player, c, event);
			}
		}
	}

	card_instance_t* source = damage_being_prevented(event);
	if (source
		&& source->damage_target_card != -1 && source->damage_target_player == player
		&& is_what(source->damage_target_player, source->damage_target_card, TYPE_CREATURE)
		&& is_attacking(source->damage_target_player, source->damage_target_card) )
	{
		int amount = source->info_slot;
		prevent_damage(affected_card_controller, affected_card, amount);
	}

	return 0;
}

int card_keranos_god_of_storms(int player, int card, event_t event)
{
	/* Keranos, God of Storms	|3|U|R	0x200bbd7
	 * Legendary Enchantment Creature - God 0/0
	 * Indestructible
	 * As long as your devotion to |Sblue and |Sred is less than seven, ~ isn't a creature.
	 * Reveal the first card you draw on each of your turns. Whenever you reveal a land card this way, draw a card. Whenever you reveal a nonland card this way, ~ deals 3 damage to any target. */
	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_BLUE, COLOR_RED, 7);

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

  if (trigger_condition == TRIGGER_CARD_DRAWN && current_turn == player && cards_drawn_this_turn[player] == 1
	  && card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY))
	{
	  int p = trigger_cause_controller, c = trigger_cause;

	  reveal_card(player, card, p, c);

	  if (is_what(p, c, TYPE_LAND))
		draw_a_card(player);
	  else
		{
		  target_definition_t td;
		  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		  td.allow_cancel = 0;

		  get_card_instance(player, card)->number_of_targets = 0;
		  if (can_target(&td) && pick_target(&td, "TARGET_ANY"))
			damage_target0(player, card, 3);
		}
	}

  return 0;
}

int card_kruphix_god_of_horizons(int player, int card, event_t event)
{
  /* Kruphix, God of Horizons	|3|G|U
   * Legendary Enchantment Creature - God 4/7
   * Indestructible
   * As long as your devotion to green and blue is less than seven, ~ isn't a creature.
   * You have no maximum hand size.
   * If unused mana would empty from your mana pool, that mana becomes colorless instead. */

  int result;
  check_legend_rule(player, card, event);
  indestructible(player, card, event);
  generic_creature_with_devotion(player, card, event, 3, 2, 7);
  if (event != EVENT_CAN_CAST)
  {
    if ((event == EVENT_MAX_HAND_SIZE) && (current_turn == player))
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        event_result = event_result + 99;
      }
    }
    result = card_horizon_stone(player, card, event);
    return result;
  }
  result = basic_spell(player, card, EVENT_CAN_CAST);
  return result;
}

int card_nyx_weaver(int player, int card, event_t event){
	/* Nyx Weaver	|1|B|G
	 * Enchantment Creature - Spider 2/3
	 * Reach
	 * At the beginning of your upkeep, put the top two cards of your library into your graveyard.
	 * |1|B|G, Exile ~: Return target card from your graveyard to your hand. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		mill(player, 2);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_ANY, "Select target card to return to hand.");

	if (event == EVENT_RESOLVE_ACTIVATION){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}
	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_RFG_ME, MANACOST_XBG(1, 1, 1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
}

int card_pharika_god_of_affliction(int player, int card, event_t event)
{
	/* Pharika, God of Affliction	|1|B|G
	 * Legendary Enchantment Creature - God 5/5
	 * Indestructible
	 * As long as your devotion to |Sblack and |Sgreen is less than seven, ~ isn't a creature.
	 * |B|G: Exile target creature card from a graveyard. Its owner puts a 1/1 |Sblack and |Sgreen Snake enchantment creature token with deathtouch onto the
	 * battlefield. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_BLACK, COLOR_GREEN, 7);

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card to exile.");

	if (event == EVENT_CAN_ACTIVATE){
		return generic_activated_ability_with_target_in_grave(player, card, event,0, MANACOST_BG(1, 1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
	}

	if (event == EVENT_ACTIVATE ){
		generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_BG(1, 1), 0, NULL, NULL,
														0, &test, GS_GRAVE_RECYCLER_BOTH_GRAVES);
		if( spell_fizzled != 1 ){
			card_instance_t* instance = get_card_instance(player, card);
			ai_modifier += (instance->targets[0].player == 1-player ? 24 : -24);
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if (selected != -1){
			rfg_card_from_grave(instance->targets[0].player, selected);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SNAKE, &token);
			token.t_player = instance->targets[0].player;
			token.color_forced = COLOR_TEST_GREEN | COLOR_TEST_BLACK;
			token.special_flags2 = SF2_ENCHANTED_EVENING;
			token.s_key_plus = SP_KEYWORD_DEATHTOUCH;
			generate_token(&token);
		}
	}

	return 0;
}

int card_revel_of_the_fallen_god(int player, int card, event_t event){
	/* Revel of the Fallen God	|3|R|R|G|G
	 * Sorcery
	 * Put four 2/2 |Sred and |Sgreen Satyr creature tokens with haste onto the battlefield. */

	if ( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SATYR, &token);
		token.qty = 4;
		token.pow = token.tou = 2;
		token.color_forced = COLOR_TEST_RED | COLOR_TEST_GREEN;
		token.s_key_plus = SP_KEYWORD_HASTE;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_stormchaser_chimera(int player, int card, event_t event){
	/* Stormchaser Chimera	|2|U|R
	 * Creature - Chimera 2/3
	 * Flying
	 * |2|U|R: Scry 1, then reveal the top card of your library. ~ gets +X/+0 until end of turn, where X is that card's converted mana cost. */

	if ( event == EVENT_RESOLVE_ACTIVATION ){
		scry(player, 1);
		int* deck = deck_ptr[player];
		if( deck[0] != -1 ){
			reveal_card_iid(player, card, deck[0]);
			int cmc = get_cmc_by_internal_id(deck[0]);

			card_instance_t* instance = get_card_instance(player, card);
			if (cmc > 0 && in_play(instance->parent_controller, instance->parent_card)){
				pump_until_eot_merge_previous(player, card, instance->parent_controller, instance->parent_card, cmc, 0);
			}
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XUR(2, 1, 1), 0, NULL, NULL);
}

int card_underworld_coinsmith(int player, int card, event_t event)
{
	/* Underworld Coinsmith	|W|B
	 * Enchantment Creature - Human Cleric 2/2
	 * Constellation - Whenever ~ or another enchantment enters the battlefield under your control, you gain 1 life.
	 * |W|B, Pay 1 life: Each opponent loses 1 life. */

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	constellation(player, card, event);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		lose_life(1-player, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_BW(1, 1), 1, NULL, NULL);
}

/***********
* Artifact *
***********/

int card_armory_of_iroas(int player, int card, event_t event){
	/* Armory of Iroas	|2
	 * Artifact - Equipment
	 * Whenever equipped creature attacks, put a +1/+1 counter on it.
	 * Equip |2 */

	if (event == EVENT_ACTIVATE && !is_equipping(player, card)){ ai_modifier += 10; }
	if (is_equipping(player, card)){
		card_instance_t* instance = get_card_instance(player, card);
		int p = instance->damage_target_player, c = instance->damage_target_card;
		store_attackers(player, card, event, 0, p, c, NULL);
		dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
		if (event == EVENT_DAT_ABILITY){ add_counters(player, card, p, c, COUNTER_P1_P1, 1); }
	}
	return basic_equipment(player, card, event, 2);
}

int card_chariot_of_victory(int player, int card, event_t event){
	/* Chariot of Victory	|3
	 * Artifact - Equipment
	 * Equipped creature has first strike, trample, and haste.
	 * Equip |1 */
	return vanilla_equipment(player, card, event, 1, 0, 0, KEYWORD_FIRST_STRIKE | KEYWORD_TRAMPLE, SP_KEYWORD_HASTE);
}

int card_deserters_quarters(int player, int card, event_t event){
	/* Deserter's Quarters	|2
	 * Artifact
	 * You may choose not to untap ~ during your untap step.
	 * |6, |T: Tap target creature. It doesn't untap during its controller's untap step for as long as ~ remains tapped. */

	choose_to_untap(player, card, event);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t* instance = get_card_instance(player, card);
		int pp = instance->parent_controller, pc = instance->parent_card;

		tap_card(instance->targets[0].player, instance->targets[0].card);
		does_not_untap_until_im_tapped(pp, pc, instance->targets[0].player, instance->targets[0].card);

		card_instance_t* parent = get_card_instance(pp, pc);
		parent->targets[1].player = instance->targets[0].player;
		parent->targets[1].card = instance->targets[0].card;
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(6), 0, &td, "TARGET_CREATURE");
}

/* Gold-Forged Sentinel	|6 => vanilla
 * Artifact Creature - Chimera 4/4
 * Flying */

int card_hall_of_triumph(int player, int card, event_t event)
{
	/* Hall of Triumph	|3
	 * Legendary Artifact
	 * As ~ enters the battlefield, choose a color.
	 * Creatures you control of the chosen color get +1/+1. */

	check_legend_rule(player, card, event);

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1 << choose_a_color_and_show_legacy(player, card, player, -1);
	}

	boost_creature_by_color(player, card, event, get_card_instance(player, card)->info_slot, 1, 1, 0, BCT_CONTROLLER_ONLY | BCT_NO_SLEIGHT);

	return 0;
}

/*******
* Land *
*******/

int card_mana_confluence(int player, int card, event_t event){

	/* Mana Confluence	""
	 * Land
	 * |T, Pay 1 life: Add one mana of any color to your mana pool. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( !can_pay_life(player, 1) ){
			return 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( life[player] < 6 ){
			ai_modifier -= 25;
		}

		cancel = 0;	// just in case
		mana_producer(player, card, event);
		if (cancel != 1 && !(mana_colors_added[player] & color_to_color_test(tapped_for_mana_color))){
			lose_life(player, 1);
		}
		return 0;
	}

	return mana_producer(player, card, event);
}

/* Temple of Epiphany	"" => New Benalia
 * Land
 * ~ enters the battlefield tapped.
 * When ~ enters the battlefield, scry 1.
 * |T: Add |U or |R to your mana pool. */

/* Temple of Malady	"" => New Benalia
 * Land
 * ~ enters the battlefield tapped.
 * When ~ enters the battlefield, scry 1.
 * |T: Add |B or |G to your mana pool. */

// Hero's Artifacts
int card_lash_of_the_tyrant(int player, int card, event_t event){
  /* Lash of the Tyrant	""
   * Hero Artifact - Equipment
   * Equipped creature gets +1/+2 and has deathtouch.
   * Equip |2 */

	return vanilla_equipment(player, card, event, 2, 1, 2, 0, SP_KEYWORD_DEATHTOUCH);
}

int card_cloak_of_the_philospher(int player, int card, event_t event){
  /* Cloak of the Philosopher	""
   * Hero Artifact - Equipment
   * Equipped creature gets +1/+1.
   * |2: Untap equipped creature.
   * Equip |2 */

	if( is_equipping(player, card) ){
		return vanilla_equipment(player, card, event, 2, 1, 1, 0, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = 0;
		if( generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_X(get_updated_equip_cost(player, card, 2)), 0, NULL, NULL) ){
			result |= check_for_equipment_targets(player, card);
		}
		if( is_equipping(player, card) && generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL) ){
			result |= 1;
		}
		return result;
	}

	if( event == EVENT_ACTIVATE ){
		int can_equip = generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_X(get_updated_equip_cost(player, card, 2)), 0, NULL, NULL) &&
						check_for_equipment_targets(player, card);
		int can_untap = is_equipping(player, card) && generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Equip", can_equip, is_equipping(player, card) ? 1 : 10,
							"Untap equipped creature", can_untap, is_equipping(player, card) && is_tapped(instance->damage_target_player, instance->damage_target_card) ? 11 : 1
							);

		if( choice == 0 ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(get_updated_equip_cost(player, card, 2))) &&
				select_target(player, card, &td, "Select a creature to equip.", NULL)
			  ){
				instance->number_of_targets = 1;
				instance->info_slot = 66+choice;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == 2 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
				instance->info_slot = 66+choice;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && valid_target(&td) ){
			equip_target_creature(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == 67 ){
			untap_card(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_spear_of_the_general(int player, int card, event_t event){
  /* Spear of the General	""
   * Hero Artifact - Equipment
   * Equipped creature gets +2/+0 and has first strike.
   * Equip |2 */

	return vanilla_equipment(player, card, event, 2, 2, 0, KEYWORD_FIRST_STRIKE, 0);
}

int card_axe_of_the_warmonger(int player, int card, event_t event){
  /* Axe of the Warmonger	""
   * Hero Artifact - Equipment
   * Equipped creature gets +2/+1 and has haste.
   * Equip |2 */

	return vanilla_equipment(player, card, event, 2, 2, 1, 0, SP_KEYWORD_HASTE);
}

int card_bow_of_the_hunter(int player, int card, event_t event){
  /* Bow of the Hunter	""
   * Hero Artifact - Equipment
   * Equipped creature has "|T: This creature deals 2 damage to any target."
   * Equip |2 */

	if( is_equipping(player, card) ){
		return vanilla_equipment(player, card, event, 2, 1, 1, 0, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.allowed_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = 0;
		if( generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_X(get_updated_equip_cost(player, card, 2)), 0, NULL, NULL) ){
			result |= check_for_equipment_targets(player, card);
		}
		if( is_equipping(player, card) &&
			generic_activated_ability(instance->damage_target_player, instance->damage_target_card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_ZONE_CREATURE_OR_PLAYER")
		  ){
			result |= 1;
		}
		return result;
	}

	if( event == EVENT_ACTIVATE ){
		int can_equip = generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_X(get_updated_equip_cost(player, card, 2)), 0, NULL, NULL) &&
						check_for_equipment_targets(player, card);
		int can_damage = is_equipping(player, card) &&
						generic_activated_ability(instance->damage_target_player, instance->damage_target_card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0,
													0, &td1, "TARGET_ZONE_CREATURE_OR_PLAYER");
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Equip", can_equip, is_equipping(player, card) ? 1 : 10,
							"Equipped creature deals 2 damages", can_damage, 10
							);

		if( choice == 0 ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == 1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(get_updated_equip_cost(player, card, 2))) &&
				select_target(player, card, &td, "Select a creature to equip.", NULL)
			  ){
				instance->number_of_targets = 1;
				instance->info_slot = 66+choice;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == 2 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST0) &&
				select_target(instance->damage_target_player, instance->damage_target_card, &td1, "Select a target creature or player.", NULL)
			  ){
				tap_card(instance->damage_target_player, instance->damage_target_card);
				instance->info_slot = 66+choice;
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && valid_target(&td) ){
			equip_target_creature(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
		if( instance->info_slot == 67 && validate_target(instance->damage_target_player, instance->damage_target_card, &td1, 0) ){
			damage_target0(instance->damage_target_player, instance->damage_target_card, 2);
		}
	}

	return 0;
}

// Defeat a God challenge deck
/*
Legendary Enchantment Creature
1 Xenagos Ascended

Enchantment Creatures
2 Maddened Oread

Creatures
16 Ecstatic Piper
4 Pheres-Band Revellers
6 Rollicking Throng
2 Serpent Dancers
2 Wild Maenads

Sorceries
7 Impulsive Charge
2 Impulsive Destruction
4 Impulsive Return
2 Rip to Pieces
3 Xenagos's Scorn
5 Xenagos's Strike

Enchantments
2 Dance of Flame
2 Dance of Panic

------

How to play Defeat a God

The challenge uses the regular Magic rules, with the following exceptions:

Player

    You start with up to three different Hero cards on the battlefield. (You don't need a Hero to play.)
    You attack Xenagos Ascended and the Revellers directly with your creatures. Any number of creatures can attack a single Reveler
    You may target Xenagos Ascended, as though it were a player.

Xenagos

    Xenagos Ascended and two cards named Rollicking Throng begin the game on the Battlefield. Shuflle the rest of Xenagos's deck to begin the game.
    At the beginning of Xenagos' main phase each turn, reveal the top two cards of the Xenagos's library. Then Xenagos casts those cards.
    Xenagos Ascended and reveller creatures don't attack unless the card specifically says they do.
    You make the choice if Xenagos needs to make a decision.
    Ignore effects that would cause Xenagos to draw or discard cards, or perform any other impossible actions.
    If one of Xenagos's pemanents would go to any zone other than his library or graveyard, that card is put into Xenagos' graveyard.
*/

int card_defeat_a_god(int player, int card, event_t event){

	if( upkeep_trigger(player, card, event) ){
		int *deck = deck_ptr[player];
		int amount = 2;
		while( amount ){
			if( deck[0] != -1 ){
				play_card_in_deck_for_free(player, player, 0);
			}
			else{
				if( ! count_subtype(player, TYPE_CREATURE, -1) ){
					lose_the_game(player);
				}
			}
			amount--;
		}
	}

	if (trigger_condition == TRIGGER_REPLACE_CARD_DRAW
		&& reason_for_trigger_controller == player
		&& affect_me(player, card)
		&& !suppress_draw
	   ){
		if (event == EVENT_TRIGGER){
			event_result = RESOLVE_TRIGGER_MANDATORY;
		}
		else if (event == EVENT_RESOLVE_TRIGGER){
				suppress_draw = 1;
		}
	}

	if( event == EVENT_PREVENT_DAMAGE ){
		card_instance_t *damage = get_card_instance(affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->damage_target_player == player &&
			damage->damage_target_card == -1
		  ){
			damage->info_slot = 0;
		}
	}

	return vanguard_card(player, card, event, 0, 20, 13);
}

#define CARD_ID_XENAGOS_ASCENDED CARD_ID_GRIZZLY_BEARS

static void pump_xenagos(int player, int card, int pow, int tou, int key, int s_key){
	int i;
	for(i=0; i<active_cards_count[player]; i++){
		if( in_play(player, i) && get_id(player, i) == CARD_ID_XENAGOS_ASCENDED ){
			pump_ability_until_eot(player, card, player, i, pow, tou, key, s_key);
			break;
		}
	}
}

int card_xenagos_ascended(int player, int card, event_t event){
  /* Xenagos Ascended	""
   * Legendary Enchantment Creature - God 6/5
   * ~ can't leave the battlefield as long as a Reveler is on the battlefield.
   * When ~ leaves the battlefield, each player wins the game. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	if( ! check_battlefield_for_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_REVELER) ){
		lose_the_game(player);
	}

	return 0;
}

int card_rollicking_throng(int player, int card, event_t event){
  /* Rollicking Throng	""
   * Creature - Human Reveler 1/3
   * When ~ enters the battlefield, reveal the top card of Xenagos's library and Xenagos casts that card. (This ability doesn't trigger as the game begins.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			play_card_in_deck_for_free(player, player, 0);
		}
	}

	return 0;
}

int impulsive_charge_legacy(int player, int card, event_t event){

	if( beginning_of_combat(player, card, event, player) ){
		pump_subtype_until_eot(player, card, player, SUBTYPE_REVELER, 0, 0, 0, SP_KEYWORD_HASTE | SP_KEYWORD_MUST_ATTACK);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_impulsive_charge(int player, int card, event_t event){
  /* Impulsive Charge	""
   * Sorcery
   * At the beginning of combat this turn, all Revelers gain haste until end of turn and attack this combat if able. */

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &impulsive_charge_legacy);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_xenagos_strike(int player, int card, event_t event){
  /* Xenagos's Strike	""
   * Sorcery
   * ~ deals 4 damage to each player. */

	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(1-player, 4, player, card);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_ecstatic_piper(int player, int card, event_t event)
{
  /* Ecstatic Piper	""
   * Creature - Satyr Reveler 2/2
   * When ~ enters the battlefield, Xenagos Ascended attacks this turn if able.
   * Hero's Reward - When ~ leaves the battlefield, each player gains 2 life. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    pump_xenagos(player, card, 0, 0, 0, SP_KEYWORD_MUST_ATTACK);
    leaves_play(player, card, EVENT_ETB, 1, 1);
    return 0;
  }
  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  gain_life(1 - player, 2);
  return 0;
}

int card_wild_maenads(int player, int card, event_t event)
{
  /* Wild Maenads	""
   * Creature - Human Reveler 3/1
   * First strike
   * Hero's Reward - When ~ leaves the battlefield, each player gains 3 life. */

  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
    gain_life(1-player, 3);
  return 0;
}

int card_pheres_band_revelers(int player, int card, event_t event)
{
  /* Pheres-Band Revelers	""
   * Creature - Centaur Reveler 4/4
   * Hero's Reward - When ~ leaves the battlefield, each player draws a card. */

  leaves_play(player, card, event, 1, 1);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
    draw_cards(1-player, 1);
  return 0;
}

// serpent dancers --> pheres band revelers

int impulsive_return_legacy(int player, int card, event_t event){

	if( beginning_of_combat(player, card, event, player) ){
		damage_player(1-player, count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_REVELER), player, card);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

#define CARD_ID_ECSTATIC_PIPER CARD_ID_SCRYB_SPRITES

int card_impulsive_return(int player, int card, event_t event){
  /* Impulsive Return	""
   * Sorcery
   * Return two cards named Ecstatic Piper from Xenagos's graveyard to the battlefield. At the beginning of combat this turn, ~ deals damage to each player equal to the number of Revelers on the battlefield. */

	if( event == EVENT_RESOLVE_SPELL ){
		const int *grave = get_grave(player);
		int count = count_graveyard(player)-1;
		int returned = 0;
		while( count > -1 && returned < 2 ){
				if( cards_data[grave[count]].id == CARD_ID_ECSTATIC_PIPER ){
					reanimate_permanent(player, card, player, count, REANIMATE_DEFAULT);
					returned++;
				}
				count--;
		}
		create_legacy_effect(player, card, &impulsive_return_legacy);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_xenagos_scorn(int player, int card, event_t event){
  /* Xenagos's Scorn	""
   * Sorcery
   * Xenagos Ascended gains trample until end of turn and attacks this turn if able. */

	if( event == EVENT_RESOLVE_SPELL ){
		pump_xenagos(player, card, 0, 0, KEYWORD_TRAMPLE, SP_KEYWORD_MUST_ATTACK);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_impulsive_destruction(int player, int card, event_t event){
  /* Impulsive Destruction	""
   * Sorcery
   * Each player may sacrifice an artifact or enchantment. ~ deals 3 damage to each player who didn't sacrifice a permanent this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( ! sacrifice(player, card, 1-player, 0, TYPE_ARTIFACT | TYPE_ENCHANTMENT, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
			damage_player(1-player, 3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_maddened_oread(int player, int card, event_t event)
{
  /* Maddened Oread	""
   * Enchantment Creature - Nymph Reveler 4/2
   * As long as five or more Revelers are on the battlefield, ~ attacks each turn if able.
   * Hero's Reward - When ~ leaves the battlefield, each player gains 3 life. */

  int result;
  if (event == EVENT_BEGIN_COMBAT)
  {
    if (current_turn == player)
    {
      result = count_subtype(2, 0x47, 0xe6);
      if (4 < result)
      {
        attack_if_able(player, card, EVENT_BEGIN_COMBAT);
      }
    }
    leaves_play(player, card, EVENT_BEGIN_COMBAT, 1, 1);
    return 0;
  }
  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  gain_life(1 - player, 3);
  return 0;
}

int rip_to_pieces_legacy(int player, int card, event_t event){

	if( beginning_of_combat(player, card, event, player) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_damage_all(player, card, ANYBODY, 1, NDA_PLAYER_TOO | NDA_ALL_CREATURES, &this_test);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_rip_to_pieces(int player, int card, event_t event){
  /* Rip to Pieces	""
   * Sorcery
   * At the beginning of combat this turn, each Reveler deals 1 damage to each player and each creature those players control. */

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, &rip_to_pieces_legacy);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_dance_of_panic(int player, int card, event_t event){
  /* Dance of Panic	""
   * Enchantment
   * As long as five or more Revelers are on the battlefield, all Revelers have haste and attack each turn if able. */

	if (event == EVENT_MUST_ATTACK && current_turn == player && count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_REVELER) > 4 ){
		int i;
		for(i=active_cards_count[player]-1; i>-1; i--){
			if( in_play(player, i) && has_subtype(player, i, SUBTYPE_REVELER) && is_what(player, i, TYPE_CREATURE) ){
				attack_if_able(player, i, event);
			}
		}
	}

	if (event == EVENT_ABILITIES && affected_card_controller == player && count_subtype(ANYBODY, TYPE_PERMANENT, SUBTYPE_REVELER) > 4 ){
		haste(affected_card_controller, affected_card, event);
	}

	return global_enchantment(player, card, event);
}

int card_dance_of_flame(int player, int card, event_t event){
  /* Dance of Flame	""
   * Enchantment
   * Whenever a Reveler attacks, ~ deals 1 damage to each player. */

	if (event == EVENT_DECLARE_ATTACKERS){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_REVELER;
		store_attackers(player, card, event, DAT_SEPARATE_TRIGGERS, player, -1, &test);
	}
	dat_ability(player, card, event, DAT_SEPARATE_TRIGGERS | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		damage_player(1-player, 1, player, card);
	}
	return global_enchantment(player, card, event);
}
