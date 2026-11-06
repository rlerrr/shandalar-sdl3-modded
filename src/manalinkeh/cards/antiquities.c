#include "manalink.h"

/* Canonical recovered helper declarations. */
const char *damage_from_artifact_source(int who_chooses, int player, int card, int unused_source_player, int unused_source_card);
static int kill_legacy_if_source_is_not_in_play_or_untapped(int player, int card, event_t event);
int pump_until_source_is_in_play_and_tapped(int player, int card, int t_player, int t_card,
                                                 int pow, int tou, int key, int s_key, int mode);

// functions
static int is_artifact_creature(int player, int card){
	return is_what(player, card, TYPE_CREATURE) && is_what(player, card, TYPE_ARTIFACT);
}

enum{
	PUSIIPAT_TAPPED = 1,
};

static int kill_legacy_if_source_is_not_in_play_or_untapped(int player, int card, event_t event)
{
  unsigned int source_flags;
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  result = instance->targets[1].player;
  if (-1 < result)
  {
    other_leaves_play(player, card, result, instance->targets[1].card, event, 0, 0);
    source_flags = instance->targets[3].player;
    if ((((0 < (int)source_flags) && ((source_flags & 1) != 0)) &&
         (result = is_tapped(instance->targets[1].player, instance->targets[1].card),
          result == 0)) ||
        (event == EVENT_LEAVES_PLAY_ABILITY))
    {
      kill_card(instance->targets[2].player, instance->targets[2].card, KILL_EXILE);
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }
  return 0;
}

int pump_until_source_is_in_play_and_tapped(int player, int card, int t_player, int t_card,
                                                 int pow, int tou, int key, int s_key, int mode)
{
  int result;
  card_instance_t *instance;
  int l2;
  pump_ability_t pump;

  default_pump_ability_definition(player, card, &pump, pow, tou, key, s_key);
  result = pump_ability(player, card, t_player, t_card, &pump);
  instance = get_card_instance(player, card);
  instance->targets[1].player = t_player;
  instance->targets[1].card = t_card;
  l2 = create_targetted_legacy_effect(
      player, card, kill_legacy_if_source_is_not_in_play_or_untapped, t_player, t_card);
  instance = get_card_instance(player, l2);
  instance->targets[1].card = card;
  instance->targets[1].player = player;
  instance->targets[2].player = player;
  instance->targets[2].card = result;
  instance->targets[3].player = mode;
  add_status(player, l2, 0x10000);
  return result;
}

// cards

int card_amulet_of_kroog(int player, int card, event_t event){
	/* Amulet of Kroog	|2	0x200c27b
	 * Artifact
	 * |2, |T: Prevent the next 1 damage that would be dealt to any target this turn. */
	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_X(2), 0, &td, "TARGET_DAMAGE");
}

int card_argivian_archaeologist(int player, int card, event_t event){
/* CARD_ID_ARGIVIAN_ARCHAEOLOGIST	467
Argivian Archaeologist	|1|W|W
Creature - Human Artificer 1/1
|W|W, |T: Return target artifact card from your graveyard to your hand. */

	if (!IS_GAA_EVENT(event))
		return 0;

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact card");

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_W(2), 0, NULL, NULL) ){
			if( count_graveyard_by_type(player, TYPE_ARTIFACT) ){
				return ! graveyard_has_shroud(2);
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_W(2)) ){
			if( select_target_from_grave_source(player, card, player, 0, AI_MAX_VALUE, &this_test, 0) != -1 ){
				tap_card(player, card);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return 0;
}

static const char* is_damaging_artifact_creature(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	card_instance_t *instance = get_card_instance(player, card);
	if( instance->internal_card_id == damage_card ){
		if( is_artifact_creature(instance->damage_target_player, instance->damage_target_card) ){
			return NULL;
		}
	}

	return "must be damage to an artifact creature";
}

int card_argivian_blacksmith(int player, int card, event_t event){
/* CARD_ID_ARGIVIAN_BLACKSMITH	468
Argivian Blacksmith	|1|W|W
Creature - Human Artificer 2/2
|T: Prevent the next 2 damage that would be dealt to target artifact creature this turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(is_damaging_artifact_creature);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
		if( dmg->info_slot >= 2 ){
			dmg->info_slot-=2;
		}
		else{
			dmg->info_slot = 0;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST0, 0, &td, "TARGET_DAMAGE");
}

int card_argothian_pixies(int player, int card, event_t event)
{
  /* CARD_ID_ARGOTHIAN_PIXIES	469
   * Argothian Pixies	|1|G
   * Creature - Faerie 2/1
   * ~ can't be blocked by artifact creatures.
   * Prevent all damage that would be dealt to ~ by artifact creatures. */

  card_instance_t *instance;
  int result;
  test_definition_t this_test;
  test_definition_t this_test2;
  if ((((event == EVENT_CHECK_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (current_phase < 0x18))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (1 - player == current_turn))
    {
      default_test_definition(&this_test, 0x42);
      this_test.type_flag = 0xb;
      this_test.state = 4;
      default_test_definition(&this_test2, 2);
      this_test2.type_flag = 9;
      this_test2.state = 4;
      result = check_battlefield_for_special_card(player, card, current_turn, 0, &this_test);
      if (result != 0)
      {
        result = check_battlefield_for_special_card(player, card, current_turn, 0, &this_test2);
        if (result == 0)
        {
          check_abilities_toughness = check_abilities_toughness + 0x3fff;
        }
      }
    }
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      if (((event == EVENT_BLOCK_LEGALITY) && (attacking_card_controller == player)) &&
          (attacking_card == card))
      {
        result = is_artifact_creature(affected_card_controller, affected_card);
        if (result != 0)
        {
          event_result = 1;
        }
      }
      instance = damage_being_prevented(event);
      if (((instance != NULL) && (instance->damage_target_card == card)) &&
          (instance->damage_target_player == player))
      {
        result = is_what((int)instance->damage_source_player, instance->damage_source_card, 0x40);
        if (result != 0)
        {
          result = is_what((int)instance->damage_source_player, instance->damage_source_card, 2);
          if (result != 0)
          {
            instance->info_slot = 0;
          }
        }
      }
    }
  }
  return 0;
}

int card_argothian_treefolk(int player, int card, event_t event)
{
  /* CARD_ID_ARGOTHIAN_TREEFOLK	470
   * Argothian Treefolk	|3|G|G
   * Creature - Treefolk 3/5
   * Prevent all damage that would be dealt to ~ by artifact sources. */

  card_instance_t *instance;
  int result;
  test_definition_t this_test;
  test_definition_t this_test2;
  if ((((event == EVENT_CHECK_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (current_phase < 0x18))
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((result == 0) && (1 - player == current_turn))
    {
      default_test_definition(&this_test, 0x42);
      this_test.type_flag = 0xb;
      this_test.state = 4;
      default_test_definition(&this_test2, 2);
      this_test2.type_flag = 9;
      this_test2.state = 4;
      result = check_battlefield_for_special_card(player, card, current_turn, 0, &this_test);
      if (result != 0)
      {
        result = check_battlefield_for_special_card(player, card, current_turn, 0, &this_test2);
        if (result == 0)
        {
          check_abilities_toughness = check_abilities_toughness + 0x3fff;
        }
      }
    }
  }
  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (result == 0)
    {
      instance = damage_being_prevented(event);
      if (((instance != NULL) && (instance->damage_target_card == card)) &&
          (instance->damage_target_player == player))
      {
        result = is_what((int)instance->damage_source_player, instance->damage_source_card, 0x40);
        if (result != 0)
        {
          instance->info_slot = 0;
        }
      }
    }
  }
  return 0;
}

int card_armageddon_clock(int player, int card, event_t event)
{
  /* Armageddon Clock	|6
   * Artifact
   * At the beginning of your upkeep, put a doom counter on ~.
   * At the beginning of your draw step, ~ deals damage equal to the number of doom counters on it to each player.
   * |4: Remove a doom counter from ~. Any player may activate this ability but only during any upkeep step. */

  card_instance_t *instance;
  card_instance_t *clock;
  int cost_or_doom_count;
  card_instance_t *parent;
  int doom_count_or_mana;
  bool human_resolution;
  bool check_damage_trigger = true;
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_DOOM, 1);
    if (trigger_condition != TRIGGER_DRAW_PHASE)
    {
      return 0;
    }
  }
  else if ((trigger_condition != TRIGGER_DRAW_PHASE) && (event != EVENT_SHOULD_AI_PLAY))
    check_damage_trigger = false;
  if (check_damage_trigger)
  {
    if ((card == affected_card) &&
        (((player == affected_card_controller && (current_turn == player)) &&
          (reason_for_trigger_controller == player))))
    {
      if (event == EVENT_TRIGGER)
      {
        event_result = event_result | 2;
        return 0;
      }
      if ((event == EVENT_RESOLVE_TRIGGER) || (event == EVENT_SHOULD_AI_PLAY))
      {
        cost_or_doom_count = count_counters(player, card, COUNTER_DOOM);
        cost_or_doom_count = (unsigned int)(event == EVENT_SHOULD_AI_PLAY) + cost_or_doom_count;
        damage_player(current_turn, cost_or_doom_count, player, card);
        damage_player(1 - current_turn, cost_or_doom_count, player, card);
      }
    }
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    cost_or_doom_count = can_use_activated_abilities(player, card);
    if (((cost_or_doom_count != 0) &&
         (cost_or_doom_count = get_cost_mod_for_activated_abilities(player, card, 4, 0, 0, 0, 0, 0),
          current_phase == 4)) &&
        ((doom_count_or_mana = count_counters(player, card, COUNTER_DOOM),
          0 < doom_count_or_mana &&
              ((cost_or_doom_count = has_mana(g_event_player, 7, cost_or_doom_count),
                cost_or_doom_count != 0 && (clock = get_card_instance(player, card),
                                            ((unsigned int)clock->info_slot & 1) == 0))))))
    {
      if (g_event_player == 0)
      {
        return 1;
      }
      if (((unsigned char)trace_mode & 2) != 0)
      {
        return 1;
      }
      if ((life[1] < life[0]) || (cost_or_doom_count = count_counters(player, card, COUNTER_DOOM),
                                  life[1] < cost_or_doom_count))
      {
        g_activation_event_flags = g_activation_event_flags | 3;
        return 1;
      }
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    instance = get_card_instance(player, card);
    instance->info_slot = instance->info_slot | 1;
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    cost_or_doom_count = get_cost_mod_for_activated_abilities(player, card, 4, 0, 0, 0, 0, 0);
    instance = get_card_instance(player, card);
    parent = in_play(instance->parent_controller, instance->parent_card);
    if ((parent != NULL) && ((((human_resolution = ai_is_speculating != 1, parent->info_slot = 0,
                                human_resolution && ((g_event_player != 1 ||
                                                      (((unsigned char)trace_mode & 2) != 0)))) ||
                               (life[1] < life[0])) ||
                              (doom_count_or_mana = count_counters(player, card, COUNTER_DOOM),
                               life[1] < doom_count_or_mana))))
    {
      if (charge_mana_while_resolving(instance->parent_controller, instance->parent_card,
                                      EVENT_RESOLVE_ACTIVATION, g_event_player,
                                      COLOR_COLORLESS, cost_or_doom_count) == 0)
      {
        spell_fizzled = -1;
      }
      else
      {
        remove_counters(player, card, instance->parent_controller, instance->parent_card,
                        COUNTER_DOOM, 1);
      }
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      ai_modifier = ai_modifier + (life[1] - life[0]) * 0x18;
    }
  }
  else if ((event == EVENT_STATIC_EFFECTS) && (stack_size == 0))
  {
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return 0;
  }
  return 0;
}

int card_artifact_blast(int player, int card, event_t event){
/* CARD_ID_ARTIFACT_BLAST	472
Artifact Blast	|R
Instant
Counter target artifact spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}
	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ARTIFACT);
	return counterspell(player, card, event, &td, 0);
}

int card_artifact_possession(int player, int card, event_t event){
/* CARD_ID_ARTIFACT_POSSESSION	516
Artifact Possession	|2|B
Enchantment - Aura
Enchant artifact
Whenever enchanted artifact becomes tapped or a player activates an ability of enchanted artifact without |T in its activation cost,
~ deals 2 damage to that artifact's controller. */

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_TAP_CARD || event == EVENT_PLAY_ABILITY) && affect_me(p, c) ){
			damage_player(p, 2, player, card);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	return targeted_aura(player, card, event, &td, "TARGET_ARTIFACT");
}

int card_artifact_ward(int player, int card, event_t event){
/* CARD_ID_ARTIFACT_WARD	473
Artifact Ward	|W
Enchantment - Aura
Enchant creature
Enchanted creature can't be blocked by artifact creatures.
Prevent all damage that would be dealt to enchanted creature by artifact sources.
Enchanted creature can't be the target of abilities from artifact sources. */

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_PROT_ARTIFACTS, 0, 0, 0, 0);
}

int card_ashnods_altar(int player, int card, event_t event){
/* CARD_ID_ASHNODS_ALTAR	474
Ashnod's Altar	|3
Artifact
Sacrifice a creature: Add |C|C to your mana pool. */

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( can_produce_mana(player, card) && can_sacrifice_type_as_cost(player, count_subtype(player, TYPE_CREATURE, -1), TYPE_CREATURE ) ){
			declare_mana_available(player, COLOR_COLORLESS, 2*count_subtype(player, TYPE_CREATURE, -1));
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_produce_mana(player, card) && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
			return 1;
		}
	}

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
			produce_mana(player, COLOR_COLORLESS, 2);
			tapped_for_mana_color = -2;
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

int card_ashnods_battle_gear(int player, int card, event_t event){
/* CARD_ID_ASHNODS_BATTLE_GEAR	475
Ashnod's Battle Gear	|2
Artifact
You may choose not to untap ~ during your untap step.
|2, |T: Target creature you control gets +2/-2 for as long as ~ remains tapped. */

	choose_to_untap(player, card, event);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if (IS_AI(player)){
		td.toughness_requirement = 3 | TARGET_PT_GREATER_OR_EQUAL;
	}
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
												2, -2, 0, 0, PUSIIPAT_TAPPED);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_ashnods_transmogrant(int player, int card, event_t event)
{
  /* CARD_ID_ASHNODS_TRANSMOGRANT	476
   * Ashnod's Transmogrant	|1
   * Artifact
   * |T, Sacrifice ~: Put a +1/+1 counter on target nonartifact creature. That creature becomes an artifact in addition to its other types. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.illegal_type = TYPE_ARTIFACT;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 0, 0, 0, 0, 0, 0, 0,
                                       &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0xf, 0, 0, 0, 0, 0, 0, 0, &td,
                                "Select target nonartifact creature.");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      result = instance->targets[0].player;
      if (result != player)
      {
        ai_modifier = ai_modifier + -0x18;
      }
      instance = get_card_instance(result, instance->targets[0].card);
      if ((instance->regen_status & 0x800000) == 0)
      {
        return 0;
      }
      if ((instance->token_status & 0x800) != 0)
      {
        return 0;
      }
      ai_modifier = ai_modifier + -0x18;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP) &&
        (event != EVENT_CHECK_PUMP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.illegal_type = TYPE_ARTIFACT;
    instance = get_card_instance(player, card);
    if (event == EVENT_CHECK_PUMP)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0xd, 0, 0, 0, 0, 0, 0, 0,
                                         &td, NULL);
      if (result == 0)
      {
        return 0;
      }
      pumpable_power[player] = pumpable_power[player] + 1;
      pumpable_toughness[player] = pumpable_toughness[player] + 1;
      return 0;
    }
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                 COUNTER_P1_P1, 1);
    turn_into_artifact(instance->parent_controller, instance->parent_card,
                       instance->targets[0].player, instance->targets[0].card, 0);
    return 0;
  }
  return 0;
}

int card_atog2(int player, int card, event_t event){
/* CARD_ID_ATOG	477
Atog	|1|R
Creature - Atog 1/2
Sacrifice an artifact: ~ gets +2/+2 until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	if (event == EVENT_CHECK_PUMP){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST0, 0, NULL, NULL) &&
			can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT) )
		{
			pumpable_power[player]+=2;
			pumpable_toughness[player]+=2;
		}
	}

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT);
		}
	}

	if( event == EVENT_ACTIVATE ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				spell_fizzled = 1;
			}
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION ){
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 2, 2);
	}
	return 0;
}

static void destroy_wall_at_end_of_combat(intptr_t player, intptr_t card, int t_player, int t_card)
{
  if (has_subtype(t_player, t_card, SUBTYPE_WALL))
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, t_player, t_card);
}

int card_battering_ram(int player, int card, event_t event){
/* CARD_ID_BATTERING_RAM	478
Battering Ram	|2
Artifact Creature - Construct 1/1
At the beginning of combat on your turn, ~ gains banding until end of combat. (Any creatures with banding, and up to one without, can attack in a band. Bands are blocked as a group. If any creatures with banding you control are being blocked by a creature, you divide that creature's combat damage, not its controller, among any of the creatures it's blocking.)
Whenever ~ becomes blocked by a Wall, destroy that Wall at end of combat. */

  //0x422cc0
  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_ABILITIES && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
	{
	  if (current_turn == player && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2)
		event_result |= KEYWORD_BANDING;
	}

  /* EVENT_ABILITIES usually isn't dispatched between the start of combat and cards being chosen as attachers, so also add banding at the phase change.  Also
   * make sure abilities are promptly recalculated after combat. */
  if (event == EVENT_PHASE_CHANGED && current_turn == player && (current_phase == PHASE_DECLARE_ATTACKERS || current_phase == PHASE_MAIN2))
	{
	  instance->regen_status |= KEYWORD_RECALC_ABILITIES;
	  if (current_phase == PHASE_DECLARE_ATTACKERS)
		get_abilities(player, card, EVENT_ABILITIES, -1);
	}

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG) && current_turn == player)
	instance->destroys_if_blocked |= DIFB_DESTROYS_WALLS;

  if (event == EVENT_DECLARE_BLOCKERS && player == current_turn && (instance->state & STATE_ATTACKING) && !is_humiliated(player, card, AL_TRIG))
	for_each_creature_blocking_me(player, card, destroy_wall_at_end_of_combat, player, card);

  return 0;
}

/* CARD_ID_BRONZE_TABLET	479  --> hardcoded
Bronze Tablet	|6
Artifact
Remove ~ from your deck before playing if you're not playing for ante.
~ enters the battlefield tapped.
|4, |T: Exile ~ and target nontoken permanent an opponent owns. That player may pay 10 life.
If he or she does, put ~ into its owner's graveyard. Otherwise, that player owns ~ and you own the other exiled card. */

int card_candelabra_of_tawnos(int player, int card, event_t event){
/* CARD_ID_CANDELABRA_OF_TAWNOS	480
Candelabra of Tawnos	|1
Artifact
|X, |T: Untap X target lands. */

  // 0x422d90
  // Used externally
  if (event == EVENT_GET_SELECTED_CARD)
	load_recorded_action_code(0);

  if (!IS_ACTIVATING(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.preferred_controller = ANYBODY;

  if (event == EVENT_CAN_ACTIVATE)
	{
	  if (g_activation_scan_flags < 1)
		max_x_value = target_available(player, card, &td);

	  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(0), 0, NULL, NULL);
	}

  if (event == EVENT_ACTIVATE)
	{
	  if (player == AI)
		ai_modifier -= 24;

	  card_instance_t* instance = get_card_instance(player, card);

	  tapped_for_mana_color = -1;
	  instance->state |= STATE_TAPPED;

	  int store_max_x = max_x_value;

	  max_x_value = target_available(player, card, &td);
	  max_x_value = MIN(max_x_value, 10);
	  charge_mana_for_activated_ability(player, card, MANACOST_X(-1));

	  max_x_value = store_max_x;

	  if (spell_fizzled != 1)
		{
		  instance->number_of_targets = 0;
		  td.preferred_controller = player;

		  char prompt[100];
		  *prompt = 0;

		  int i;
		  for (i = 0; i < x_value; ++i)
			{
			  if (ai_is_speculating != 1)
				{
				  load_text(0, "CANDLEABRA_OF_TAWNOS");
				  scnprintf(prompt, 100, text_lines[0], i + 1, x_value);
				}

			  if (!select_target(player, card, &td, prompt, &instance->targets[i]))
				{
				  spell_fizzled = 1;
				  break;
				}

			  get_card_instance(instance->targets[i].player, instance->targets[i].card)->state |= STATE_CANNOT_TARGET | STATE_TARGETTED;
			}

		  for (i = 0; i < instance->number_of_targets; ++i)
			get_card_instance(instance->targets[i].player, instance->targets[i].card)->state &= ~(STATE_CANNOT_TARGET | STATE_TARGETTED);
		}

	  if (spell_fizzled == 1)
		{
		  instance->number_of_targets = 0;
		  instance->state &= ~STATE_TAPPED;
		}
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  untap_card(instance->targets[i].player, instance->targets[i].card);

	  get_card_instance(instance->parent_controller, instance->parent_card)->number_of_targets = 0;
	}

  return 0;
}

const char *damage_from_artifact_source(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  card_instance_t *damage;
  card_instance_t *source_permanent;
  unsigned int artifact_source;
  int result;

  damage = get_card_instance(player, card);
  if (damage->internal_card_id != damage_card)
  {
    return "must be a damage card";
  }
  if ((damage->damage_target_player == who_chooses) && (damage->damage_target_card == -1))
  {
    artifact_source = damage->targets[3].player;
    source_permanent = in_play((int)damage->damage_source_player, damage->damage_source_card);
    if (source_permanent == NULL)
    {
      artifact_source = artifact_source & 0x40;
    }
    else
    {
      artifact_source =
          is_what((int)damage->damage_source_player, damage->damage_source_card, 0x40);
    }
    if (artifact_source == 0)
    {
      return "must be damage from artifact source";
    }
    result = is_what((int)damage->damage_source_player, damage->damage_source_card, 2);
    if ((result == 0) || (result = check_special_flags((int)damage->damage_source_player,
                                                       damage->damage_source_card, 0x40),
                          result == 0))
    {
      return NULL;
    }
  }
  return "must be damage dealt to you";
}

int card_circle_of_protection_artifacts(int player, int card, event_t event){
/* CARD_ID_CIRCLE_OF_PROTECTION_ARTIFACTS	481
Circle of Protection: Artifacts	|1|W
Enchantment
|2: The next time an artifact source of your choice would deal damage to you this turn, prevent that damage. */

	if( ! IS_GAA_EVENT(event)  ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(damage_from_artifact_source);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card,
							get_card_instance(instance->targets[0].player, instance->targets[0].card)->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_X(2), 0, &td, "TARGET_DAMAGE");
}

int card_citanul_druid(int player, int card, event_t event)
{
  /* CARD_ID_CITANUL_DRUID	482
Citanul Druid	|1|G
Creature - Human Druid 1/1
Whenever an opponent casts an artifact spell, put a +1/+1 counter on ~. */

  sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ARTIFACT, NULL, player, card);
  if (event == EVENT_SPA)
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
  return 0;
}

int card_clay_statue(int player, int card, event_t event){
/* CARD_ID_CLAY_STATUE	483
Clay Statue	|4
Artifact Creature - Golem 3/1
|2: Regenerate ~. */

	return regeneration(player, card, event, MANACOST_X(2));
}

int card_clockwork_avian(int player, int card, event_t event){
/* CARD_ID_CLOCKWORK_AVIAN	484
Clockwork Avian	|5
Artifact Creature - Bird 0/4
Flying
~ enters the battlefield with four +1/+0 counters on it.
At end of combat, if ~ attacked or blocked this combat, remove a +1/+0 counter from it.
|X, |T: Put up to X +1/+0 counters on ~. This ability can't cause the total number of +1/+0 counters on ~ to be greater than four.
Activate this ability only during your upkeep. */
  // 0x426600

  return hom_clockwork(player, card, event, 4);
}

int card_colossus_of_sardia(int player, int card, event_t event){
/* CARD_ID_COLOSSUS_OF_SARDIA	485
Colossus of Sardia	|9
Artifact Creature - Golem 9/9
Trample (This creature can deal excess combat damage to defending player or planeswalker while attacking.)
~ doesn't untap during your untap step.
|9: Untap ~. Activate this ability only during your upkeep. */

	does_not_untap(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		untap_card(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_IN_YOUR_TURN | GAA_ONLY_ON_UPKEEP, MANACOST_X(9), 0, NULL, NULL);
}

int card_coral_helm(int player, int card, event_t event){
/* CARD_ID_CORAL_HELM	486
Coral Helm	|3
Artifact
|3, Discard a card at random: Target creature gets +2/+2 until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
	}

	return generic_activated_ability(player, card, event, GAA_DISCARD_RANDOM | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_crumble(int player, int card, event_t event){
/* CARD_ID_CRUMBLE	487
Crumble	|G
Instant
Destroy target artifact. It can't be regenerated. That artifact's controller gains life equal to its converted mana cost. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_cmc(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			gain_life(instance->targets[0].player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ARTIFACT", 1, NULL);
}

int card_cursed_rack(int player, int card, event_t event){
/* CARD_ID_CURSED_RACK	488
Cursed Rack	|4
Artifact
As ~ enters the battlefield, choose an opponent.
The chosen player's maximum hand size is four. */

	if( event == EVENT_MAX_HAND_SIZE && current_turn == 1-player && ! is_humiliated(player, card, AL_STATIC) ){
		event_result = 4;
	}
	return 0;
}

int card_damping_field(int player, int card, event_t event){
/* CARD_ID_DAMPING_FIELD	489
Damping Field	|2|W
Enchantment
Players can't untap more than one artifact during their untap steps. */

	untap_only_one_permanent_type(player, card, event, ANYBODY, TYPE_ARTIFACT);
	return global_enchantment(player, card, event);
}

int card_detonate(int player, int card, event_t event){
/* CARD_ID_DETONATE	490
Detonate	|X|R
Sorcery
Destroy target artifact with converted mana cost X. It can't be regenerated. ~ deals X damage to that artifact's controller. */

	return card_detonate_exe(player, card, event);
}

int card_drafnas_restoration(int player, int card, event_t event)
{
  /* Drafna's Restoration	|U
   * Sorcery
   * Return any number of target artifact cards from target player's graveyard to the top of his or her library in any order. */

  // 0x41F560



  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;

  if (event == EVENT_CAN_CAST)
	return can_target(&td);

  // Should target the artifacts here, too, but the exe doesn't and I can't be bothered to either.
  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	pick_target(&td, "TARGET_PLAYER");

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (valid_target(&td) && !graveyard_has_shroud(instance->targets[0].player))
		{
		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_ARTIFACT, "Select target artifact card.");

		  int selected;
		  do
			{
			  selected = new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_DECK, 0, AI_MAX_VALUE, &test);
			} while (selected >= 0 && !IS_AI(player));	// AI puts best artifact on top of own deck.
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_dragon_engine(int player, int card, event_t event){
/* CARD_ID_DRAGON_ENGINE	492
Dragon Engine	|3
Artifact Creature - Construct 1/3
|2: ~ gets +1/+0 until end of turn. */

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(2), 1, 0);
}

int card_dwarven_weaponsmith(int player, int card, event_t event)
{
  /* CARD_ID_DWARVEN_WEAPONSMITH	493
   * Dwarven Weaponsmith	|1|R
   * Creature - Dwarf Artificer 1/1
   * |T, Sacrifice an artifact: Put a +1/+1 counter on target creature. Activate this ability only during your upkeep. */

  card_instance_t *instance;
  unsigned int sacrifice_choice;
  int result;
  unsigned int player_00;
  target_definition_t td;
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x18005, 0, 0, 0, 0, 0, 0,
                                       0, &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      new_default_test_definition(&test, 0x40, "Select an artifact to sacrifice.");
      sacrifice_choice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (sacrifice_choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance = get_card_instance(player, card);
      tapped_for_mana_color = -1;
      instance->state = instance->state | 0x10;
      result = pick_target(&td, "TARGET_CREATURE");
      player_00 = (int)sacrifice_choice >> 0x10 & 0xff;
      if (result != 0)
      {
        kill_card(player_00, sacrifice_choice >> 0x18, KILL_SACRIFICE);
        return 0;
      }
      state_untargettable(player_00, sacrifice_choice >> 0x18, 0);
      instance = get_card_instance(player, card);
      instance->state = instance->state & 0xffffffef;
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
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                 COUNTER_P1_P1, 1);
  }
  return 0;
}

int card_energy_flux(int player, int card, event_t event)
{
  /* CARD_ID_ENERGY_FLUX	494
   * Energy Flux	|2|U
   * Enchantment
   * All artifacts have "At the beginning of your upkeep, sacrifice this artifact unless you pay |2." */

  int upkeeps_or_artifact_count;
  int ai_artifact_or_count;
  card_instance_t *instance;
  int payment_or_result;
  int card_00;
  bool recheck_source = true;
  test_definition_t this_test;

  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if ((player != 1) || (affected_card_controller != 1))
    {
      if (trigger_condition != TRIGGER_UPKEEP)
      {
        return 0;
      }
      recheck_source = false;
    }
    else
    {
      upkeeps_or_artifact_count = count_subtype(0, 0x40, -1);
      ai_artifact_or_count = count_subtype(1, 0x40, -1);
      ai_modifier = ai_modifier + (upkeeps_or_artifact_count - ai_artifact_or_count) * 5;
    }
  }
  if (trigger_condition != TRIGGER_UPKEEP)
  {
    return 0;
  }
  if (recheck_source && card != affected_card)
  {
    return 0;
  }
  if ((((player == affected_card_controller) &&
        (upkeeps_or_artifact_count = is_humiliated(player, card, AL_STATIC),
         upkeeps_or_artifact_count == 0)) &&
       (reason_for_trigger_controller == player)) &&
      ((upkeeps_or_artifact_count = count_upkeeps(player),
        0 < upkeeps_or_artifact_count &&
            (ai_artifact_or_count = count_subtype(current_turn, 0x40, -1),
             0 < ai_artifact_or_count))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      ai_artifact_or_count = -1;
      default_test_definition(&this_test, 0x40);
      do
      {
        card_00 = active_cards_count[current_turn] + -1;
        if (-1 < card_00)
        {
          do
          {
            instance = in_play(current_turn, card_00);
            if (((instance != NULL) && (payment_or_result = is_what(current_turn, card_00, 0x40),
                                        payment_or_result != 0)) &&
                (payment_or_result = is_humiliated(current_turn, card_00, AL_TRIG),
                 payment_or_result == 0))
            {
              add_state(current_turn, card_00, 0x200000);
              bool upkeep_paid = false;
              payment_or_result = has_mana(current_turn, 0, 2);
              if (payment_or_result != 0)
              {
                if (current_turn == 1 && ai_artifact_or_count == -1)
                  ai_artifact_or_count =
                      check_battlefield_for_special_card(player, card, 1, 1, &this_test);
                bool should_pay = false;
                if (current_turn == 0)
                {
                  payment_or_result =
                      do_dialog(0, 0, card_00, -1, -1, " Pay upkeep\n Do not pay", 0);
                  should_pay = payment_or_result == 0;
                }
                else if (ai_artifact_or_count == card_00 && current_turn == 1)
                  should_pay = true;
                if (should_pay)
                {
                  charge_mana(current_turn, COLOR_COLORLESS, 2);
                  if (spell_fizzled != 1)
                  {
                    remove_state(current_turn, card_00, 0x200000);
                    ai_artifact_or_count = -1;
                    upkeep_paid = true;
                  }
                }
              }
              if (!upkeep_paid)
              {
                remove_state(current_turn, card_00, 0x200000);
                kill_card(current_turn, card_00, KILL_SACRIFICE);
              }
            }
            card_00 = card_00 + -1;
          } while (card_00 != -1);
        }
        upkeeps_or_artifact_count = upkeeps_or_artifact_count + -1;
      } while (upkeeps_or_artifact_count != 0);
    }
  }
  return 0;
}

int card_feldons_cane(int player, int card, event_t event)
{
  /* Feldon's Cane	|1
   * Artifact
   * |T, Exile ~: Shuffle your graveyard into your library. */

  // 0x420E70



	if ( event == EVENT_RESOLVE_ACTIVATION ){
		reshuffle_grave_into_deck(player, 0);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_RFG_ME, MANACOST0, 0, NULL, NULL);
}

int card_gaeas_avenger(int player, int card, event_t event)
{
  /* CARD_ID_GAEAS_AVENGER	496
   * Gaea's Avenger	|1|G|G
   * Creature - Treefolk 101/101
   * ~'s power and toughness are each equal to 1 plus the number of artifacts your opponents control. */

  int result;
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    result = count_subtype(1 - player, 0x40, -1);
    set_characteristic_pt(result + 1);
  }
  return 0;
}

int card_gate_to_phyrexia(int player, int card, event_t event){
/* CARD_ID_GATE_TO_PHYREXIA	497
Gate to Phyrexia	|B|B
Enchantment
Sacrifice a creature: Destroy target artifact. Activate this ability only during your upkeep and only once each turn. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		ai_modifier+=5*count_subtype(1-player, TYPE_ARTIFACT, -1);
	}

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET | GAA_ONCE_PER_TURN | GAA_ONLY_ON_UPKEEP | GAA_IN_YOUR_TURN,
									MANACOST0, 0, &td, "TARGET_ARTIFACT");
}

int card_goblin_artisans(int player, int card, event_t event){
/* CARD_ID_GOBLIN_ARTISANS	498
Goblin Artisans	|R
Creature - Goblin Artificer 1/1
|T: Flip a coin. If you win the flip, draw a card. If you lose the flip, counter target artifact spell you control
that isn't the target of an ability from another creature named ~. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			if( counterspell(player, card, EVENT_CAN_CAST, &td, 0) ){
				if( ! check_special_flags2(card_on_stack_controller, card_on_stack, SF2_GOBLIN_ARTISANS) ){
					return 99;
				}
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		ai_modifier+=5*(hand_count[player]-7);
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			instance->targets[0].player = card_on_stack_controller;
			instance->targets[0].card = card_on_stack;
			instance->number_of_targets = 1;
			set_special_flags2(card_on_stack_controller, card_on_stack, SF2_GOBLIN_ARTISANS);
			tap_card(player, card);
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td, 0) ){
			if( flip_a_coin(instance->parent_controller, instance->parent_card) ){
				draw_cards(instance->parent_controller, 1);
			}
			else{
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
			}
		}
	}
	return 0;
}

int card_golgothian_sylex(int player, int card, event_t event){
/* CARD_ID_GOLGOTHIAN_SYLEX	499
Golgothian Sylex	|4
Artifact
|1, |T: Each nontoken permanent with a name originally printed in the Antiquities expansion is sacrificed by its controller. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		APNAP(p,{
					int count = active_cards_count[p]-1;
					while( count > -1 ){
							if( in_play(p, count) && is_what(p, count, TYPE_PERMANENT) && !is_token(p, count)){
								int csvid = get_id(p, count);
								if( csvid >= CARD_ID_AMULET_OF_KROOG && csvid < CARD_ID_YOTIAN_SOLDIER ){
									kill_card(p, count, KILL_SACRIFICE);
								}
							}
							count--;
					}
				}
		);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
}

int card_grapeshot_catapult(int player, int card, event_t event){
/* CARD_ID_GRAPESHOT_CATAPULT	500
Grapeshot Catapult	|4
Artifact Creature - Construct 2/3
|T: ~ deals 1 damage to target creature with flying. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		damage_target0(player, card, 1);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature with flying.");
}

int card_haunting_wind(int player, int card, event_t event){
/* CARD_ID_HAUNTING_WIND	501
Haunting Wind	|3|B
Enchantment
Whenever an artifact becomes tapped or a player activates an artifact's ability without |T in its activation cost, ~ deals 1 damage to that artifact's controller. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		ai_modifier+=5*(count_subtype(1-player, TYPE_ARTIFACT, -1)-count_subtype(player, TYPE_ARTIFACT, -1));
	}

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_PLAY_ABILITY && is_what(affected_card_controller, affected_card, TYPE_ARTIFACT)){
			damage_player(affected_card_controller, 1, player, card);
		}

		if( event == EVENT_TAP_CARD && is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
			damage_player(affected_card_controller, 1, player, card);
		}
	}

	return 0;
}

int card_hurkyls_recall(int player, int card, event_t event){
/* CARD_ID_HURKYLS_RECALL	502
Hurkyl's Recall	|1|U
Instant
Return all artifacts target player owns to his or her hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ARTIFACT);
			new_manipulate_all(player, card, get_card_instance(player, card)->targets[0].player, &this_test, ACT_BOUNCE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_ivory_tower(int player, int card, event_t event){
/* CARD_ID_IVORY_TOWER	503
Ivory Tower	|1
Artifact
At the beginning of your upkeep, you gain X life, where X is the number of cards in your hand minus 4. */

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		upkeep_trigger_ability_mode(player, card, event, player, hand_count[player] >= 5 ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int amount = hand_count[player]-4;
		if( amount ){
			gain_life(player, amount);
		}
	}

	return 0;
}

int card_jalum_tome(int player, int card, event_t event){
/* CARD_ID_JALUM_TOME	504
Jalum Tome	|3
Artifact
|2, |T: Draw a card, then discard a card. */

	if (event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
		discard(player, 0, player);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_martyrs_of_korlis(int player, int card, event_t event ){
  /* CARD_ID_MARTYRS_OF_KORLIS	505
   * Martyrs of Korlis	|3|W|W
   * Creature - Human 1/6
   * As long as ~ is untapped, all damage that would be dealt to you by artifacts is dealt to ~ instead. */

	// original code : 0x453EB0


	if( event == EVENT_PREVENT_DAMAGE && ! is_tapped(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == -1 && damage->damage_target_player == player &&
				!damage_is_to_planeswalker(damage) && damage->info_slot > 0
			  ){
				if( damage_from_artifact_source(-1, affected_card_controller, affected_card, -1, -1) ){
					damage->damage_target_card = card;
				}
			}
		}
	}

	return 0;
}

int card_mightstone(int player, int card, event_t event ){
/* CARD_ID_MIGHTSTONE	506
Mightstone	|4
Artifact
Attacking creatures get +1/+0. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_POWER && is_attacking(affected_card_controller, affected_card) ){
			event_result++;
		}
	}
	return 0;
}

int card_millstone(int player, int card, event_t event){
/* CARD_ID_MILLSTONE	507
Millstone	|2
Artifact
|2, |T: Target player puts the top two cards of his or her library into his or her graveyard. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			mill( instance->targets[0].player, 2);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0, &td1, "TARGET_PLAYER");
}

int card_mishras_factory(int player, int card, event_t event){
  /* CARD_ID_MISHRAS_FACTORY	508
   * Mishra's Factory	""
   * Land
   * |T: Add |C to your mana pool.
   * |1: ~ becomes a 2/2 Assembly-Worker artifact creature until end of turn. It's still a land.
   * |T: Target Assembly-Worker creature gets +1/+1 until end of turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_TRANSFORM = 2,
    CHOICE_PUMP = 3
  };



    if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player && CAN_TAP(player, card) &&
        can_use_activated_abilities(player, card) && has_mana_for_activated_ability(player, card, MANACOST0) &&
        current_turn == 1-player && has_subtype(affected_card_controller, affected_card, SUBTYPE_ASSEMBLY_WORKER)){
        ++check_abilities_power;
        ++check_abilities_toughness;
    }
    if (event == EVENT_COUNT_MANA && affect_me(player, card)){
        return mana_producer(player, card, event);
    }
    has_subtype_if_animated_self(player, card, event, SUBTYPE_ASSEMBLY_WORKER);
    if (!IS_GAA_EVENT(event)){ return 0; }
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = player;
    td.required_subtype = SUBTYPE_ASSEMBLY_WORKER;
    card_instance_t *instance = get_card_instance(player, card);
    int can_pump = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
        GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0, &td, NULL);
    int pump_priority = current_phase == PHASE_AFTER_BLOCKING ? 15 : 1;
    int paying_for_pump = paying_mana();
    int status = instance->token_status;
    int paying_for_animation = paying_mana();
    int mana_priority = paying_mana();
    int can_mana = mana_producer(player, card, EVENT_CAN_ACTIVATE);
    int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, DLG_NO_DISPLAY_FOR_AI,
        "Produce mana", can_mana, mana_priority,
        "Become an Assembly Worker", !paying_for_animation, !(status & STATUS_LEGACY_TYPECHANGE), DLG_MANA(MANACOST_X(1)),
        "Pump an Assembly Worker", can_pump && !paying_for_pump, pump_priority);
    if (event == EVENT_CAN_ACTIVATE){ return choice; }
    if (event == EVENT_ACTIVATE){
        if (!choice){ return 0; }
        if (choice == CHOICE_MANA){ return mana_producer(player, card, event); }
        if (choice == CHOICE_PUMP){
            if (charge_mana_for_activated_ability(player, card, MANACOST0)){
                if (!new_pick_target(&td, "Select an Assembly Worker to pump", 0, 1 | GS_LITERAL_PROMPT)){
                    spell_fizzled = 1;
                    remove_state(player, card, STATE_TAPPED);
                    return 0;
                }
                add_state(player, card, STATE_TAPPED);
                tapped_for_mana_color = -1;
            }
        }
    }
    if (event == EVENT_RESOLVE_ACTIVATION){
        if (choice == CHOICE_TRANSFORM){
            alternate_legacy_text(2, player, animate_self_to_artifact_creature(player, card, 2, 2, 0, 0, 0, 0));
            add_status(instance->parent_controller, instance->parent_card, STATUS_ANIMATED);
        }
        if (choice == CHOICE_PUMP && valid_target(&td)){
            alternate_legacy_text(1, player, pump_until_eot(instance->parent_controller, instance->parent_card,
                instance->targets[0].player, instance->targets[0].card, 1, 1));
        }
    }
    return 0;
}

int card_assembly_worker(int player, int card, event_t event){
/* CARD_ID_ASSEMBLY_WORKER	910
Assembly-Worker	""(token)
Artifact Creature Land - Assembly-Worker 2/2
|T: Add |C to your mana pool.
|T: Target ~ creature gets +1/+1 until end of turn.
|1: Until end of turn, Mishra's Factory becomes a 2/2 ~ artifact creature. It's still a land. */

	return 0;
}

int card_mishras_war_machine(int player, int card, event_t event){
/* CARD_ID_MISHRAS_WAR_MACHINE	509
Mishra's War Machine	|7
Artifact Creature - Juggernaut 5/5
Banding (Any creatures with banding, and up to one without, can attack in a band. Bands are blocked as a group.
If any creatures with banding you control are blocking or being blocked by a creature, you divide that creature's combat damage, not its controller,
among any of the creatures it's being blocked by or is blocking.)
At the beginning of your upkeep, ~ deals 3 damage to you unless you discard a card. If ~ deals damage to you this way, tap it. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		if( hand_count[player] > 0 ){
			int ai_choice = 0;
			if( (! can_attack(player, card) && life[player] > 5) || hand_count[player] < count_upkeeps(player)	){
				ai_choice = 1;
			}
			int choice = do_dialog(player, player, card, -1, -1, " Pay upkeep\n Pass", ai_choice);
			if( choice == 0 ){
				discard(player, 0, player);
				kill--;
			}
		}
		if( kill == 1 ){
			damage_player(player, 3, player, card);
			tap_card(player, card);
		}
	}
	return 0;
}

int card_mishras_workshop(int player, int card, event_t event)
{
/* CARD_ID_MISHRAS_WORKSHOP	510
Mishra's Workshop	""
Land
|T: Add |C|C|C to your mana pool. Spend this mana only to cast artifact spells. */

  //0x47b5e0
  if (event == EVENT_COUNT_MANA && affect_me(player, card)
	  && !is_tapped(player, card) && !is_animated_and_sick(player, card) && can_produce_mana(player, card))
	declare_mana_available(player, COLOR_ARTIFACT, 3);

  if (event == EVENT_CAN_ACTIVATE)
	return !is_tapped(player, card) && !is_animated_and_sick(player, card) && can_produce_mana(player, card);

  if (event == EVENT_ACTIVATE)
	produce_mana_tapped(player, card, COLOR_ARTIFACT, 3);

  if (event == EVENT_RESOLVE_SPELL)
	play_land_sound_effect(player, card);

  if (event == EVENT_QUERY){ return attacking_card == QUERY_IS_PLAIN_MANA_PRODUCER; }

  return 0;
}

static const char* permanent_you_control_and_own(int who_chooses, int player, int card, int unused_source_player, int unused_source_card){
	if( who_chooses == AI ){
		if( check_state(player, card, STATE_OWNED_BY_OPPONENT) ){
			return NULL;
		}
	}
	if( who_chooses == HUMAN ){
		if( ! check_state(player, card, STATE_OWNED_BY_OPPONENT) ){
			return NULL;
		}
	}

	return "must be a permanent you both own and control";
}

int card_obelisk_of_undoing(int player, int card, event_t event){
/* CARD_ID_OBELISK_OF_UNDOING	511
Obelisk of Undoing	|1
Artifact
|6, |T: Return target permanent you both own and control to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT );
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td1.extra = target_predicate_extra(permanent_you_control_and_own);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			bounce_permanent( instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_CAN_TARGET, MANACOST_X(6), 0, &td1, "TARGET_PERMANENT");
}

int card_onulet(int player, int card, event_t event){
/* CARD_ID_ONULET	512
Onulet	|3
Artifact Creature - Construct 2/2
When ~ dies, you gain 2 life. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		gain_life(player, 2);
	}

	return 0;
}

int card_orcish_mechanic(int player, int card, event_t event){
	/* Orcish Mechanics	|2|R	0x200c302
	 * Creature - Orc 1/1
	 * |T, Sacrifice an artifact: ~ deals 2 damage to any target. */

	if (!IS_GAA_EVENT(event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){ damage_target0(player, card, 2); }
	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ANY", TYPE_ARTIFACT, NULL, SARV_0);
}

// Ornithopter --> vanilla

int card_phyrexian_gremlins(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_GRIMOIRE	3228
Phyrexian Grimoire	|3
Artifact
|4, |T: Target opponent chooses one of the top two cards of your graveyard. Exile that card and put the other one into your hand. */
	// original code : 0x4549D0

	choose_to_untap(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *parent = get_card_instance( instance->parent_controller, instance->parent_card );
			parent->targets[1] = instance->targets[0];
			does_not_untap_until_im_tapped(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card);
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_ARTIFACT");
}

static int is_monolith(int id){
	if( id == CARD_ID_MANA_VAULT ||
		id == CARD_ID_GRIM_MONOLITH ||
		id == CARD_ID_BASALT_MONOLITH
	  ){
		return 1;
	}
	return 0;
}

int card_power_artifact(int player, int card, event_t event)
{
  /* CARD_ID_POWER_ARTIFACT	517
   * Power Artifact	|U|U
   * Enchantment - Aura
   * Enchant artifact
   * Enchanted artifact's activated abilities cost |2 less to activate.
   * This effect can't reduce the amount of mana an ability costs to activate to less than one mana. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  instance = in_play(player, card);
  if (instance != NULL &&
      (instance = get_card_instance(player, card), instance->damage_target_player != -1))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_STATIC_EFFECTS)
    {
      if (card == affected_card && player == affected_card_controller &&
          (result =
               is_what((int)instance->damage_target_player, instance->damage_target_card, 0x40),
           result == 0))
        kill_card(player, card, KILL_SACRIFICE);
      leaves_play(player, card, EVENT_STATIC_EFFECTS, 0, 0);
    }
    else
    {
      leaves_play(player, card, event, 0, 0);
      if (event == EVENT_LEAVES_PLAY_ABILITY)
        remove_cost_mod_for_activated_abilities((int)instance->damage_target_player,
                                                instance->damage_target_card, 2, NULL);
    }
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x40);
    td.preferred_controller = player;
    get_card_instance(player, card);
    return targeted_aura(player, card, EVENT_CAN_CAST, &td, "TARGET_ARTIFACT");
  }
  if (event == EVENT_CAST_SPELL)
  {
    if (card != affected_card || player != affected_card_controller)
      return 0;
    default_target_definition(player, card, &td, 0x40);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (card != affected_card || player != affected_card_controller)
      return 0;
    targeted_aura(player, card, EVENT_CAST_SPELL, &td, "TARGET_ARTIFACT");
    if (spell_fizzled == 1)
    {
      return 0;
    }
    result = instance->targets[0].card;
    if (result == 1)
    {
      return 0;
    }
    result = get_id(instance->targets[0].player, result);
    if ((result != 0x9d && result != 0x5e5) && (result != 0xb))
    {
      ai_modifier = ai_modifier + -0x19;
      return 0;
    }
    ai_modifier = ai_modifier + 0x32;
    return 0;
  }
  if (event == EVENT_CHANGE_TARGET)
  {
    default_target_definition(player, card, &td, 0x40);
    td.preferred_controller = player;
    get_card_instance(player, card);
    return 0;
  }
  if (event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 0x40);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result == 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
      return 0;
    }
  }
  else
  {
    if (event == EVENT_CAN_MOVE_AURA)
    {
      state_untargettable((int)instance->damage_target_player, instance->damage_target_card, 1);
      result = can_target(&td);
      if (result == 0)
      {
        state_untargettable((int)instance->damage_target_player, instance->damage_target_card, 0);
        return 0;
      }
      return 1;
    }
    if (event == EVENT_MOVE_AURA)
    {
      state_untargettable((int)instance->damage_target_player, instance->damage_target_card, 1);
      result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {
        state_untargettable((int)instance->damage_target_player, instance->damage_target_card, 0);
        return 0;
      }
      state_untargettable((int)instance->damage_target_player, instance->damage_target_card, 0);
      return 0;
    }
    if (event != EVENT_RESOLVE_MOVING_AURA)
    {
      return 0;
    }
    result = validate_target(player, card, &td, 0);
    if (result == 0)
    {
      return 0;
    }
    remove_cost_mod_for_activated_abilities((int)instance->damage_target_player,
                                            instance->damage_target_card, 2, NULL);
  }
  attach_aura_to_target(player, card, EVENT_RESOLVE_SPELL, instance->targets[0].player,
                        instance->targets[0].card);
  set_cost_mod_for_activated_abilities(instance->targets[0].player, instance->targets[0].card, 2,
                                       NULL);
  return 0;
}

int card_powerleech(int player, int card, event_t event){
/* CARD_ID_POWERLEECH	518
Powerleech	|G|G
Enchantment
Whenever an artifact an opponent controls becomes tapped or an opponent activates an artifact's ability without |T in its activation cost,
you gain 1 life.*/
	// original code : 0x4327D0

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		ai_modifier+=5*count_subtype(1-player, TYPE_ARTIFACT, -1);
	}

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( event == EVENT_PLAY_ABILITY && affected_card_controller == 1-player &&
			is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) &&
			! is_tapped(affected_card_controller, affected_card) )
		{
			gain_life(player, 1);
		}

		if( event == EVENT_TAP_CARD && affected_card_controller == 1-player &&
			is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) )
		{
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_priest_of_yawgmoth(int player, int card, event_t event){
  /* Priest of Yawgmoth |1|B
   * Creature — Human Cleric 1/2
   * {T}, Sacrifice an artifact: Add to your mana pool an amount of {B} equal to the sacrificed artifact's converted mana cost. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			return can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT);
		}
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if (!sac){
				cancel = 1;
				return 0;
			}
			tap_card(player, card);
			int cmc = get_cmc(BYTE2(sac), BYTE3(sac));
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			produce_mana(player, COLOR_BLACK, cmc);
		}
	}

    if (event == EVENT_COUNT_MANA && affect_me(player, card) && !is_tapped(player, card)
        && !is_animated_and_sick(player, card) && can_produce_mana(player, card)){
        int max_cmc = 0;
        for (int c = 0; c < active_cards_count[player]; ++c){
            if (in_play(player, c) && is_what(player, c, TYPE_ARTIFACT) && can_sacrifice_this_as_cost(player, c)){
                int cmc = get_cmc(player, c);
                if (cmc > max_cmc){ max_cmc = cmc; }
            }
        }
        declare_mana_available(player, COLOR_BLACK, max_cmc);
    }
	return 0;
}

int card_primal_clay(int player, int card, event_t event)
{
  /* Primal Clay	|4
   * Artifact Creature - Shapeshifter 100/100
   * As ~ enters the battlefield, it becomes your choice of a 3/3 artifact creature, a 2/2 artifact creature with flying, or a 1/6 Wall artifact creature with
   * defender in addition to its other types. */

  //0x4210a0

  /* Primal Clay	|4
   * Artifact Creature - Shapeshifter 100/100
   * As ~ enters the battlefield, it becomes your choice of a 3/3 artifact creature, a 2/2 artifact creature with flying, or a 1/6 Wall artifact creature with
   * defender in addition to its other types. */

  // Also Primal Plasma, which doesn't add subtype Wall.
  /* Primal Plasma	|3|U
   * Creature - Elemental Shapeshifter 100/100
   * As ~ enters the battlefield, it becomes your choice of a 3/3 creature, a 2/2 creature with flying, or a 1/6 creature with defender. */

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  instance->state |= STATE_OUBLIETTED;

	  int new_iid = create_a_card_type(instance->internal_card_id);
	  card_data_t* cd = &cards_data[new_iid];

	  char prompt[1207];
	  if (ai_is_speculating != 1)
		{
		  load_text(0, "PRIMAL_CLAY");
		  sprintf(prompt, "%s\n %s\n %s\n %s", text_lines[0], text_lines[get_id(player, card) == CARD_ID_PRIMAL_CLAY ? 1 : 4], text_lines[2], text_lines[3]);
		}

	  switch (do_dialog(player, player, card, -1, -1, prompt, 1))
		{
		  case 0:
			cd->power = 1;
			cd->toughness = 6;
			cd->static_ability = KEYWORD_DEFENDER;
			if (get_id(player, card) == CARD_ID_PRIMAL_CLAY)
			  add_a_subtype_permanently(player, card, player, card, SUBTYPE_WALL);
			break;

		  case 1:
			cd->power = cd->toughness = 2;
			cd->static_ability = KEYWORD_FLYING;
			break;

		  case 2:
			cd->power = cd->toughness = 3;
			cd->static_ability = 0;
			break;
		}

	  instance->state &= ~STATE_OUBLIETTED;

	  instance->backup_internal_card_id = instance->dummy3 = instance->internal_card_id = new_iid;
	  instance->regen_status |= KEYWORD_RECALC_ABILITIES|KEYWORD_RECALC_POWER|KEYWORD_RECALC_TOUGHNESS|KEYWORD_RECALC_CHANGE_TYPE;
	}

  if (event == EVENT_CHANGE_TYPE
	  && !(land_can_be_played & LCBP_DURING_EVENT_CHANGE_TYPE_SECOND_PASS)
	  && affect_me(player, card)
	  && (in_play(player, card) || (instance->state & STATE_INVISIBLE)))
	event_result = instance->dummy3;

  return 0;
}

int card_rakalite(int player, int card, event_t event)
{
  /* Rakalite	|6	0x200c30c
   * Artifact
   * |2: Prevent the next 1 damage that would be dealt to any target this turn. Return ~ to its owner's hand at the beginning of the next end step. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if ((0 < instance->info_slot) && (result = eot_trigger(player, card, event), result != 0))
  {
    bounce_permanent(player, card);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0xff);
    td.extra = damage_card;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x1000004, 2, 0, 0, 0, 0,
                                         0, 0, &td, "TARGET_DAMAGE");
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x1000004, 2, 0, 0, 0, 0, 0, 0, &td,
                                "TARGET_DAMAGE");
      if (spell_fizzled == 1)
      {
        return 0;
      }
      instance->info_slot = instance->info_slot + 1;
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0xff);
    td.extra = damage_card;
    td.illegal_abilities = 0;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
    return 0;
  }
  return 0;
}

int card_reconstruction(int player, int card, event_t event){
/* CARD_ID_RECONSTRUCTION	521
Reconstruction	|U
Sorcery
Return target artifact card from your graveyard to your hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	char msg[100] = "Select an Artifact card.";
	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ARTIFACT, msg);

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

int card_reverse_polarity(int player, int card, event_t event)
{
/* CARD_ID_REVERSE_POLARITY	522
Reverse Polarity	|W|W
Instant
You gain X life, where X is twice the damage dealt to you so far this turn by artifacts. */
  // 0x475260

  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  gain_life(player, 2 * get_trap_condition(player, TRAP_ARTIFACT_DAMAGE_TAKEN));
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_rocket_launcher(int player, int card, event_t event){
	/* Rocket Launcher	|4	0x200c316
	 * Artifact
	 * |2: ~ deals 1 damage to any target. Destroy ~ at the beginning of the next end step. Activate this ability only if you've controlled ~ continuously since the beginning of your most recent turn. */
if( get_card_instance(player, card)->info_slot > 0 && eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		set_special_flags(player, card, SF_CORRECTLY_RESOLVED);
	}

	if( event == EVENT_CLEANUP ){
		remove_special_flags(player, card, SF_CORRECTLY_RESOLVED);
	}

	if( !IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && ! check_special_flags(player, card, SF_CORRECTLY_RESOLVED) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_X(2), 0, &td, "TARGET_ANY");
		if( spell_fizzled != 1 ){
			instance->info_slot++;
		}
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		damage_target0(player, card, 1);
	}

	return 0;
}

int card_sage_of_lat_nam(int player, int card, event_t event){
/* CARD_ID_SAGE_OF_LAT_NAM	524
Sage of Lat-Nam	|1|U
Creature - Human Artificer 1/2
|T, Sacrifice an artifact: Draw a card. */

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ){
			return can_sacrifice_type_as_cost(player, 1, TYPE_ARTIFACT);
		}
	}
	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an artifact to sacrifice.");
			if( new_sacrifice(player, card, player, SAC_AS_COST, &this_test) ){
				tap_card(player, card);
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}
	return 0;
}

int card_shapeshifter2(int player, int card, event_t event)
{
  /* Shapeshifter	|6
   * Artifact Creature - Shapeshifter 100/207
   * As ~ enters the battlefield, choose a number between 0 and 7.
   * At the beginning of your upkeep, you may choose a number between 0 and 7.
   * ~'s power is equal to the last chosen number and its toughness is equal to 7 minus that number. */

  card_instance_t *instance;
  unsigned int chosen_power;
  int32_t ai_toughness;
  int characteristic_or_result;
  int32_t ai_power;
  test_definition_t this_test;

  if (card == -1)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_POWER || event == EVENT_TOUGHNESS)
  {
    if (card == affected_card && player == affected_card_controller)
    {
      if (event == EVENT_POWER)
        characteristic_or_result = instance->targets[1].player;
      else
        characteristic_or_result = instance->targets[1].card;
      if (characteristic_or_result >= 0)
        set_characteristic_pt(characteristic_or_result);
    }
  }
  else
  {
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_UPKEEP_TRIGGER_ABILITY))
    {
      if (player == 0)
      {
        do
        {
          chosen_power = choose_a_number(0, "Choose a number between 0 and 7 for Power", 0);
        } while (7 < chosen_power);
        instance->targets[1].player = chosen_power;
        instance->targets[1].card = 7 - chosen_power;
      }
      else
      {
        default_test_definition(&this_test, 2);
        characteristic_or_result =
            check_battlefield_for_special_card(player, card, 1 - player, 0x80, &this_test);
        ai_toughness = characteristic_or_result + 1;
        if (ai_toughness < 8)
        {
          ai_power = 7 - ai_toughness;
        }
        else
        {
          ai_power = 6;
          ai_toughness = 1;
        }
        instance->targets[1].player = ai_power;
        instance->targets[1].card = ai_toughness;
      }
    }
  }
  if (((trigger_condition == TRIGGER_UPKEEP) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    characteristic_or_result = duh_mode(player);
    upkeep_trigger_ability_mode(player, card, event, player,
                                (characteristic_or_result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  }
  return 0;
}

int card_shatterstorm(int player, int card, event_t event){
/* CARD_ID_SHATTERSTORM	526
Shatterstorm	|2|R|R
Sorcery
Destroy all artifacts. They can't be regenerated. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_BURY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_staff_of_zegon(int player, int card, event_t event){
/* CARD_ID_STAFF_OF_ZEGON	527
Staff of Zegon	|4
Artifact
|3, |T: Target creature gets -2/-0 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, -2, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_strip_mine(int player, int card, event_t event){
/* CARD_ID_STRIP_MINE	528
Strip Mine	""
Land
|T: Add |C to your mana pool.
|T, Sacrifice ~: Destroy target land. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_ACTIVATE){
		instance->info_slot = 0;
		int choice = 0;
		if( !paying_mana() && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST0, 0, &td, NULL) ){
			choice = do_dialog(player, player, card, -1, -1, " Get mana\n Destroy a land\n Cancel", 1);
		}
		if( choice == 0 ){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				add_state(player, card, STATE_TAPPED);
				if( charge_mana_for_activated_ability(player, card, MANACOST0) &&
					new_pick_target(&td, "TARGET_LAND", 0, 1)
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
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_su_chi(int player, int card, event_t event){
/* CARD_ID_SU_CHI	529
Su-Chi	|4
Artifact Creature - Construct 4/4
When ~ dies, add |C|C|C|C to your mana pool. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		produce_mana(player, COLOR_COLORLESS, 4);
	}
	return 0;
}

static int tablet_of_epityr_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_TABLET_OF_EPITYR)
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
              instance->targets[11].player = 0;
            SET_BYTE2(instance->targets[11].player) = 1;
          }
        }
        count_for_gfp_ability(player, card, event, player, TYPE_ARTIFACT, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount && has_mana(player, COLOR_ANY, 1))
    {
      charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1);
      if (spell_fizzled != 1)
      {
        gain_life(player, 1);
      }
      else
      {
        break;
      }
      amount--;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_tablet_of_epityr(int player, int card, event_t event){
  /* Tablet of Epityr |1
   * Artifact
   * Whenever an artifact you control is put into a graveyard from the battlefield, you may pay {1}. If you do, you gain 1 life. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)tablet_of_epityr_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &tablet_of_epityr_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		count_for_gfp_ability(player, card, event, player, TYPE_ARTIFACT, NULL);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			int amount = BYTE0(result);
			while( amount && has_mana(player, COLOR_ANY, 1) ){
					charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 1);
					if( spell_fizzled != 1 ){
						gain_life(player, 1);
					}
					else{
						break;
					}
					amount--;
			}
		}
	}

	return 0;
}

int card_tawnos_wand(int player, int card, event_t event){
/* CARD_ID_TAWNOSS_WAND	532
Tawnos's Wand	|4
Artifact
|2, |T: Target creature with power 2 or less can't be blocked this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.power_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(2), 0,
										&td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
									&td, "Select target creature with Power 2 or less.");
		if( spell_fizzled != 1 ){
			int key = get_abilities(instance->targets[0].player, instance->targets[0].card, EVENT_ABILITIES, -1);
			int s_key = get_special_abilities(instance->targets[0].player, instance->targets[0].card);
			int negative_key_mask = KEYWORD_FLYING | KEYWORD_DEFENDER;
			int i;
			for(i=0; i<5; i++){
				negative_key_mask |= (1<<i); //Landwalk
			}
			int positive_skey_mask = SP_KEYWORD_LIFELINK | SP_KEYWORD_VIGILANCE | SP_KEYWORD_MUST_ATTACK | SP_KEYWORD_DIE_AT_EOT;
			if( (key & KEYWORD_INFECT) || (s_key & positive_skey_mask) ){
				ai_modifier += get_power(instance->targets[0].player, instance->targets[0].card)*10;
			}
			if( (key & negative_key_mask) || (s_key && !(s_key & positive_skey_mask)) ){
				ai_modifier -= 20;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
	}

	return 0;
}

int card_tawnoss_coffin(int player, int card, event_t event){
/* CARD_ID_TAWNOSS_COFFIN	531
Tawnos's Coffin	|4
Artifact
You may choose not to untap ~ during your untap step.
|3, |T: Exile target creature and all Auras attached to it. Note the number and kind of counters that were on that creature.
When ~ leaves the battlefield or becomes untapped, return that exiled card to the battlefield under its owner's control tapped with the noted number and kind of counters
 on it. If you do, return the other exiled cards to the battlefield under their owner's control attached to that permanent. */

	// straight from "choose_to_untap", with the noted differences
	if (event == EVENT_UNTAP && affect_me(player, card)){
		card_instance_t *instance = get_card_instance(player, card);
		instance->untap_status &= ~2;
	}

	if (current_phase == PHASE_UNTAP && affect_me(player, card)){
		card_instance_t *instance = get_card_instance(player, card);
		if (event == EVENT_TRIGGER){
			if (instance->untap_status & 1){
				if (!(instance->untap_status & 2)
					&& !(cards_data[instance->internal_card_id].type & types_that_dont_untap)){
					if ((player != 1 || (trace_mode & 2)) && ai_is_speculating != 1){
						event_result |= 1;	// Human: untapping is optional
					}
					else{
						/* AI : leave tapped if CSVID contained in "instance->targets[1].card" is found in "instance->targets[1].player"'s Exile zone
						and instance->targets[1].player != player */
						if (instance->targets[1].player > -1 ){
							if( instance->targets[1].player == player ){
								instance->targets[1].player = -1;
								event_result |= 2;
							}
							else{
								if( ! check_rfg(instance->targets[1].player, instance->targets[1].card)){
									instance->targets[1].player = -1;
									event_result |= 2;
								}
							}
						}
					}
				}
			}
		}
		else if (event == EVENT_RESOLVE_TRIGGER){
				instance->untap_status |= 2;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( valid_target(&td) ){
			if( ! is_token(instance->targets[0].player, instance->targets[0].card) ){
				card_instance_t *parent = get_card_instance(player, instance->parent_card);
				parent->targets[1].player = get_owner(instance->targets[0].player, instance->targets[0].card);
				parent->targets[1].card = cards_data[get_original_id(instance->targets[0].player, instance->targets[0].card)].id;
			}
			exile_permanent_and_auras_attached(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
				EPAAC_TAWNOSS_COFFIN | EPAAC_RETURN_TO_PLAY_TAPPED );
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_tawnos_weaponry(int player, int card, event_t event){
/* CARD_ID_TAWNOSS_WEAPONRY	533
Tawnos's Weaponry	|2
Artifact
You may choose not to untap ~ during your untap step.
|2, |T: Target creature gets +1/+1 for as long as ~ remains tapped. */

	choose_to_untap(player, card, event);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 1, 1, 0, 0, PUSIIPAT_TAPPED);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_CREATURE");
}

int card_tetravite(int player, int card, event_t event){
  /* Tetravite	""
   * Artifact Creature - Tetravite 1/1
   * Flying
   * ~ can't be enchanted. */

	// original code: 00421B70



	if( get_special_infos(player, card) > 0 ){
		cannot_be_enchanted(player, card, event);
	}

	return 0;
}

static int tetra_code = 0;
static const char* is_my_tetravite(int unused, int player, int card, int unused_source_player, int unused_source_card){
	if( get_id(player, card) == CARD_ID_TETRAVITE && get_special_infos(player, card) == tetra_code ){
		return NULL;
	}
	return "this Tetravite was generated by another Tetravus.";
}

int card_tetravus(int player, int card, event_t event)
{
  /* CARD_ID_TETRAVUS	534
   * Tetravus	|6
   * Artifact Creature - Construct 1/1
   * Flying
   * ~ enters the battlefield with three +1/+1 counters on it.
   * At the beginning of your upkeep, you may remove any number of +1/+1 counters from ~. If you do, create that many 1/1 colorless Tetravite artifact creature tokens.
   * They each have flying and "This creature can't be enchanted."
   * At the beginning of your upkeep, you may exile any number of tokens created with ~. If you do, put that many +1/+1 counters on ~. */

  int i;
  int count;
  int choice;
  card_instance_t *instance;
  int release_count_or_result;
  card_instance_t *candidate;
  int tetravus_code_or_id;
  int candidate_code;
  int available_counters;
  resolve_trigger_t trigger_mode;
  int tetravite_count;

  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    set_special_infos(player, card, internal_rand(100));
    enters_the_battlefield_with_counters(player, card, EVENT_RESOLVE_SPELL, COUNTER_P1_P1, 3);
    if (duh_mode(player) == 0)
    {
      trigger_mode = RESOLVE_TRIGGER_OPTIONAL;
    }
    else
    {
      trigger_mode = RESOLVE_TRIGGER_MANDATORY;
    }
    upkeep_trigger_ability_mode(player, card, EVENT_RESOLVE_SPELL, player, trigger_mode);
    return 0;
  }
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 3);
  release_count_or_result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (release_count_or_result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  count = count_counters(player, card, COUNTER_P1_P1);
  if (!(count < 1))
  {
    choice =
        do_dialog(player, player, card, -1, -1,
                  " Release some Tetravites (Auto)\n Release some Tetravites (Manual)\n Pass", 0);
    if (choice == 0)
    {
      release_count_or_result = count_counters(player, card, COUNTER_P1_P1);
    }
    else
    {
      if (choice == 1)
      {
        count = count_counters(player, card, COUNTER_P1_P1);
        release_count_or_result = choose_a_number(player, "Remove how many counters?", count);
        available_counters = count_counters(player, card, COUNTER_P1_P1);
        if (available_counters < release_count_or_result)
          release_count_or_result = count_counters(player, card, COUNTER_P1_P1);
      }
      else
        release_count_or_result = 0;
    }
    if (0 < release_count_or_result)
    {
      i = 0;
      do
      {
        i = i + 1;
        remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
      } while (release_count_or_result != i);
      token_generation_t token;
      default_token_definition(player, card, CARD_ID_TETRAVITE, &token);
      token.qty = (unsigned char)release_count_or_result;
      token.special_infos = get_special_infos(player, card);
      generate_token(&token);
    }
  }
  release_count_or_result = 0;
  tetravite_count = 0;
  do
  {
    if (0 < active_cards_count[release_count_or_result])
    {
      i = 0;
      do
      {
        candidate = in_play(release_count_or_result, i);
        if ((candidate != NULL) && (tetravus_code_or_id = get_id(release_count_or_result, i),
                                    tetravus_code_or_id == 0x37b))
        {
          tetravus_code_or_id = get_special_infos(player, card);
          candidate_code = get_special_infos(release_count_or_result, i);
          tetravite_count = tetravite_count + (unsigned int)(tetravus_code_or_id == candidate_code);
        }
        i = i + 1;
      } while (i < active_cards_count[release_count_or_result]);
    }
    release_count_or_result = release_count_or_result + 1;
  } while (release_count_or_result != 2);
  if (0 < tetravite_count)
  {
    release_count_or_result = 2;
    if (0 < active_cards_count[player])
    {
      i = 0;
      do
      {
        candidate = in_play(player, i);
        if ((candidate != NULL) &&
            ((((tetravus_code_or_id = get_id(player, i),
                tetravus_code_or_id == 0x1e90 ||
                    (tetravus_code_or_id = get_id(player, i), tetravus_code_or_id == 0x2f09)) ||
               (tetravus_code_or_id = get_id(player, i), tetravus_code_or_id == 0x41c4)) ||
              ((tetravus_code_or_id = get_id(player, i),
                tetravus_code_or_id == 0x39ba ||
                    (tetravus_code_or_id = get_id(player, i), tetravus_code_or_id == 0x3541))))))
        {
          release_count_or_result = 0;
          break;
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
    }
    release_count_or_result =
        do_dialog(player, player, card, -1, -1,
                  " Dock some Tetravites (Auto)\n Dock some Tetravites (Manual)\n Pass",
                  release_count_or_result);
    if (release_count_or_result == 0)
    {
      release_count_or_result = 0;
      do
      {
        i = 0;
        if (0 < active_cards_count[release_count_or_result])
        {
          do
          {
            instance = in_play(release_count_or_result, i);
            if ((instance != NULL) && (tetravus_code_or_id = get_id(release_count_or_result, i),
                                       tetravus_code_or_id == 0x37b))
            {
              tetravus_code_or_id = get_special_infos(player, card);
              candidate_code = get_special_infos(release_count_or_result, i);
              if (tetravus_code_or_id == candidate_code)
              {
                kill_card(release_count_or_result, i, KILL_SACRIFICE);
                add_counters(player, card, player, card, COUNTER_P1_P1, 1);
              }
            }
            i = i + 1;
          } while (i < active_cards_count[release_count_or_result]);
        }
        release_count_or_result = release_count_or_result + 1;
      } while (release_count_or_result != 2);
    }
    else if (release_count_or_result == 1)
    {
      target_definition_t td;
      default_target_definition(player, card, &td, 2);
      td.illegal_abilities = 0;
      td.extra = target_predicate_extra(is_my_tetravite);
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      tetra_code = get_special_infos(player, card);
      while (
          (release_count_or_result = can_target(&td),
           release_count_or_result != 0 && (release_count_or_result = pick_target(&td, "TETRAVITE"),
                                            release_count_or_result != 0)))
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
        add_counters(player, card, player, card, COUNTER_P1_P1, 1);
      }
    }
  }
  return 0;
}

int card_the_rack(int player, int card, event_t event){
  /* The Rack	|1
   * Artifact
   * As this artifact enters, choose an opponent.
   * At the beginning of the chosen player's upkeep, this artifact deals X damage to that player, where X is 3 minus the number of cards in their hand. */

	// 0x420650
	/*
	  The Rack |1
	  Artifact
	  As The Rack enters the battlefield, choose an opponent.
	  At the beginning of the chosen player's upkeep, The Rack deals X damage to that player,
		where X is 3 minus the number of cards in his or her hand.
	*/
	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		ai_modifier+=(5*(7-hand_count[1-player]));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->targets[0].player = 1-player;
	}

	if( get_card_instance(player, card)->targets[0].player > -1 ){
		upkeep_trigger_ability(player, card, event, get_card_instance(player, card)->targets[0].player);

		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			int af_player = get_card_instance(player, card)->targets[0].player;
			int amount = 3-hand_count[af_player];
			if( amount > 0 ){
				damage_player(af_player, amount, player, card);
			}
		}
	}

	return 0;
}

static int titanias_song_legacy(int player, int card, event_t event)
{

  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE)
  {
    test_definition_t this_test;
    default_test_definition(&this_test, TYPE_ARTIFACT);
    this_test.type_flag = F1_NO_CREATURE;

    global_type_change(player, card, event, 2, TYPE_CREATURE, &this_test, 16384, 16384, 0, 0, 0);
  }

  if (event == EVENT_ABILITIES && is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) &&
      !is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
      !is_what(affected_card_controller, affected_card, TYPE_EFFECT))
  {
    humiliate(player, card, affected_card_controller, affected_card, 1);
  }

  if (instance->targets[0].player > -1 &&
      !in_play(instance->targets[0].player, instance->targets[0].card))
  {
    if (instance->token_status & STATUS_INVISIBLE_FX)
    {
      remove_status(player, card, STATUS_INVISIBLE_FX);
    }
    if (eot_trigger(player, card, event))
    {
      test_definition_t this_test;
      default_test_definition(&this_test, TYPE_ARTIFACT);
      kill_card(player, card, KILL_EXILE);
    }
  }

  return 0;
}

int card_titanias_song(int player, int card, event_t event)
{
  /* CARD_ID_TITANIAS_SONG	536
  Titania's Song	|3|G
  Enchantment
  Each noncreature artifact loses all abilities and becomes an artifact creature with power and
  toughness each equal to its converted mana cost. If ~ leaves the battlefield, this effect
  continues until end of turn. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    int legacy = create_legacy_effect(player, card, &titanias_song_legacy);
    get_card_instance(player, legacy)->targets[0].player = player;
    get_card_instance(player, legacy)->targets[0].card = card;
    get_card_instance(player, legacy)->number_of_targets = 1;
    get_card_instance(player, legacy)->token_status |= STATUS_INVISIBLE_FX;
  }

  return global_enchantment(player, card, event);
}

int card_transmute_artifact(int player, int card, event_t event){
/* CARD_ID_TRANSMUTE_ARTIFACT	537
Transmute Artifact	|U|U
Sorcery
Sacrifice an artifact. If you do, search your library for an artifact card. If that card's converted mana cost is less than or equal to the sacrificed artifact's
converted mana cost, put it onto the battlefield. If it's greater, you may pay |X, where X is the difference. If you do, put it onto the battlefield.
If you don't, put it into its owner's graveyard. Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ARTIFACT, "Select an Artifact card");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &this_test);
		if( sac ){
			int amount = get_cmc(BYTE2(sac), BYTE3(sac));
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			if( player == AI ){
				int count = 1;
				while( has_mana(player, COLOR_COLORLESS, count) ){
						count++;
				}
				this_test.cmc = amount+count+1;
				this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			}
			int card_added = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			if( card_added >= 0 ){
				int pip = 1;
				int diff = get_cmc(player, card_added)-amount;
				if( diff > 0 ){
					charge_mana(player, COLOR_COLORLESS, diff);
					pip = 1-spell_fizzled;
				}
				if( pip ){
					put_into_play(player, card_added);
				}
				else{
					put_on_top_of_deck(player, card_added);
					mill(player, 1);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x224caf0; original cards/antiquities.c:2728. */
int triskelion_like_creature(int player, int card, event_t event, int amount_of_starting_counters,
                             int c, int b, int u, int g, int r, int w)
{
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1,
                                      amount_of_starting_counters);
  if (event != EVENT_ACTIVATE && event != EVENT_CAN_ACTIVATE &&
      event != EVENT_RESOLVE_ACTIVATION && event != EVENT_CLEANUP)
    return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
    damage_target0(player, card, 1);
  return generic_activated_ability(player, card, event, 0x44, c, b, u, g, r, w, 0,
                                   &td, "TARGET_ANY");
}

int card_triskelion(int player, int card, event_t event){
	/* Triskelion	|6	0x2002b2c
	 * Artifact Creature - Construct 1/1
	 * ~ enters the battlefield with three +1/+1 counters on it.
	 * Remove a +1/+1 counter from ~: It deals 1 damage to any target. */

	return triskelion_like_creature(player, card, event, 3, MANACOST0);
}

int card_urzas_avenger(int player, int card, event_t event){
/* CARD_ID_URZAS_AVENGER	539
Urza's Avenger	|6
Artifact Creature - Shapeshifter 4/4
|0: ~ gets -1/-1 and gains your choice of banding, flying, first strike, or trample until end of turn.
(Any creatures with banding, and up to one without, can attack in a band. Bands are blocked as a group.
If any creatures with banding you control are blocking or being blocked by a creature, you divide that creature's combat damage, not its controller,
among any of the creatures it's being blocked by or is blocking.) */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		return player == AI ? result && get_toughness(player, card) > 1 : result;
	}

	if( event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int kw[4] = {KEYWORD_BANDING, KEYWORD_FLYING, KEYWORD_FIRST_STRIKE, KEYWORD_TRAMPLE};
			int already_pumped = instance->targets[1].player > -1 ? instance->targets[1].player : 0;
			int priorities[4] = {	current_phase < PHASE_DECLARE_ATTACKERS && !(already_pumped  & KEYWORD_BANDING) ? 5 : 1,
									current_phase < PHASE_DECLARE_BLOCKERS && !(already_pumped  & KEYWORD_FLYING) ? 10 : 1,
									current_phase == PHASE_AFTER_BLOCKING && !(already_pumped  & KEYWORD_FIRST_STRIKE) &&
									! is_unblocked(player, card) ? 10 : 1,
									current_phase == PHASE_AFTER_BLOCKING && !(already_pumped  & KEYWORD_TRAMPLE) &&
									! is_unblocked(player, card) ? 5 : 1,
			};

			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
								"Banding", 		1, priorities[0],
								"Flying", 		1, priorities[1],
								"First Strike", 1, priorities[2],
								"Trample", 		1, priorities[3]);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}

			instance->targets[1].card = kw[choice-1];
			if( instance->targets[1].player < 0 ){
				instance->targets[1].player = 0;
			}
			instance->targets[1].player |= kw[choice-1];
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								-1, -1, instance->targets[1].card, 0);
	}

	return 0;
}

int card_urzas_chalice(int player, int card, event_t event){
/* CARD_ID_URZAS_CHALICE	540
Urza's Chalice	|1
Artifact
Whenever a player casts an artifact spell, you may pay |1. If you do, you gain 1 life. */

	sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		if (has_mana(player, COLOR_ANY, 1)){
			charge_mana_while_resolving(player, card, EVENT_SPA, player, COLOR_COLORLESS, 1);
			if (spell_fizzled != 1){
				gain_life(player, 1);
			}
		}
	}
	return 0;
}

static int urzatron_land(int player, int card, event_t event, int mana_amt)
{
  if (event == EVENT_COUNT_MANA && affect_me(player, card) && !is_tapped(player, card) &&
      !is_animated_and_sick(player, card) && can_produce_mana(player, card))
    declare_mana_available(player, mana_colors_added[player],
                           urzatron_parts(player) == 7 ? mana_amt : 1);

  if (event == EVENT_CAN_ACTIVATE)
    return !is_tapped(player, card) && !is_animated_and_sick(player, card) &&
           can_produce_mana(player, card);

  if (event == EVENT_ACTIVATE)
    produce_mana_tapped(player, card, COLOR_COLORLESS, urzatron_parts(player) == 7 ? mana_amt : 1);

  if (event == EVENT_RESOLVE_SPELL)
    play_land_sound_effect(player, card);

  return 0;
}

int card_urzas_tower(int player, int card, event_t event)
{
/* CARD_ID_URZAS_TOWER	544
Urza's Tower	""
Land - Urza's Tower
|T: Add |C to your mana pool. If you control an Urza's Mine and an Urza's Power-Plant, add |C|C|C to your mana pool instead. */

  //0x4a91e0
  return urzatron_land(player, card, event, 3);
}

int card_urzas_mine_card_urzas_power_plant(int player, int card, event_t event)
{
/* CARD_ID_URZAS_MINE	541
Urza's Mine	""
Land - Urza's Mine
|T: Add |C to your mana pool. If you control an Urza's Power-Plant and an Urza's Tower, add |C|C to your mana pool instead. */

  //0x4a90b0
  return urzatron_land(player, card, event, 2);
}

static int urzas_miter_effect(int player, int card, event_t event)
{

  // In case something is copying Tablet of Epityr and then reverts to its original form.
  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_URZAS_MITER)
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
          }
          if (instance->targets[11].player < 0)
          {
            instance->targets[11].player = 0;
          }
          SET_BYTE2(instance->targets[11].player) = 1;
        }
        if (kill_code == KILL_DESTROY || kill_code == KILL_BURY)
        {
          count_for_gfp_ability(player, card, event, player, TYPE_ARTIFACT, NULL);
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
  if (!check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount && has_mana(player, COLOR_ANY, 3))
    {
      charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 3);
      if (spell_fizzled != 1)
      {
        draw_cards(player, 1);
      }
      else
      {
        break;
      }
      amount--;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

// wall of spears --> vanilla

int card_urzas_miter(int player, int card, event_t event){
  /* Urza's Miter |3
   * Artifact
   * Whenever an artifact you control is put into a graveyard from the battlefield, if it wasn't sacrificed, you may pay {3}.
   * 	If you do, draw a card. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)urzas_miter_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &urzas_miter_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
		if( kill_code == KILL_DESTROY || kill_code == KILL_BURY ){
			count_for_gfp_ability(player, card, event, player, TYPE_ARTIFACT, NULL);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( result ){
			int amount = BYTE0(result);
			while( amount && has_mana(player, COLOR_ANY, 3) ){
					charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 3);
					if( spell_fizzled != 1 ){
						draw_cards(player, 1);
					}
					else{
						break;
					}
					amount--;
			}
		}
	}

	return 0;
}

int card_weakstone(int player, int card, event_t event ){
/* CARD_ID_WEAKSTONE	546
Weakstone	|4
Artifact
Attacking creatures get -1/-0. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_POWER && is_attacking(affected_card_controller, affected_card) ){
			event_result--;
		}
	}
	return 0;
}

int card_xenic_poltergeist(int player, int card, event_t event){
/* CARD_ID_XENIC_POLTERGEIST	547
Xenic Poltergeist	|1|B|B
Creature - Spirit 1/1
|T: Until your next upkeep, target noncreature artifact becomes an artifact creature with power and toughness each equal to its converted mana cost. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT );
	td.preferred_controller = player;
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int pt = get_cmc(instance->targets[0].player, instance->targets[0].card);
			int legacy = turn_into_creature(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card,
											8, pt, pt);
			get_card_instance(player, legacy)->targets[4].player = instance->parent_controller;
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target noncreature artifact.");
}

// Yotian Soldier --> Serra Angel

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int triskelion_like_creature(int player, int card, event_t event, int amount_of_starting_counters,
                             int c, int b, int u, int g, int r, int w);

int card_yawgmoths_demon(int player, int card, event_t event){
/* CARD_ID_YAWGMOTH_DEMON	548
Yawgmoth Demon	|4|B|B
Creature - Demon 6/6
Flying (This creature can't be blocked except by creatures with flying or reach.)
First strike (This creature deals combat damage before creatures without first strike.)
At the beginning of your upkeep, you may sacrifice an artifact. If you don't, tap ~ and it deals 2 damage to you. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int kill = 1;
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		if( new_can_sacrifice(player, card, 1, &this_test) ){
			int ai_choice = 0;
			if( (! can_attack(player, card) && life[player] > 5) || count_subtype(player, TYPE_ARTIFACT, -1) < count_upkeeps(player) ){
				ai_choice = 1;
			}
			int choice = do_dialog(player, player, card, -1, -1, " Pay upkeep\n Pass", ai_choice);
			if( choice == 0 ){
				if( new_sacrifice(player, card, player, 0, &this_test) ){
					kill--;
				}
			}
		}
		if( kill == 1 ){
			damage_player(player, 2, player, card);
			tap_card(player, card);
		}
	}
	return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x22444f0; original cards/antiquities.c:687. */

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2244410; original cards/antiquities.c:12. */

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2245110; original cards/antiquities.c:33. */

#endif
