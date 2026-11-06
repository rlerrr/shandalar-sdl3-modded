// -*- c-basic-offset:2 -*-
// Keyword abilities, and abilities handled analagously to them.

#include "manalink.h"

static void set_misc_protection_impl(int player, int card);

/* Canonical recovered helper declarations. */
void remove_abilities_by_test(int player, event_t event, test_definition_t *test,
                                   ability_loss_t mode);
int can_use_activated_abilities(int player, int card);
int can_use_activated_abilities_by_instance(card_instance_t *instance);
int can_produce_mana(int player, int card);
void disable_all_activated_abilities(int player, int card, int mode);
void disable_nonmana_activated_abilities(int player, int card, int mode);
int humiliate_and_set_pt_abilities(int player, int card, int t_player, int t_card, int mode,
                                        test_definition_t *hc);
void remove_special_ability(int player, int card, int sa);
int check_for_special_ability(int player, int card, int keyword);
int get_special_abilities_by_instance(card_instance_t *instance);
int get_special_abilities(int player, int card);
void special_abilities(int player, int card, event_t event, int sp_abilities,
                            int source_player, int source_card);
int attack_if_able(int player, int card, event_t event);
int block_if_able(int player, int card, event_t event);
void vigilance(int player, int card, event_t event);
void granted_flanking(int s_player, int s_card, int t_player, int t_card, event_t event);
int humiliate(int src_player, int src_card, int t_player, int t_card, int mode);
int humility_legacy(int player, int card, event_t event);
static void lure_effect(int player, int card, event_t event, int luring_player,
                             int luring_card, int all_must_block, int subtype, int ability,
                             const char *prompt);
int is_humiliated(int player, int card, ability_loss_t mode);

/***********************
* Removal of abilities *
***********************/

/* Uses bits 0-11 of all cards' targets[17].card to indicate non-mana activated abilities removed (0-3), all activated abilities removed (4-7), and all
 * abilities removed (8-11).  Just in case one of these types of removal is imposed 16 times. */

int is_humiliated_by_instance(card_instance_t *instance, ability_loss_t mode)
{
  int flags = instance->targets[17].card;
  return flags > 0 ? flags & mode : 0;
}

/* ManalinkEh 2022: 0x26dd430; original functions/abilities.c:47. */
/* Native public is_humiliated(player, card, mode), with its private worker
 * inlined here. targets[17].card holds the ability-class removal mask. */
int is_humiliated(int player, int card, ability_loss_t mode)
{
  if (player == -1 || card == -1)
    return 0;
  int flags = get_card_instance(player, card)->targets[17].card;
  return flags > 0 ? flags & mode : 0;
}

static void real_humiliation(int t_player, int t_card, int mode){
	card_instance_t* instance = get_card_instance(t_player, t_card);

	if (instance->targets[17].card < 0){
		instance->targets[17].card = 0;
	}

	uint32_t u = instance->targets[17].card & 0xF00;

	if (mode == HUMILIATE_ON){
		if (u != 0xF00){	// counter not maxed
			u += 0x100;
		}
	}
	if (mode == HUMILIATE_OFF){
		if (u != 0){	// any counters to remove
			u -= 0x100;
		}
	}

	instance->targets[17].card &= ~0xF00;
	instance->targets[17].card |= u;
}

int set_pt_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  char enchanted_player = instance->damage_target_player;
  if (enchanted_player >= 0)
  {
    if (event == EVENT_POWER && instance->damage_target_card == affected_card &&
        enchanted_player == affected_card_controller && instance->counter_power >= 0)
      set_base_pt((int)instance->counter_power);
    else if (event == EVENT_TOUGHNESS && instance->damage_target_card == affected_card &&
             enchanted_player == affected_card_controller && instance->counter_toughness >= 0)
      set_base_pt((int)instance->counter_toughness);
  }
  unsigned int mode = instance->targets[1].player;
  if ((int)mode < 0)
    return 0;
  unsigned int end_effect = (event == EVENT_CLEANUP) & mode;
  leaves_play(instance->targets[2].player, instance->targets[2].card, event, 0, 0);
  unsigned int at_upkeep = mode & 4;
  if (event == EVENT_LEAVES_PLAY_ABILITY && (mode & 2) != 0 &&
      instance->targets[2].player >= 0)
  {
    if (at_upkeep == 0 || current_phase != 4 ||
        instance->damage_target_player != current_turn)
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
    end_effect = 1;
  }
  if (at_upkeep != 0 && current_phase == 4 &&
      instance->damage_target_player == current_turn)
  {
    if (upkeep_trigger(player, card, event))
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }
  if (end_effect != 0)
    kill_card(player, card, KILL_EXILE);
  return 0;
}

static int is_pt_effect(int player, int card){
	card_instance_t* inst = get_card_instance(player, card);
	if( is_what(player, card, TYPE_EFFECT) && (inst->info_slot == (intptr_t)set_pt_legacy || inst->display_pic_csv_id == CARD_ID_GIGANTOPLASM) ){
		return 1;
	}
	return 0;
}

int real_set_pt(int player, int card, int t_player, int t_card, int pow, int tou, int mode)
{
  if (mode & 8)
    for (int p = 0; p < 2; ++p)
      for (int c = active_cards_count[p]; c >= 0; --c)
      {
        card_instance_t *instance = in_play(p, c);
        if (instance && instance->damage_target_player == t_player &&
            instance->damage_target_card == t_card && instance->targets[2].player == player &&
            instance->targets[2].card == card && is_pt_effect(p, c))
          kill_card(p, c, KILL_EXILE);
      }
  int leg = create_targetted_legacy_effect(player, card, set_pt_legacy, t_player, t_card);
  if (leg != -1)
  {
    card_instance_t *legacy = get_card_instance(player, leg);
    legacy->targets[1].player = mode;
    legacy->counter_power = (int16_t)pow;
    legacy->counter_toughness = (int16_t)tou;
    legacy->targets[2].player = player;
    legacy->targets[2].card = card;
  }
  return leg;
}

int set_pt_and_abilities_until_eot(int player, int card, int t_player, int t_card, int pow, int tou,
                                   int key, int s_key, int mode)
{
  // pow = -1 : leave power unchanged
  // tou = -1 : leave toughness unchanged
  if (t_player != ANYBODY && t_card != -1)
  {
    real_set_pt(player, card, t_player, t_card, pow, tou, mode | 1);
    int legacy = -1;
    if (key || s_key)
    {
      legacy = pump_ability_until_eot(player, card, t_player, t_card, 0, 0, key, s_key);
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
    return legacy;
  }
  int i;
  for (i = 0; i < 2; i++)
  {
    if (i == t_player || t_player == ANYBODY)
    {
      int count = active_cards_count[i] - 1;
      while (count > -1)
      {
        if (in_play(i, count) && is_what(i, count, TYPE_CREATURE))
        {
          int good = 1;
          if ((mode & 1) && !(is_attacking(i, count) && is_unblocked(i, count)))
          {
            good = 0;
          }
          if (good == 1)
          {
            real_set_pt(player, card, i, count, pow, tou, mode | 1);
            if (key || s_key)
            {
              int legacy = pump_ability_until_eot(player, card, i, count, 0, 0, key, s_key);
              add_status(player, legacy, STATUS_INVISIBLE_FX);
            }
          }
        }
        count--;
      }
    }
  }
  return -1;
}

static void set_humiliation_legacy(int player, int card, int mode, test_definition_t *hc){
	card_instance_t *instance = get_card_instance(player, card);
	instance->targets[1].player = hc != NULL ? hc->power : 1;
	instance->targets[1].card = hc != NULL ? hc->toughness : 1;
	instance->targets[2].player = hc != NULL ? hc->keyword : 0;
	instance->targets[2].card = hc != NULL ? hc->color : 0;
	instance->targets[3].player = mode;
	instance->targets[3].card = hc != NULL ? hc->subtype : 0;
}

/****************************
* Abilities in sp_keyword_t *
****************************/

// static int block_if_able(int player, int card, event_t event);

// Single-bit abilities

void cannot_block(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x20;
    return;
  }
  return;
}

void fear(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 1;
    return;
  }
  return;
}

void horsemanship2(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x40000000;
    return;
  }
  return;
}

void intimidate(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x2000000;
    return;
  }
  return;
}
void lifelink(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 2;
    return;
  }
  return;
}
void rfg_when_damage(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x100000;
    return;
  }
  return;
}
void shadow(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x100;
    return;
  }
  return;
}
void unblockable(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x10;
    return;
  }
  return;
}
void wither(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x4000;
    return;
  }
  return;
}

#undef SP_ABILITY
#undef SP_ABILITY_SUPPRESSABLE

// Usually pump_ability_until_eot() is better, but this can go on non-creatures too (e.g. Boros Charm).
static void indestructible_part(int player, int card);

int indestructible_until_eot(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (affect_me(instance->damage_target_player, instance->damage_target_card))
		indestructible_part(instance->damage_target_player, instance->damage_target_card);
	}

  if (event == EVENT_CLEANUP)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

// Protection
void protection_from_creatures(int player, int card, event_t event)
{
  unblockable(player, card, event);

  card_instance_t* damage = damage_being_prevented(event);
  if (damage
	  && damage->damage_target_card == card && damage->damage_target_player == player
	  && (damage->targets[3].player & TYPE_CREATURE))
	damage->info_slot = 0;
}

// Regeneration

int can_be_regenerated(int player, int card)	// by something else.
{
  card_instance_t* instance = in_play(player, card);

  return ((!instance	// not in play
		   || instance->kill_code != KILL_DESTROY	// not being destroyed (or is being buried)
		   || (instance->token_status & STATUS_CANNOT_REGENERATE))
		  ? 0 : 99);
}

int can_regenerate(int player, int card){
	if ((land_can_be_played & LCBP_REGENERATION)	// This seems actively unhelpful - it prevents regeneration_shield_legacy() from working, for instance.
		&& can_be_regenerated(player, card)
		&& can_use_activated_abilities(player, card)){
		return 99;
	} else {
		return 0;
	}
}

int regenerate_target(int player, int card)
{
  card_instance_t *instance;
  int result;
  if ((player != -1) && (card != -1))
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      instance = get_card_instance(player, card);
      if ((instance->kill_code == '\x02') && ((instance->token_status & 0x800000) == 0))
      {
        regenerate_target_exe(player, card);
        result = get_id(player, card);
        if (result == 0x934)
        {
          result = is_humiliated(player, card, AL_STATIC);
          if (result == 0)
          {
            spiny_starfish_splitting_ability(player, card);
          }
        }
      }
    }
  }
  return 0;
}

int regeneration(int player, int card, event_t event, int cless, int black, int blue, int green, int red, int white){

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		if( can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_REGENERATION, cless, black, blue, green, red, white, 0, NULL, NULL);
}

int regeneration_shield_legacy(int player, int card, event_t event){
	/* targets[0]: Permanent this is attached to (also in card_instance_t::damage_target_player/card) */

	card_instance_t *instance = get_card_instance(player, card);

	if (instance->targets[0].player >= 0){
		if (instance->targets[1].player < 0){
			instance->targets[1].player = 0;
		}
		if (event == EVENT_ABILITIES && affect_me(instance->targets[0].player, instance->targets[0].card)){
			event_result |= KEYWORD_REGENERATION;	// mostly cosmetic, though its presence affects the AI
		}

		static int regeneration_from_shield_in_progress = 0;	// May not be necessary
		if (can_be_regenerated(instance->targets[0].player, instance->targets[0].card)
			&& !regeneration_from_shield_in_progress){
			++regeneration_from_shield_in_progress;
			regenerate_target(instance->targets[0].player, instance->targets[0].card);
			kill_card(player, card, KILL_EXILE);
			--regeneration_from_shield_in_progress;
		}
	}

	if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int regenerate_or_shield(int player, int card, int t_player, int t_card){
	if ((player == card && t_player == t_card) ? can_regenerate(player, card) : can_be_regenerated(player, card)){
		regenerate_target(t_player, t_card);
		return -1;
	} else {
		return create_targetted_legacy_effect(player, card, &regeneration_shield_legacy, t_player, t_card);
	}
}

static int legacy_cannot_regenerate_until_eot(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( eot_trigger(player, card, event) ){
		remove_status(instance->targets[0].player, instance->targets[0].card, STATUS_CANNOT_REGENERATE);
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int cannot_regenerate_until_eot(int player, int card, int t_player, int t_card){
	add_status(t_player, t_card, STATUS_CANNOT_REGENERATE);
	int leg = create_targetted_legacy_effect(player, card, &legacy_cannot_regenerate_until_eot, t_player, t_card);
	int leg2 = create_legacy_effect_exe(player, card, LEGACY_EFFECT_GENERIC, t_player, t_card);
	if (leg2 != -1){
		// exe looks, not for STATUS_CANNOT_REGENERATE on a card trying to regenerate, but a LEGACY_EFFECT_GENERIC with STATUS_CANNOT_REGENERATE attached to the card
		card_instance_t* legacy = get_card_instance(player, leg2);
		legacy->token_status |= STATUS_CANNOT_REGENERATE | STATUS_INVISIBLE_FX;
		// will be removed at eot since STATUS_PERMANENT isn't set
	}
	return leg;
}

// Other abilities

static int can_block_target_player, can_block_target_card;
/* Retained 208-byte test-layout compatibility implementation.
 * The native 2022 entry is remove_abilities_by_test_2022 below. */

/* ManalinkEh 2022: 0x26ddb20; original functions/abilities.c:355. */
void remove_abilities(int player, int card, event_t event, ability_loss_t mode)
{
  card_instance_t *instance = in_play(affected_card_controller, affected_card);
  if (instance != NULL)
  {
    int flags = instance->targets[17].card;
    if (flags >= 0)
      mode |= flags;
    instance->targets[17].card = mode;
  }
}

/* ManalinkEh 2022: 0x26ddb60; original functions/abilities.c:367. */
int has_an_activated_ability(iid_t iid)
{
  if (!(cards_data[iid].type & (TYPE_LAND | TYPE_CREATURE | TYPE_ENCHANTMENT | TYPE_ARTIFACT)))
    return 0;

  int csvid = cards_data[iid].id;
  switch (csvid)
  {
  case CARD_ID_FASTBOND:
  case CARD_ID_INVOKE_PREJUDICE:
  case CARD_ID_FORBIDDEN_LORE:
  case CARD_ID_MYSTIC_MIGHT:
  case CARD_ID_PYGMY_HIPPO:
  case CARD_ID_BONE_DANCER:
  case CARD_ID_GOBLIN_GRENADIERS:
  case CARD_ID_GOBLIN_VANDAL:
  case CARD_ID_OPHIDIAN:
  case CARD_ID_DAUTHI_MINDRIPPER:
  case CARD_ID_ROOTWATER_SHAMAN:
  case CARD_ID_SAMITE_BLESSING:
  case CARD_ID_THALAKOS_DECEIVER:
  case CARD_ID_HERMETIC_STUDY:
  case CARD_ID_YAWGMOTHS_WILL:
  case CARD_ID_SAPRAZZAN_HEIR:
  case CARD_ID_VERNAL_EQUINOX:
  case CARD_ID_LACCOLITH_GRUNT:
  case CARD_ID_LACCOLITH_TITAN:
  case CARD_ID_LACCOLITH_WARRIOR:
  case CARD_ID_LACCOLITH_WHELP:
  case CARD_ID_YAWGMOTHS_AGENDA:
  case CARD_ID_QUICKSILVER_DAGGER:
  case CARD_ID_ANIMAL_BONEYARD:
  case CARD_ID_CAUSTIC_TAR:
  case CARD_ID_CHAMBER_OF_MANIPULATION:
  case CARD_ID_PSIONIC_GIFT:
  case CARD_ID_SQUIRREL_NEST:
  case CARD_ID_ARCANE_TEACHINGS:
  case CARD_ID_FUTURE_SIGHT:
  case CARD_ID_MINDS_DESIRE:
  case CARD_ID_PSYCHIC_MEMBRANE:
  case CARD_ID_CRUCIBLE_OF_WORLDS:
  case CARD_ID_VEDALKEN_ORRERY:
  case CARD_ID_FLAME_FUSILLADE:
  case CARD_ID_SINS_OF_THE_PAST:
  case CARD_ID_COUNTERBALANCE:
  case CARD_ID_HAAKON_STROMGALD_SCOURGE:
  case CARD_ID_FALLEN_IDEAL:
  case CARD_ID_TEFERI_MAGE_OF_ZHALFIR:
  case CARD_ID_MAGUS_OF_THE_FUTURE:
  case CARD_ID_COLFENORS_URN:
  case CARD_ID_ELEMENTAL_MASTERY:
  case CARD_ID_FURYSTOKE_GIANT:
  case CARD_ID_POWER_OF_FIRE:
  case CARD_ID_BANISHING_KNACK:
  case CARD_ID_ENDLESS_HORIZONS:
  case CARD_ID_SCOURGE_OF_THE_NOBILIS:
  case CARD_ID_HIVE_MIND:
  case CARD_ID_ORACLE_OF_MUL_DAYA:
  case CARD_ID_PREDATORY_URGE:
  case CARD_ID_PYROMANCER_ASCENSION:
  case CARD_ID_SPHINX_OF_JWAR_ISLE:
  case CARD_ID_SPLINTER_TWIN:
  case CARD_ID_LEYLINE_OF_ANTICIPATION:
  case CARD_ID_STORMTIDE_LEVIATHAN:
  case CARD_ID_CLONE_SHELL:
  case CARD_ID_SEMBLANCE_ANVIL:
  case CARD_ID_SHIMMER_MYR:
  case CARD_ID_PRAETORS_GRASP:
  case CARD_ID_MANA_CHARGED_DRAGON:
  case CARD_ID_GARRUKS_HORDE:
  case CARD_ID_PAST_IN_FLAMES:
  case CARD_ID_SNAPCASTER_MAGE:
  case CARD_ID_TALONS_OF_FALKENRATH:
  case CARD_ID_ABUNDANT_GROWTH:
  case CARD_ID_DUAL_CASTING:
  case CARD_ID_LIGHTNING_PROWESS:
  case CARD_ID_OMNISCIENCE:
  case CARD_ID_YEVA_NATURES_HERALD:
  case CARD_ID_DEVIANT_GLEE:
  case CARD_ID_HYPERSONIC_DRAGON:
  case CARD_ID_PURSUIT_OF_FLIGHT:
  case CARD_ID_RACECOURSE_FURY:
  case CARD_ID_SEARCH_THE_CITY:
  case CARD_ID_SECURITY_BLOCKADE:
  case CARD_ID_UNDERWORLD_CONNECTIONS:
  case CARD_ID_DEBTORS_PULPIT:
  case CARD_ID_NIGHTVEIL_SPECTER:
  case CARD_ID_SKYGAMES:
  case CARD_ID_TIN_STREET_MARKET:
  case CARD_ID_MELEK_IZZET_PARAGON:
  case CARD_ID_DRAGON_MANTLE:
  case CARD_ID_PROPHET_OF_KRUPHIX:
  case CARD_ID_SPAWNING_GROUNDS:
  case CARD_ID_CLAIM_OF_EREBOS:
  case CARD_ID_COURSER_OF_KRUPHIX:
  case CARD_ID_EPHARAS_RADIANCE:
  case CARD_ID_EPIPHANY_STORM:
  case CARD_ID_EVANESCENT_INTELLECT:
  case CARD_ID_FEARSOME_TEMPER:
  case CARD_ID_LIGHTNING_VOLLEY:
  case CARD_ID_ORACLES_INSIGHT:
  case CARD_ID_RETRACTION_HELIX:
  case CARD_ID_PROPHETIC_FLAMESPEAKER:
  case CARD_ID_BURNING_ANGER:
  case CARD_ID_CHIEF_ENGINEER:
  case CARD_ID_LENS_OF_CLARITY:
  case CARD_ID_SINGING_BELL_STRIKE:
  case CARD_ID_GLEAM_OF_AUTHORITY:
  case CARD_ID_GRUESOME_SLAUGHTER:
  case CARD_ID_GIGANTOPLASM:
  case CARD_ID_OATH_OF_NISSA:
  case CARD_ID_RATTLECHAINS:
  case CARD_ID_SIGARDAS_AID:
  case CARD_ID_CONSPIRACY_EMISSARYS_PLOY:
  case CARD_ID_QUICKSMITH_REBEL:
  case CARD_ID_QUICKSMITH_SPY:
  case CARD_ID_VIZIER_OF_THE_MENAGERIE:
  case CARD_ID_RAMUNAP_EXCAVATOR:
  case CARD_ID_RAFF_CAPASHEN_SHIPS_MAGE:
  case CARD_ID_EXPANSION_EXPLOSION:
    return 0;
  default:
    break;
  }

  if ((cards_data[iid].extra_ability & EA_MANA_SOURCE) || csvid == CARD_ID_TREASONOUS_OGRE)
    return 2;
  if (cards_data[iid].extra_ability & (EA_ACT_ABILITY | EA_ACT_INTERRUPT))
    return 1;
  return ((unsigned int)check_card_for_rules_engine(iid) >> 12) & 1;
}

/* ManalinkEh 2022: 0x26de3c0; original functions/abilities.c:602. */
void special_abilities2(int player, int card, event_t event, sp_keyword2_t sp_keyword2)
{
  card_instance_t *instance;

  if (((event != EVENT_ABILITIES) || (card != affected_card)) ||
      (player != affected_card_controller))
  {
    return;
  }
  if (((player != -1) && (card != -1)) &&
      (is_humiliated(player, card, AL_KEY) != 0))
  {
    return;
  }
  instance = get_card_instance(player, card);
  instance->ext->sp_keywords2 = instance->ext->sp_keywords2 | sp_keyword2;
  return;
}

/* ManalinkEh 2022: 0x26de6f0; original functions/abilities.c:660. */
void squirrellink(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x20000000;
    return;
  }
  return;
}

void haste(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 4;
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26de830; original functions/abilities.c:664. */
void trample_over_planeswalkers(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x40000;
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26de880; original functions/abilities.c:665. */
void misc_protection(int player, int card, event_t event)
{
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    set_misc_protection_impl(player, card);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26dd130; original functions/abilities.c:665. */
static void set_misc_protection_impl(int player, int card)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  instance->ext->sp_keywords2 = instance->ext->sp_keywords2 | 1;
  return;
}

/* ManalinkEh 2022: 0x26de8b0; original functions/abilities.c:666. */
void misc_hexproof(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    instance = get_card_instance(player, card);
    instance->ext->sp_keywords2 = instance->ext->sp_keywords2 | 4;
    return;
  }
  return;
}

static const char* can_block_target(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  if (can_block_me(player, card, can_block_target_player, can_block_target_card))
	return NULL;
  else
	return gs_prompt_defendwhom_illegal_block;	// localized "Illegal block." from PROMPT_DEFENDWHOM
}

int can_block(int player, int card){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = 1-player;
	td.allowed_controller = 1-player;
	td.allow_cancel = 0;
	td.required_state = TARGET_STATE_ATTACKING;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(can_block_target);
	can_block_target_player = player;
	can_block_target_card = card;
	return can_target(&td);
}

void select_blocker(int player, int card, int player_who_chooses_blockers){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = 1-player;
	td.allowed_controller = 1-player;
	td.allow_cancel = 0;
	td.who_chooses = player_who_chooses_blockers;
	td.required_state = TARGET_STATE_ATTACKING;
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(can_block_target);
	can_block_target_player = player;
	can_block_target_card = card;

	card_instance_t *instance = get_card_instance(player, card);
	while( instance->blocking >= 255 && can_target(&td) ){
			instance->number_of_targets = 0;
			if( pick_target(&td, "PROMPT_DEFENDWHOM") ){
				block(player, card, 1-player, instance->targets[0].card);
			}
	}
}

const char* target_must_be_able_to_block_me(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  return get_card_instance(player, card)->blocking == 255 && is_legal_block(player, card, targeting_player, targeting_card) ? NULL : gs_prompt_defendwhom_illegal_block;	// localized "Illegal block." from PROMPT_DEFENDWHOM
}

int everybody_must_block_me(int player, int card, event_t event)
{
  lure_effect(player, card, event, player, card, 1, -1, 0, NULL);
  return 0;
}

void subtype_must_block_me(int player, int card, event_t event, subtype_t subtype, const char* prompt)
{
  lure_effect(player, card, event, player, card, 1, subtype, 0, prompt);
}

int must_be_blocked(int player, int card, event_t event)
{
  lure_effect(player, card, event, player, card, 0, -1, 0, NULL);
  return 0;
}

static int target_must_block_me_legacy(int player, int card, event_t event)
{
  if (trigger_condition == TRIGGER_MUST_BLOCK &&
      (event == EVENT_TRIGGER || event == EVENT_RESOLVE_TRIGGER) && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card), *luring, *lured;

    int luring_player = instance->damage_source_player;
    int luring_card = instance->damage_source_card;

    int lured_player = instance->damage_target_player;
    int lured_card = instance->damage_target_card;

    if (luring_player == current_turn &&
        reason_for_trigger_controller ==
            ((event_flags & EF_ATTACKER_CHOOSES_BLOCKERS) ? current_turn : 1 - current_turn) &&
        (luring = in_play(luring_player, luring_card)) && (luring->state & STATE_ATTACKING)

        && luring_player != lured_player && (lured = in_play(lured_player, lured_card)) &&
        lured->blocking == 255 &&
        is_legal_block(lured_player, lured_card, luring_player, luring_card))
    {
      if (event == EVENT_TRIGGER)
        event_result |= RESOLVE_TRIGGER_MANDATORY;

      if (event == EVENT_RESOLVE_TRIGGER)
        block(lured_player, lured_card, luring_player, luring_card);
    }
  }

  if ((event == EVENT_CLEANUP && (get_card_instance(player, card)->targets[2].player & 1)) ||
      (event == EVENT_PHASE_CHANGED && current_phase == PHASE_AFTER_BLOCKING &&
       (get_card_instance(player, card)->targets[2].player & 2)))
    kill_card(player, card, KILL_EXILE);

  return 0;
}

int target_must_block_me(int player, int card, int t_player, int t_card, int mode){
	// bitfields_for_mode
	// 1 --> Effect ends at eot
	int legacy_card = create_targetted_legacy_effect(player, card, &target_must_block_me_legacy, t_player, t_card);
	card_instance_t *legacy_instance = get_card_instance(player, legacy_card);
	legacy_instance->targets[2].player = mode;
	return legacy_card;
}

int exalted(int player, int card, event_t event, int ability, int sp_ability)
{
  if (declare_attackers_trigger(player, card, event, DAT_ATTACKS_ALONE|DAT_TRACK, player, -1))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int attacker = BYTE0(instance->targets[2].player);
	  if (in_play(player, attacker))
		pump_ability_until_eot(player, card, player, attacker, 1, 1, ability, sp_ability);

	  return attacker;
	}

  return -1;
}

int check_for_flanking_removal(int player, int card)
{
  if (!check_state(player, card, 0x44))
    return 0;

  int opponent = 1 - player;
  for (int candidate = 0; candidate < active_cards_count[opponent]; ++candidate)
  {
    if (in_play(opponent, candidate) && get_id(opponent, candidate) == 0x962)
    {
      if (opponent == -1 || candidate == -1 || !is_humiliated(opponent, candidate, AL_KEY))
        return 1;
    }
  }
  return 0;
}

void flanking(int player, int card, event_t event)
{
  granted_flanking(player, card, player, card, event);
}

int flash(int player, int card, event_t event){
	// Must also be set as type instant
	if (event == EVENT_CAN_CAST){
		return 1;
	} else if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		ai_modifier -= 16;
	}
	return 0;
}

int freeze_when_damage(int player, int card, event_t event)
{
  target_t target;
  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  instance->ext->sp_keywords = instance->ext->sp_keywords | 0x1000000;
  if (((player != -1) && (card != -1)) &&
      (result = is_humiliated(player, card, AL_TRIG), result != 0))
  {
    return 0;
  }
  damage_effects(player, card, event);
  if (((2 < instance->targets[1].player) && (trigger_condition == TRIGGER_DEAL_DAMAGE)) &&
      ((card == affected_card &&
        ((player == affected_card_controller && (reason_for_trigger_controller == player))))))
  {
    result = 2;
    do
    {
      target = instance->targets[result];
      ++result;
      effect_frost_titan(player, card, target.player, target.card);
    } while (result < instance->targets[1].player);
    instance->targets[1].player = 2;
  }
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  instance->targets[1].player = 2;
  return 0;
}

void give_haste(int player, int card) // Avoid!
{
  card_instance_t *instance = in_play(player, card);
  if (instance)
    instance->state &= ~0x30000U;
}

void persist(int player, int card, event_t event)
{
  if (count_minus1_minus1_counters(player, card) <= 0)
	{
	  nice_creature_to_sacrifice(player, card);

	  int owner, position;
	  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)
		  && find_in_owners_graveyard(player, card, &owner, &position))
		reanimate_permanent(owner, -1, owner, position, REANIMATE_MINUS1_MINUS1_COUNTER);	// -1 for card doesn't abort since no legacy is added
	}
}

void rampage(int player, int card, event_t event, int number)
{
  int result;
  card_instance_t *instance;
  if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (current_turn == player)
    {
      if (((card != -1) && (player != -1)) &&
          (result = is_humiliated(player, card, AL_KEY), result != 0))
      {
        return;
      }
      result = count_my_blockers(player, card);
      if (1 < result)
      {
        result = (result + -1) * number;
        pump_until_eot(player, card, player, card, result, result);
        return;
      }
    }
  }
  else if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      instance->ext->sp_keywords = instance->ext->sp_keywords | 0x40;
      return;
    }
  }
  else if (((event == EVENT_QUERY_AFF) && (card == affected_card)) &&
           ((player == affected_card_controller && (attacking_card == 0x40))))
  {
    if (((player != -1) && (card != -1)) &&
        (result = is_humiliated(player, card, AL_KEY), result != 0))
    {
      return;
    }
    event_result = event_result + number;
    return;
  }
  return;
}

void menace(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x10000;
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26df640; original functions/abilities.c:1183. */
void minblockers_3(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x80000;
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26df690; original functions/abilities.c:1191. */
void maxblockers_1(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {

    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x200000;
    return;
  }
  return;
}

void skulk(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords = instance->ext->sp_keywords | 0x20000;
    return;
  }
  return;
}

/*************************
* Detection of abilities *
*************************/

int check_for_ability(int player, int card, int keyword){
	if( card != -1 ){
		if( get_abilities(player, card, EVENT_ABILITIES, -1) & keyword ){
			return 1;
		}
	}
	else{
		int count = 0;
		while( count < active_cards_count[player] ){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
					if( get_abilities(player, count, EVENT_ABILITIES, -1) & keyword ){
						return 1;
					}
				}
				count++;
		}
	}
	return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

static void set_misc_protection_impl(int player, int card);

/* ManalinkEh 2022: 0x26df800; original functions/abilities.c:1250. */
int get_special_abilities_by_instance(card_instance_t *instance)
{
  unsigned int keyword = instance->ext->sp_keywords;
  if (instance->state & STATE_VIGILANCE)
  {
    keyword |= SP_KEYWORD_VIGILANCE;
    instance->ext->sp_keywords = keyword;
  }
  int mode = instance->targets[17].card;
  if (mode > 0)
  {
    if (mode & AL_KEY)
      keyword = 0;
    if (mode & AL_INDESTRUCTIBLE)
      keyword |= SP_KEYWORD_INDESTRUCTIBLE;
  }
  return keyword;
}

static void get_protection_source_impl(int8_t can_return_dmg, int *eff_acc, int *eff_ac);

/* ManalinkEh 2022: 0x26d7530; original functions/abilities.c:1269. */
static int has_sp_key_by_csvid_impl(int csvid, int sp_key)
{
  switch (csvid)
  {
  case CARD_ID_GLUTTONOUS_ZOMBIE:
  case CARD_ID_SEVERED_LEGION:
  case CARD_ID_SHADOWMAGE_INFILTRATOR:
  case CARD_ID_ORDER_OF_YAWGMOTH:
  case CARD_ID_SUQATA_ASSASSIN:
  case CARD_ID_RAZORTOOTH_RATS:
  case CARD_ID_COMMANDER_GREVEN_IL_VEC:
  case CARD_ID_SQUIRMING_MASS:
  case CARD_ID_CATERAN_ENFORCER:
  case CARD_ID_RATHI_INTIMIDATOR:
  case CARD_ID_AVATAR_OF_WOE:
  case CARD_ID_FRIGHTCRAWLER:
  case CARD_ID_GUILTFEEDER:
  case CARD_ID_DROSS_PROWLER:
  case CARD_ID_WOEBEARER:
  case CARD_ID_ARCBOUND_FIEND:
  case CARD_ID_DROSS_GOLEM:
  case CARD_ID_DESECRATION_ELEMENTAL:
  case CARD_ID_NEZUMI_CUTTHROAT:
  case CARD_ID_PAINWRACKER_ONI:
  case CARD_ID_CRAWLING_FILTH:
  case CARD_ID_DIMIR_HOUSE_GUARD:
  case CARD_ID_UNDERCITY_SHADE:
  case CARD_ID_RATCATCHER:
  case CARD_ID_SQUEALING_DEVIL:
  case CARD_ID_PHOBIAN_PHANTASM:
  case CARD_ID_CORPULENT_CORPSE:
  case CARD_ID_DUST_ELEMENTAL:
  case CARD_ID_NIHILITH:
  case CARD_ID_DREAD:
  case CARD_ID_SHRIEKMAW:
  case CARD_ID_SQUEAKING_PIE_SNEAK:
  case CARD_ID_PRICKLY_BOGGART:
  case CARD_ID_CROWD_OF_CINDERS:
  case CARD_ID_INKFATHOM_WITCH:
  case CARD_ID_LINGERING_TORMENTOR:
  case CARD_ID_JHESSIAN_ZOMBIES:
    return sp_key & SP_KEYWORD_FEAR;

  case CARD_ID_RHOX_WAR_MONK:
  case CARD_ID_KNIGHT_OF_MEADOWGRAIN:
  case CARD_ID_MISTMEADOW_SKULK:
  case CARD_ID_BRION_STOUTARM:
  case CARD_ID_CHANGELING_HERO:
  case CARD_ID_DIVINITY_OF_PRIDE:
  case CARD_ID_NIP_GWYLLION:
  case CARD_ID_VORACIOUS_HATCHLING:
  case CARD_ID_WINDWRIGHT_MAGE:
  case CARD_ID_BANESLAYER_ANGEL:
  case CARD_ID_CHILD_OF_NIGHT:
  case CARD_ID_CARAVAN_HURDA:
  case CARD_ID_ESCAPED_NULL:
  case CARD_ID_SERRA_ASCENDANT:
  case CARD_ID_FLENSERMITE:
  case CARD_ID_CHANCELLOR_OF_THE_DROSS:
  case CARD_ID_VAULT_SKIRGE:
  case CARD_ID_VISH_KAL_BLOOD_ARBITER:
  case CARD_ID_VAMPIRE_OUTCASTS:
  case CARD_ID_MARKOV_PATRICIAN:
  case CARD_ID_DROGSKOL_REAVER:
  case CARD_ID_ELGAUD_INQUISITOR:
  case CARD_ID_GRISELBRAND:
  case CARD_ID_SERAPH_OF_DAWN:
  case CARD_ID_ELDERWOOD_SCION:
  case CARD_ID_AJANIS_SUNSTRIKER:
  case CARD_ID_RHOX_FAITHMENDER:
  case CARD_ID_DAGGERDROME_IMP:
  case CARD_ID_TRAINED_CARACAL:
  case CARD_ID_BLOOD_BARON_OF_VIZKOPA:
  case CARD_ID_TITHE_DRINKER:
  case CARD_ID_ARCHANGEL_OF_THUNE:
  case CARD_ID_FIENDSLAYER_PALADIN:
  case CARD_ID_HOPEFUL_EIDOLON:
  case CARD_ID_INSATIABLE_HARPY:
  case CARD_ID_DAWNBRINGER_CHARIOTEERS:
  case CARD_ID_SUNGRACE_PEGASUS:
  case CARD_ID_ABZAN_GUIDE:
  case CARD_ID_SIDISIS_PET:
  case CARD_ID_SOULFIRE_GRAND_MASTER:
  case CARD_ID_CHAMPION_OF_ARASHIN:
  case CARD_ID_DRAGONLORD_DROMOKA:
  case CARD_ID_HIDDEN_DRAGONSLAYER:
  case CARD_ID_SHIELDHIDE_DRAGON:
  case CARD_ID_LILIANA_HERETICAL_HEALER:
  case CARD_ID_WAR_ORACLE:
  case CARD_ID_KALITAS_TRAITOR_OF_GHET:
  case CARD_ID_INDULGENT_ARISTOCRAT:
  case CARD_ID_NEARHEATH_CHAPLAIN:
  case CARD_ID_FAITHBEARER_PALADIN:
  case CARD_ID_GISELA_THE_BROKEN_BLADE:
  case CARD_ID_HERONS_GRACE_CHAMPION:
  case CARD_ID_LONE_RIDER:
  case CARD_ID_MARKOV_CRUSADER:
  case CARD_ID_AETHERBORN_MARAUDER:
  case CARD_ID_CONTRABAND_KINGPIN:
  case CARD_ID_TYMNA_THE_WEAVER:
  case CARD_ID_BALEFUL_AMMIT:
  case CARD_ID_SACRED_CAT:
  case CARD_ID_SCROUNGER_OF_SOULS:
  case CARD_ID_BLOODLINE_NECROMANCER:
  case CARD_ID_LICIA_SANGUINE_TRIBUNE:
  case CARD_ID_BISHOPS_SOLDIER:
  case CARD_ID_QUEENS_AGENT:
  case CARD_ID_ELENDA_THE_DUSK_ROSE:
  case CARD_ID_SADISTIC_SKYMARCHER:
  case CARD_ID_EVRA_HALCYON_WITNESS:
  case CARD_ID_LYRA_DAWNBRINGER:
  case CARD_ID_MESA_UNICORN:
  case CARD_ID_SANCTUM_SPIRIT:
  case CARD_ID_COURT_CLERIC:
  case CARD_ID_DAYBREAK_CHAPLAIN:
  case CARD_ID_MILITANT_ANGEL:
  case CARD_ID_VENGEANT_VAMPIRE:
  case CARD_ID_BLOOD_OPERATIVE:
  case CARD_ID_HEALERS_HAWK:
  case CARD_ID_THE_HAUNT_OF_HIGHTOWER:
  case CARD_ID_THIRSTING_SHADE:
  case CARD_ID_VIZKOPA_VAMPIRE:
  case CARD_ID_CHARITY_EXTRACTOR:
  case CARD_ID_CHARMED_STRAY:
  case CARD_ID_CAVALIER_OF_NIGHT:
  case CARD_ID_SEPHARA_SKYS_BLADE:
  case CARD_ID_KRRIK_SON_OF_YAWGMOTH:
  case CARD_ID_THALIAS_GEISTCALLER:
  case CARD_ID_BELOVED_PRINCESS:
  case CARD_ID_HUSHBRINGER:
  case CARD_ID_MURDEROUS_RIDER:
  case CARD_ID_SMITTEN_SWORDMASTER:
  case CARD_ID_ALSEID_OF_LIFES_BOUNTY:
  case CARD_ID_ARCHON_OF_SUNS_GRACE:
  case CARD_ID_DREAM_TRAWLER:
  case CARD_ID_GRAVEBREAKER_LAMIA:
  case CARD_ID_HATEFUL_EIDOLON:
  case CARD_ID_CUBWARDEN:
  case CARD_ID_LURRUS_OF_THE_DREAM_DEN:
  case CARD_ID_SPLENDOR_MARE:
  case CARD_ID_VITALITY_HUNTER:
  case CARD_ID_ANOINTED_CHORISTER:
  case CARD_ID_ARCHFIENDS_VESSEL:
  case CARD_ID_BASRIS_ACOLYTE:
  case CARD_ID_BLOOD_GLUTTON:
  case CARD_ID_INDULGING_PATRICIAN:
  case CARD_ID_MANGARA_THE_DIPLOMAT:
  case CARD_ID_GHASTLY_GLOOMHUNTER:
  case CARD_ID_ORAH_SKYCLAVE_HIEROPHANT:
  case CARD_ID_LATHIEL_THE_BOUNTEOUS_DAWN:
  case CARD_ID_LIESA_SHROUD_OF_DUSK:
    return sp_key & SP_KEYWORD_LIFELINK;

  case CARD_ID_NETHER_SHADOW:
  case CARD_ID_BALL_LIGHTNING:
  case CARD_ID_GOBLIN_CHARIOT:
  case CARD_ID_LAVA_HOUNDS:
  case CARD_ID_LIGHTNING_ELEMENTAL:
  case CARD_ID_RAGING_GOBLIN:
  case CARD_ID_VIASHINO_SANDSTALKER:
  case CARD_ID_BLAZING_SPECTER:
  case CARD_ID_SKYKNIGHT_LEGIONNAIRE:
  case CARD_ID_GROUNDBREAKER:
  case CARD_ID_SPARK_ELEMENTAL:
  case CARD_ID_RORIX_BLADEWING:
  case CARD_ID_SPIRIT_OF_THE_NIGHT:
  case CARD_ID_VULSHOK_SORCERER:
  case CARD_ID_RIP_CLAN_CRASHER:
  case CARD_ID_TALRUUM_MINOTAUR:
  case CARD_ID_SLITH_FIREWALKER:
  case CARD_ID_HELLKITE_OVERLORD:
  case CARD_ID_ERON_THE_RELENTLESS:
  case CARD_ID_VOLCANIC_DRAGON:
  case CARD_ID_THUNDERING_GIANT:
  case CARD_ID_JESKA_WARRIOR_ADEPT:
  case CARD_ID_BALDUVIAN_WAR_MAKERS:
  case CARD_ID_VIASHINO_CUTTHROAT:
  case CARD_ID_ASHEN_GHOUL:
  case CARD_ID_AMBUSH_PARTY:
  case CARD_ID_YAVIMAYA_ANTS:
  case CARD_ID_RAGING_COUGAR:
  case CARD_ID_RAGING_MINOTAUR:
  case CARD_ID_THUNDERMARE:
  case CARD_ID_FLOWSTONE_HELLION:
  case CARD_ID_FURNACE_SPIRIT:
  case CARD_ID_BRIMSTONE_DRAGON:
  case CARD_ID_OGRE_BERSERKER:
  case CARD_ID_CRAZED_SKIRGE:
  case CARD_ID_GOBLIN_WAR_BUGGY:
  case CARD_ID_SHIVAN_RAPTOR:
  case CARD_ID_AVALANCHE_RIDERS:
  case CARD_ID_VIASHINO_SANDSCOUT:
  case CARD_ID_GOBLIN_BERSERKER:
  case CARD_ID_KELDON_CHAMPION:
  case CARD_ID_WILD_COLOS:
  case CARD_ID_MOUNTAIN_BANDIT:
  case CARD_ID_RENEGADE_TROOPS:
  case CARD_ID_GERRARDS_IRREGULARS:
  case CARD_ID_KYREN_LEGATE:
  case CARD_ID_LAVA_RUNNER:
  case CARD_ID_SHRIEKING_MOGG:
  case CARD_ID_HALAM_DJINN:
  case CARD_ID_RAGING_KAVU:
  case CARD_ID_SKIZZIK:
  case CARD_ID_GOBLIN_RINGLEADER:
  case CARD_ID_MINOTAUR_TACTICIAN:
  case CARD_ID_KAMAHL_PIT_FIGHTER:
  case CARD_ID_MAGNIVORE:
  case CARD_ID_PARDIC_FIRECAT:
  case CARD_ID_CENTAUR_CHIEFTAIN:
  case CARD_ID_HELL_BENT_RAIDER:
  case CARD_ID_ICHORID:
  case CARD_ID_ANGER:
  case CARD_ID_AVARAX:
  case CARD_ID_BLISTERING_FIRECAT:
  case CARD_ID_MENACING_OGRE:
  case CARD_ID_RECKLESS_ONE:
  case CARD_ID_CLICKSLITHER:
  case CARD_ID_CRESTED_CRAGHORN:
  case CARD_ID_MACETAIL_HYSTRODON:
  case CARD_ID_SKIRK_ALARMIST:
  case CARD_ID_KARONA_FALSE_GOD:
  case CARD_ID_GOBLIN_STRIKER:
  case CARD_ID_VULSHOK_BATTLEMASTER:
  case CARD_ID_VULSHOK_BERSERKER:
  case CARD_ID_ARCBOUND_HYBRID:
  case CARD_ID_OXIDDA_GOLEM:
  case CARD_ID_IRON_BARB_HELLION:
  case CARD_ID_AKKI_LAVARUNNER:
  case CARD_ID_KIKI_JIKI_MIRROR_BREAKER:
  case CARD_ID_ASHEN_MONSTROSITY:
  case CARD_ID_AKUTA_BORN_OF_ASH:
  case CARD_ID_GLITTERFANG:
  case CARD_ID_ONI_OF_WILD_PLACES:
  case CARD_ID_SHINEN_OF_FURYS_FIRE:
  case CARD_ID_GOBLIN_FIRE_FIEND:
  case CARD_ID_HUNTED_DRAGON:
  case CARD_ID_GIANT_SOLIFUGE:
  case CARD_ID_HELLHOLE_RATS:
  case CARD_ID_STALKING_VENGEANCE:
  case CARD_ID_GARZA_ZOL_PLAGUE_QUEEN:
  case CARD_ID_LIGHTNING_SERPENT:
  case CARD_ID_BARBED_SHOCKER:
  case CARD_ID_MIRRI_THE_CURSED:
  case CARD_ID_TIMBERMARE:
  case CARD_ID_UKTABI_DRAKE:
  case CARD_ID_BONDED_FETCH:
  case CARD_ID_DEEPCAVERN_IMP:
  case CARD_ID_HENCHFIEND_OF_UKOR:
  case CARD_ID_SKIZZIK_SURGER:
  case CARD_ID_STORM_ENTITY:
  case CARD_ID_TAROX_BLADEWING:
  case CARD_ID_CHANGELING_BERSERKER:
  case CARD_ID_GLAREWIELDER:
  case CARD_ID_HOSTILITY:
  case CARD_ID_FROGTOSSER_BANNERET:
  case CARD_ID_DEMIGOD_OF_REVENGE:
  case CARD_ID_MUDBRAWLER_COHORT:
  case CARD_ID_PYRE_CHARGER:
  case CARD_ID_SOOTSTOKE_KINDLER:
  case CARD_ID_HEARTLASH_CINDER:
  case CARD_ID_HOTHEADED_GIANT:
  case CARD_ID_THUNDERBLUST:
  case CARD_ID_BLOODTHORN_TAUNTER:
  case CARD_ID_HELLS_THUNDER:
  case CARD_ID_PREDATOR_DRAGON:
  case CARD_ID_HELLSPARK_ELEMENTAL:
  case CARD_ID_RAKKA_MAR:
  case CARD_ID_BLITZ_HELLION:
  case CARD_ID_BLOODBRAID_ELF:
  case CARD_ID_GIANT_AMBUSH_BEETLE:
  case CARD_ID_IGNEOUS_POUNCER:
  case CARD_ID_KARRTHUS_TYRANT_OF_JUND:
  case CARD_ID_SEWN_EYE_DRAKE:
  case CARD_ID_THRAXIMUNDAR:
  case CARD_ID_GOBLIN_CHIEFTAIN:
  case CARD_ID_CRYPT_RIPPER:
  case CARD_ID_GOBLIN_GUIDE:
  case CARD_ID_GOBLIN_RUINBLASTER:
  case CARD_ID_HELLKITE_CHARGER:
  case CARD_ID_TUKTUK_GRUNTS:
  case CARD_ID_AKOUM_BATTLESINGER:
  case CARD_ID_CUNNING_SPARKMAGE:
  case CARD_ID_SKITTER_OF_LIZARDS:
  case CARD_ID_TUKTUK_THE_EXPLORER:
  case CARD_ID_VENGEVINE:
  case CARD_ID_ARC_RUNNER:
  case CARD_ID_GAEAS_REVENGE:
  case CARD_ID_BLACKCLEAVE_GOBLIN:
  case CARD_ID_FLAMEBORN_HELLION:
  case CARD_ID_KULDOTHA_PHOENIX:
  case CARD_ID_PUTREFAX:
  case CARD_ID_HELLKITE_IGNITER:
  case CARD_ID_HERO_OF_OXID_RIDGE:
  case CARD_ID_SLASH_PANTHER:
  case CARD_ID_SKULLBRIAR_THE_WALKING_GRAVE:
  case CARD_ID_CHANDRAS_PHOENIX:
  case CARD_ID_FALKENRATH_MARAUDERS:
  case CARD_ID_MANOR_SKELETON:
  case CARD_ID_ERDWAL_RIPPER:
  case CARD_ID_FALKENRATH_ARISTOCRAT:
  case CARD_ID_HELLRIDER:
  case CARD_ID_MARKOV_WARLORD:
  case CARD_ID_STRANGLEROOT_GEIST:
  case CARD_ID_ARCHWING_DRAGON:
  case CARD_ID_CRATERHOOF_BEHEMOTH:
  case CARD_ID_FERVENT_CATHAR:
  case CARD_ID_MAD_PROPHET:
  case CARD_ID_ZEALOUS_CONSCRIPTS:
  case CARD_ID_DUSKMANTLE_PROWLER:
  case CARD_ID_RECKLESS_BRUTE:
  case CARD_ID_THUNDERMAW_HELLKITE:
  case CARD_ID_ASH_ZEALOT:
  case CARD_ID_CARNIVAL_HELLSTEED:
  case CARD_ID_DREG_MANGLER:
  case CARD_ID_HYPERSONIC_DRAGON:
  case CARD_ID_IZZET_STATICASTER:
  case CARD_ID_MINOTAUR_AGGRESSOR:
  case CARD_ID_RAKDOS_SHRED_FREAK:
  case CARD_ID_TENEMENT_CRASHER:
  case CARD_ID_LEGION_LOYALIST:
  case CARD_ID_MOLTEN_PRIMORDIAL:
  case CARD_ID_EXAVA_RAKDOS_BLOOD_WITCH:
  case CARD_ID_MAZE_RUSHER:
  case CARD_ID_SPIKE_JESTER:
  case CARD_ID_VIASHINO_FIRSTBLADE:
  case CARD_ID_MINOTAUR_SKULLCLEAVER:
  case CARD_ID_MISTCUTTER_HYDRA:
  case CARD_ID_STORMBREATH_DRAGON:
  case CARD_ID_IMPETUOUS_SUNCHASER:
  case CARD_ID_ORACLE_OF_BONES:
  case CARD_ID_SATYR_NYX_SMITH:
  case CARD_ID_STORMCALLER_OF_KERANOS:
  case CARD_ID_ANKLE_SHANKER:
  case CARD_ID_GURMAG_SWIFTWING:
  case CARD_ID_MONASTERY_SWIFTSPEAR:
  case CARD_ID_VALLEY_DASHER:
  case CARD_ID_ZURGO_HELMSMASHER:
  case CARD_ID_TYRANTS_FAMILIAR:
  case CARD_ID_FLAMEWAKE_PHOENIX:
  case CARD_ID_LIGHTNING_SHRIEKER:
  case CARD_ID_SHAMAN_OF_THE_GREAT_HUNT:
  case CARD_ID_DRAGONLORD_KOLAGHAN:
  case CARD_ID_KOLAGHAN_STORMSINGER:
  case CARD_ID_FIREFIEND_ELEMENTAL:
  case CARD_ID_RECLUSIVE_ARTIFICER:
  case CARD_ID_SCAB_CLAN_BERSERKER:
  case CARD_ID_AKOUM_FIREBIRD:
  case CARD_ID_BEASTCALLER_SAVANT:
  case CARD_ID_DUST_STALKER:
  case CARD_ID_MUNDA_AMBUSH_LEADER:
  case CARD_ID_WARCHIEF_GIANT:
  case CARD_ID_ELDRAZI_OBLIGATOR:
  case CARD_ID_REALITY_SMASHER:
  case CARD_ID_RECKLESS_BUSHWHACKER:
  case CARD_ID_STORMCHASER_MAGE:
  case CARD_ID_EMBER_EYE_WOLF:
  case CARD_ID_GEIER_REACH_BANDIT:
  case CARD_ID_GOLDNIGHT_CASTIGATOR:
  case CARD_ID_INCORRIGIBLE_YOUTHS:
  case CARD_ID_VOLDAREN_DUELIST:
  case CARD_ID_DECIMATOR_OF_THE_PROVINCES:
  case CARD_ID_HANWEIR_THE_WRITHING_TOWNSHIP:
  case CARD_ID_IMPETUOUS_DEVILS:
  case CARD_ID_MOURNWILLOW:
  case CARD_ID_BOMAT_COURIER:
  case CARD_ID_BRAZEN_SCOURGE:
  case CARD_ID_FLEETWHEEL_CRUISER:
  case CARD_ID_IRON_LEAGUE_STEED:
  case CARD_ID_LATHNU_HELLION:
  case CARD_ID_OVALCHASE_DRAGSTER:
  case CARD_ID_SNARE_THOPTER:
  case CARD_ID_SPEEDWAY_FANATIC:
  case CARD_ID_CHARGING_CINDERHORN:
  case CARD_ID_KRAUM_LUDEVICS_OPUS:
  case CARD_ID_ENRAGED_GIANT:
  case CARD_ID_LIGHTNING_RUNNER:
  case CARD_ID_YAHENNI_UNDYING_PARTISAN:
  case CARD_ID_AHN_CROP_CRASHER:
  case CARD_ID_CROCODILE_OF_THE_CROSSING:
  case CARD_ID_GLORYBRINGER:
  case CARD_ID_EARTHSHAKER_KHENRA:
  case CARD_ID_EDGAR_MARKOV:
  case CARD_ID_IZZET_CHEMISTER:
  case CARD_ID_TERRITORIAL_HELLKITE:
  case CARD_ID_BRAZEN_BUCCANEERS:
  case CARD_ID_CAPTAIN_LANNERY_STORM:
  case CARD_ID_CHARGING_MONSTROSAUR:
  case CARD_ID_NEST_ROBBER:
  case CARD_ID_TILONALLIS_SKINSHIFTER:
  case CARD_ID_MASTERFUL_NINJA:
  case CARD_ID_PARTY_CRASHER:
  case CARD_ID_FANATICAL_FIREBRAND:
  case CARD_ID_STORM_FLEET_SPRINTER:
  case CARD_ID_ADELIZ_THE_CINDER_WIND:
  case CARD_ID_DARIGAAZ_REINCARNATED:
  case CARD_ID_GRAND_WARLORD_RADHA:
  case CARD_ID_KELDON_OVERSEER:
  case CARD_ID_PYROMANTIC_PILGRIM:
  case CARD_ID_WARCRY_PHOENIX:
  case CARD_ID_KHORVATH_BRIGHTFLAME:
  case CARD_ID_MAGMA_HELLION:
  case CARD_ID_HOSTILE_MINOTAUR:
  case CARD_ID_EMISSARY_OF_GRUDGES:
  case CARD_ID_LOYAL_APPRENTICE:
  case CARD_ID_TAWNOS_URZAS_APPRENTICE:
  case CARD_ID_ARCLIGHT_PHOENIX:
  case CARD_ID_BARGING_SERGEANT:
  case CARD_ID_GOLGARI_RAIDERS:
  case CARD_ID_TAJIC_LEGIONS_EDGE:
  case CARD_ID_TORCH_COURIER:
  case CARD_ID_CHARGING_WAR_BOAR:
  case CARD_ID_FIREBLADE_ARTIST:
  case CARD_ID_TIN_STREET_DODGER:
  case CARD_ID_DREADHORDE_BUTCHER:
  case CARD_ID_GOBLIN_ASSAULT_TEAM:
  case CARD_ID_RUBBLEBELT_RIOTERS:
  case CARD_ID_TENTH_DISTRICT_LEGIONNAIRE:
  case CARD_ID_GOBLIN_CHAMPION:
  case CARD_ID_LIGHTNING_SKELEMENTAL:
  case CARD_ID_ORE_SCALE_GUARDIAN:
  case CARD_ID_VENGEFUL_DEVIL:
  case CARD_ID_VIASHINO_SANDSPRINTER:
  case CARD_ID_GLINT_HORN_BUCCANEER:
  case CARD_ID_GOBLIN_SMUGGLER:
  case CARD_ID_LIGHTNING_STORMKIN:
  case CARD_ID_THUNDERKIN_AWAKENER:
  case CARD_ID_ANJE_FALKENRATH:
  case CARD_ID_SKYFIRE_PHOENIX:
  case CARD_ID_TECTONIC_HELLION:
  case CARD_ID_BURNING_YARD_TRAINER:
  case CARD_ID_CLACKBRIDGE_TROLL:
  case CARD_ID_DEATHLESS_KNIGHT:
  case CARD_ID_EMBERETH_PALADIN:
  case CARD_ID_FERVENT_CHAMPION:
  case CARD_ID_GINGERBRUTE:
  case CARD_ID_RANKLE_MASTER_OF_PRANKS:
  case CARD_ID_ROBBER_OF_THE_RICH:
  case CARD_ID_GALLIA_OF_THE_ENDLESS_DANCE:
  case CARD_ID_PHOENIX_OF_ASH:
  case CARD_ID_STORM_HERALD:
  case CARD_ID_THUNDERING_CHARIOT:
  case CARD_ID_INFERNIUS_SPAWNINGTON_III_ESQ:
  case CARD_ID_LAVA_SERPENT:
  case CARD_ID_SPRITE_DRAGON:
  case CARD_ID_YIDARO_WANDERING_MONSTER:
  case CARD_ID_DARING_FIENDBONDER:
  case CARD_ID_FIREFLUX_SQUAD:
  case CARD_ID_PAKO_ARCANE_RETRIEVER:
  case CARD_ID_BOLT_HOUND:
  case CARD_ID_CHANDRAS_FIREMAW:
  case CARD_ID_SUBIRA_TULZIDI_CARAVANNER:
  case CARD_ID_LIGHTNING_PHOENIX:
  case CARD_ID_BRUSHFIRE_ELEMENTAL:
  case CARD_ID_KAZA_ROIL_CHASER:
  case CARD_ID_RELIC_ROBBER:
  case CARD_ID_SHATTERSKULL_CHARGER:
  case CARD_ID_SHATTERSKULL_MINOTAUR:
  case CARD_ID_WAYWARD_GUIDE_BEAST:
  case CARD_ID_BOARDING_PARTY:
  case CARD_ID_FRENZIED_SADDLEBRUTE:
    return sp_key & SP_KEYWORD_HASTE;

  case CARD_ID_LIGHTNING_REAVER:
    return sp_key & (SP_KEYWORD_FEAR | SP_KEYWORD_HASTE);

  case CARD_ID_RAKDOS_RAGEMUTT:
  case CARD_ID_SPARK_TROOPER:
  case CARD_ID_BANEHOUND:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_HASTE);

  case CARD_ID_SERRA_ANGEL:
  case CARD_ID_YOTIAN_SOLDIER:
  case CARD_ID_BARTEL_RUNEAXE:
  case CARD_ID_RABID_WOMBAT:
  case CARD_ID_ZEPHYR_FALCON:
  case CARD_ID_WINDSEEKER_CENTAUR:
  case CARD_ID_ARDENT_MILITIA:
  case CARD_ID_DIVING_GRIFFIN:
  case CARD_ID_STOIC_ANGEL:
  case CARD_ID_ARCHANGEL:
  case CARD_ID_SERRA_SPHINX:
  case CARD_ID_MIRRI_CAT_WARRIOR:
  case CARD_ID_WILT_LEAF_CAVALIERS:
  case CARD_ID_GODSIRE:
  case CARD_ID_CHARGING_TROLL:
  case CARD_ID_GHOST_HOUNDS:
  case CARD_ID_BAY_FALCON:
  case CARD_ID_IRON_HEART_CHIMERA:
  case CARD_ID_TEMPEST_DRAKE:
  case CARD_ID_SKYSHROUD_FALCON:
  case CARD_ID_STANDING_TROOPS:
  case CARD_ID_ALABORN_GRENADIER:
  case CARD_ID_ARMORED_GRIFFIN:
  case CARD_ID_HERALD_OF_SERRA:
  case CARD_ID_RADIANT_ARCHANGEL:
  case CARD_ID_ALERT_SHU_INFANTRY:
  case CARD_ID_ANGEL_OF_LIGHT:
  case CARD_ID_VETERAN_CAVALIER:
  case CARD_ID_JHOVALL_QUEEN:
  case CARD_ID_STEADFAST_GUARD:
  case CARD_ID_ARDENT_SOLDIER:
  case CARD_ID_RIPTIDE_CRAB:
  case CARD_ID_TAHNGARTH_TALRUUM_HERO:
  case CARD_ID_CANTIVORE:
  case CARD_ID_MYSTIC_PENITENT:
  case CARD_ID_WAYWARD_ANGEL:
  case CARD_ID_MILITANT_MONK:
  case CARD_ID_POSSESSED_NOMAD:
  case CARD_ID_REBORN_HERO:
  case CARD_ID_BORDER_PATROL:
  case CARD_ID_LIEGE_OF_THE_AXE:
  case CARD_ID_NOBLE_TEMPLAR:
  case CARD_ID_RAZOR_GOLEM:
  case CARD_ID_LOXODON_STALWART:
  case CARD_ID_RAKSHA_GOLDEN_CUB:
  case CARD_ID_SKYHUNTER_PROWLER:
  case CARD_ID_TORII_WATCHWARD:
  case CARD_ID_COURIER_HAWK:
  case CARD_ID_GUARDIAN_OF_VITU_GHAZI:
  case CARD_ID_NIGHTGUARD_PATROL:
  case CARD_ID_OATHSWORN_GIANT:
  case CARD_ID_LIONHEART_MAVERICK:
  case CARD_ID_COURT_HUSSAR:
  case CARD_ID_ADARKAR_VALKYRIE:
  case CARD_ID_ITH_HIGH_ARCANIST:
  case CARD_ID_JEDITS_DRAGOONS:
  case CARD_ID_SERRA_AVENGER:
  case CARD_ID_ARBITER_OF_KNOLLRIDGE:
  case CARD_ID_OAKGNARL_WARRIOR:
  case CARD_ID_THOUGHTWEFT_TRIO:
  case CARD_ID_CHANGELING_SENTINEL:
  case CARD_ID_STONEHEWER_GIANT:
  case CARD_ID_KITHKIN_RABBLE:
  case CARD_ID_TWILIGHT_SHEPHERD:
  case CARD_ID_STEWARD_OF_VALERON:
  case CARD_ID_MEGLONOTH:
  case CARD_ID_NACATL_HUNT_PRIDE:
  case CARD_ID_SKYWARD_EYE_PROPHETS:
  case CARD_ID_CAPTAIN_OF_THE_WATCH:
  case CARD_ID_GRIFFIN_SENTINEL:
  case CARD_ID_KAZANDU_BLADEMASTER:
  case CARD_ID_SHEPHERD_OF_THE_LOST:
  case CARD_ID_IKIRAL_OUTRIDER:
  case CARD_ID_KNIGHT_OF_CLIFFHAVEN:
  case CARD_ID_SUN_TITAN:
  case CARD_ID_GLIMMERPOINT_STAG:
  case CARD_ID_KEMBAS_LEGION:
  case CARD_ID_PRIESTS_OF_NORN:
  case CARD_ID_CHANCELLOR_OF_THE_TANGLE:
  case CARD_ID_ELESH_NORN_GRAND_CENOBITE:
  case CARD_ID_HOVERMYR:
  case CARD_ID_TARIEL_RECKONER_OF_SOULS:
  case CARD_ID_ABBEY_GRIFFIN:
  case CARD_ID_ELITE_INQUISITOR:
  case CARD_ID_GEIST_HONORED_MONK:
  case CARD_ID_THRABEN_SENTRY:
  case CARD_ID_LOYAL_CATHAR:
  case CARD_ID_SCORNED_VILLAGER:
  case CARD_ID_VILLAGE_SURVIVORS:
  case CARD_ID_VORAPEDE:
  case CARD_ID_BRUNA_LIGHT_OF_ALABASTER:
  case CARD_ID_RIDERS_OF_GAVONY:
  case CARD_ID_THRABEN_VALIANT:
  case CARD_ID_KROND_THE_DAWN_CLAD:
  case CARD_ID_GUARDIAN_LIONS:
  case CARD_ID_SENTINEL_SPIDER:
  case CARD_ID_BRUSHSTRIDER:
  case CARD_ID_HUSSAR_PATROL:
  case CARD_ID_RISEN_SANCTUARY:
  case CARD_ID_ANGELIC_SKIRMISHER:
  case CARD_ID_KNIGHT_OF_OBLIGATION:
  case CARD_ID_LUMINATE_PRIMORDIAL:
  case CARD_ID_TRUEFIRE_PALADIN:
  case CARD_ID_JELENN_SPHINX:
  case CARD_ID_MAZE_SENTINEL:
  case CARD_ID_RURIC_THAR_THE_UNBOWED:
  case CARD_ID_TEYSA_ENVOY_OF_GHOSTS:
  case CARD_ID_ANAX_AND_CYMEDE:
  case CARD_ID_HUNDRED_HANDED_ONE:
  case CARD_ID_OBSERVANT_ALSEID:
  case CARD_ID_SENTRY_OF_THE_UNDERWORLD:
  case CARD_ID_ROON_OF_THE_HIDDEN_REALM:
  case CARD_ID_AKROAN_PHALANX:
  case CARD_ID_BRIMAZ_KING_OF_ORESKOS:
  case CARD_ID_VANGUARD_OF_BRIMAZ:
  case CARD_ID_EAGLE_OF_THE_WATCH:
  case CARD_ID_PHERES_BAND_WARCHIEF:
  case CARD_ID_RAVENOUS_LEUCROCOTA:
  case CARD_ID_AVACYN_GUARDIAN_ANGEL:
  case CARD_ID_SOUL_OF_THEROS:
  case CARD_ID_WARDEN_OF_THE_BEYOND:
  case CARD_ID_ALABASTER_KIRIN:
  case CARD_ID_HALLOWED_SPIRITKEEPER:
  case CARD_ID_DAGHATAR_THE_ADAMANT:
  case CARD_ID_DRAGON_BELL_MONK:
  case CARD_ID_OJUTAI_SOUL_OF_WINTER:
  case CARD_ID_CIRCLE_OF_ELDERS:
  case CARD_ID_HERALD_OF_DROMOKA:
  case CARD_ID_MISTHOOF_KIRIN:
  case CARD_ID_CITADEL_CASTELLAN:
  case CARD_ID_SENTINEL_OF_THE_ETERNAL_WATCH:
  case CARD_ID_SKYSNARE_SPIDER:
  case CARD_ID_TOPAN_FREEBLADE:
  case CARD_ID_DEATHLESS_BEHEMOTH:
  case CARD_ID_GHOSTLY_SENTINEL:
  case CARD_ID_WOODLAND_WANDERER:
  case CARD_ID_HERALD_OF_THE_HOST:
  case CARD_ID_KALEMNES_CAPTAIN:
  case CARD_ID_KALEMNE_DISCIPLE_OF_IROAS:
  case CARD_ID_AFFA_PROTECTOR:
  case CARD_ID_EMBODIMENT_OF_INSIGHT:
  case CARD_ID_STEPPE_GLIDER:
  case CARD_ID_SYLVAN_ADVOCATE:
  case CARD_ID_TAJURU_PATHWARDEN:
  case CARD_ID_WARDEN_OF_GEOMETRIES:
  case CARD_ID_ARCHANGEL_AVACYN:
  case CARD_ID_SAGE_OF_ANCIENT_LORE:
  case CARD_ID_BRUNA_THE_FADING_LIGHT:
  case CARD_ID_TANGLECLAW_WEREWOLF:
  case CARD_ID_WOODLAND_PATROL:
  case CARD_ID_CATACLYSMIC_GEARHULK:
  case CARD_ID_TRUSTY_COMPANION:
  case CARD_ID_AKIRI_LINE_SLINGER:
  case CARD_ID_AETHER_INSPECTOR:
  case CARD_ID_HEART_OF_KIRAN:
  case CARD_ID_AVEN_WIND_GUIDE:
  case CARD_ID_UNWAVERING_INITIATE:
  case CARD_ID_WINGED_SHEPHERD:
  case CARD_ID_ANGEL_OF_CONDEMNATION:
  case CARD_ID_BITTERBOW_SHARPSHOOTERS:
  case CARD_ID_DJERU_WITH_EYES_OPEN:
  case CARD_ID_RAMUNAP_HYDRA:
  case CARD_ID_STEADFAST_SENTINEL:
  case CARD_ID_BISHOP_OF_REBIRTH:
  case CARD_ID_DUSK_LEGION_DREADNOUGHT:
  case CARD_ID_HUATLIS_SNUBHORN:
  case CARD_ID_STEADFAST_ARMASAUR:
  case CARD_ID_SUNRISE_SEEKER:
  case CARD_ID_KNIGHT_OF_THE_WIDGET:
  case CARD_ID_RELENTLESS_RAPTOR:
  case CARD_ID_SUN_SENTINEL:
  case CARD_ID_ZACAMA_PRIMAL_CALAMITY:
  case CARD_ID_ARYEL_KNIGHT_OF_WINDGRACE:
  case CARD_ID_BAIRD_STEWARD_OF_ARGIVE:
  case CARD_ID_UNTAMED_KAVU:
  case CARD_ID_ARCHON_OF_VALORS_REACH:
  case CARD_ID_GORM_THE_GREAT:
  case CARD_ID_ARCADES_THE_STRATEGIST:
  case CARD_ID_GALLANT_CAVALRY:
  case CARD_ID_GREENWOOD_SENTINEL:
  case CARD_ID_KNIGHT_OF_THE_TUSK:
  case CARD_ID_MILITIA_BUGLER:
  case CARD_ID_PALLADIA_MORS_THE_RUINER:
  case CARD_ID_SERRAS_GUARDIAN:
  case CARD_ID_VIGILANT_BALOTH:
  case CARD_ID_LOYAL_UNICORN:
  case CARD_ID_THANTIS_THE_WARWEAVER:
  case CARD_ID_BOUNTY_AGENT:
  case CARD_ID_CONCLAVE_CAVALIER:
  case CARD_ID_INTRUSIVE_PACKBEAST:
  case CARD_ID_PARHELION_PATROL:
  case CARD_ID_ROSEMANE_CENTAUR:
  case CARD_ID_SWATHCUTTER_GIANT:
  case CARD_ID_SWIFTBLADE_VINDICATOR:
  case CARD_ID_VERNADI_SHIELDMATE:
  case CARD_ID_WARY_OKAPI:
  case CARD_ID_AZORIUS_KNIGHT_ARBITER:
  case CARD_ID_IMPERIOUS_OLIGARCH:
  case CARD_ID_LUMBERING_BATTLEMENT:
  case CARD_ID_SPHINX_OF_NEW_PRAHV:
  case CARD_ID_TENTH_DISTRICT_VETERAN:
  case CARD_ID_HUATLIS_RAPTOR:
  case CARD_ID_LOXODON_SERGEANT:
  case CARD_ID_MOWU_LOYAL_COMPANION:
  case CARD_ID_PARHELION_II:
  case CARD_ID_SEGOVIAN_ANGEL:
  case CARD_ID_CAVALIER_OF_DAWN:
  case CARD_ID_DIAMOND_KNIGHT:
  case CARD_ID_GARGOS_VICIOUS_WATCHER:
  case CARD_ID_GOLDMANE_GRIFFIN:
  case CARD_ID_KAALIA_ZENITH_SEEKER:
  case CARD_ID_SQUAD_CAPTAIN:
  case CARD_ID_STEADFAST_SENTRY:
  case CARD_ID_CLIFFSIDE_RESCUER:
  case CARD_ID_CHULANE_TELLER_OF_TALES:
  case CARD_ID_FAEBURROW_ELDER:
  case CARD_ID_FEASTING_TROLL_KING:
  case CARD_ID_LINDEN_THE_STEADFAST_QUEEN:
  case CARD_ID_LONESOME_UNICORN:
  case CARD_ID_REALM_CLOAKED_GIANT:
  case CARD_ID_SHINECHASER:
  case CARD_ID_SIGNPOST_SCARECROW:
  case CARD_ID_SILVERWING_SQUADRON:
  case CARD_ID_GRASPING_GIANT:
  case CARD_ID_TARANIKA_AKROAN_VETERAN:
  case CARD_ID_HIGHCLIFF_FELIDAR:
  case CARD_ID_ALERT_HEEDBONDER:
  case CARD_ID_FARFINDER:
  case CARD_ID_FRONDLAND_FELIDAR:
  case CARD_ID_IMPOSING_VANTASAUR:
  case CARD_ID_KAHEERA_THE_ORPHANGUARD:
  case CARD_ID_MAJESTIC_AURICORN:
  case CARD_ID_MANED_SERVAL:
  case CARD_ID_MOSSCOAT_GORIAK:
  case CARD_ID_YANNIK_SCAVENGING_SENTINEL:
  case CARD_ID_ALPINE_WATCHDOG:
  case CARD_ID_BASRIS_LIEUTENANT:
  case CARD_ID_ELDER_GARGAROTH:
  case CARD_ID_GARRUKS_WARSTEED:
  case CARD_ID_PREDATORY_WURM:
  case CARD_ID_VALOROUS_STEED:
  case CARD_ID_WARDEN_OF_THE_WOODS:
  case CARD_ID_EMERIA_CAPTAIN:
  case CARD_ID_EXPEDITION_HEALER:
  case CARD_ID_MURASA_ROOTGRAZER:
  case CARD_ID_PROWLING_FELIDAR:
  case CARD_ID_TROVE_WARDEN:
  case CARD_ID_VETERAN_ADVENTURER:
  case CARD_ID_AKROMA_VISION_OF_IXIDOR:
  case CARD_ID_COLFENOR_THE_LAST_YEW:
  case CARD_ID_KANGEE_SKY_WARDEN:
  case CARD_ID_STAUNCH_THRONEGUARD:
  case CARD_ID_YURLOK_OF_SCORCH_THRASH:
    return sp_key & SP_KEYWORD_VIGILANCE;

  case CARD_ID_SPHINX_OF_THE_STEEL_WIND:
  case CARD_ID_FELIDAR_SOVEREIGN:
  case CARD_ID_DAWNSTRIKE_PALADIN:
  case CARD_ID_CHROMANTICORE:
  case CARD_ID_BRISELA_VOICE_OF_NIGHTMARES:
  case CARD_ID_AERIAL_RESPONDER:
  case CARD_ID_ANGEL_OF_INVENTION:
  case CARD_ID_VONA_BUTCHER_OF_MAGAN:
  case CARD_ID_DANITHA_CAPASHEN_PARAGON:
  case CARD_ID_SUNBLADE_ANGEL:
  case CARD_ID_SPEAKER_OF_THE_HEAVENS:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_VIGILANCE);

  case CARD_ID_AKROMA_ANGEL_OF_WRATH:
  case CARD_ID_LIGHTNING_ANGEL:
  case CARD_ID_BULL_CERODON:
  case CARD_ID_RAZIA_BOROS_ARCHANGEL:
  case CARD_ID_HORDE_OF_NOTIONS:
  case CARD_ID_CERODON_YEARLING:
  case CARD_ID_AURELIA_THE_WARLEADER:
  case CARD_ID_MANTIS_RIDER:
  case CARD_ID_SASKIA_THE_UNYIELDING:
  case CARD_ID_SAMUT_VOICE_OF_DISSENT:
  case CARD_ID_GISHATH_SUNS_AVATAR:
  case CARD_ID_END_RAZE_FORERUNNERS:
  case CARD_ID_KELSIEN_THE_PLAGUE:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_VIGILANCE);

  case CARD_ID_DAUTHI_GHOUL:
  case CARD_ID_DAUTHI_HORROR:
  case CARD_ID_DAUTHI_MARAUDER:
  case CARD_ID_DAUTHI_MERCENARY:
  case CARD_ID_DAUTHI_MINDRIPPER:
  case CARD_ID_DAUTHI_SLAYER:
  case CARD_ID_SOLTARI_CRUSADER:
  case CARD_ID_SOLTARI_FOOT_SOLDIER:
  case CARD_ID_SOLTARI_GUERRILLAS:
  case CARD_ID_SOLTARI_LANCER:
  case CARD_ID_SOLTARI_MONK:
  case CARD_ID_SOLTARI_PRIEST:
  case CARD_ID_SOLTARI_TROOPER:
  case CARD_ID_THALAKOS_DREAMSOWER:
  case CARD_ID_THALAKOS_MISTFOLK:
  case CARD_ID_THALAKOS_SEER:
  case CARD_ID_THALAKOS_SENTRY:
  case CARD_ID_SOLTARI_CHAMPION:
  case CARD_ID_THALAKOS_DECEIVER:
  case CARD_ID_DAUTHI_CUTTHROAT:
  case CARD_ID_DAUTHI_JACKAL:
  case CARD_ID_DAUTHI_WARLORD:
  case CARD_ID_SOLTARI_VISIONARY:
  case CARD_ID_THALAKOS_SCOUT:
  case CARD_ID_DRIFTER_IL_DAL:
  case CARD_ID_FACELESS_DEVOURER:
  case CARD_ID_LOOTER_IL_KOR:
  case CARD_ID_STRONGHOLD_OVERSEER:
  case CARD_ID_ZEALOT_IL_VEC:
  case CARD_ID_AUGUR_IL_VEC:
  case CARD_ID_INFILTRATOR_IL_KOR:
  case CARD_ID_SPIRIT_EN_DAL:
  case CARD_ID_STRONGHOLD_RATS:
    return sp_key & SP_KEYWORD_SHADOW;

  case CARD_ID_NETHER_TRAITOR:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_SHADOW);

  case CARD_ID_WRENS_RUN_VANQUISHER:
  case CARD_ID_TIDEHOLLOW_STRIX:
  case CARD_ID_THORNWEALD_ARCHER:
  case CARD_ID_MOONGLOVE_WINNOWER:
  case CARD_ID_PESTILENT_KATHARI:
  case CARD_ID_WINGED_COATL:
  case CARD_ID_ACIDIC_SLIME:
  case CARD_ID_DEADLY_RECLUSE:
  case CARD_ID_GIANT_SCORPION:
  case CARD_ID_TURNTIMBER_BASILISK:
  case CARD_ID_DAGGERBACK_BASILISK:
  case CARD_ID_NIRKANA_CUTTHROAT:
  case CARD_ID_GRAVE_TITAN:
  case CARD_ID_GREATER_BASILISK:
  case CARD_ID_GLISSA_THE_TRAITOR:
  case CARD_ID_HORNET_QUEEN:
  case CARD_ID_DAMIA_SAGE_OF_STONE:
  case CARD_ID_VENGEFUL_PHARAOH:
  case CARD_ID_AMBUSH_VIPER:
  case CARD_ID_BITTERHEART_WITCH:
  case CARD_ID_TYPHOID_RATS:
  case CARD_ID_DIREGRAF_CAPTAIN:
  case CARD_ID_KESSIG_RECLUSE:
  case CARD_ID_HARVESTER_OF_SOULS:
  case CARD_ID_BALEFUL_STRIX:
  case CARD_ID_XATHRID_GORGON:
  case CARD_ID_SLUICEWAY_SCORPION:
  case CARD_ID_THRILL_KILL_ASSASSIN:
  case CARD_ID_WASTELAND_VIPER:
  case CARD_ID_KOROZDA_GORGON:
  case CARD_ID_MASTER_OF_CRUELTIES:
  case CARD_ID_MAZE_ABOMINATION:
  case CARD_ID_DEATHGAZE_COCKATRICE:
  case CARD_ID_LILIANAS_REAVER:
  case CARD_ID_AGENT_OF_THE_FATES:
  case CARD_ID_BALEFUL_EIDOLON:
  case CARD_ID_HYTHONIA_THE_CRUEL:
  case CARD_ID_KEEPSAKE_GORGON:
  case CARD_ID_SEDGE_SCORPION:
  case CARD_ID_FELHIDE_PETRIFIER:
  case CARD_ID_FLEETFEATHER_COCKATRICE:
  case CARD_ID_PHARIKAS_CHOSEN:
  case CARD_ID_SOUL_OF_INNISTRAD:
  case CARD_ID_HEIR_OF_THE_WILDS:
  case CARD_ID_RUTHLESS_RIPPER:
  case CARD_ID_SHAMBLING_ATTENDANTS:
  case CARD_ID_RAVING_DEAD:
  case CARD_ID_ACID_SPEWER_DRAGON:
  case CARD_ID_DEATHMIST_RAPTOR:
  case CARD_ID_DRAGONLORD_SILUMGAR:
  case CARD_ID_HAND_OF_SILUMGAR:
  case CARD_ID_SIDISI_UNDEAD_VIZIER:
  case CARD_ID_UKUD_COBRA:
  case CARD_ID_GRAVEBLADE_MARAUDER:
  case CARD_ID_PHARIKAS_DISCIPLE:
  case CARD_ID_FATHOM_FEEDER:
  case CARD_ID_MALAKIR_FAMILIAR:
  case CARD_ID_SKULLWINDER:
  case CARD_ID_AYLI_ETERNAL_PILGRIM:
  case CARD_ID_THE_GITROG_MONSTER:
  case CARD_ID_CRYPTOLITH_FRAGMENT:
  case CARD_ID_GNARLWOOD_DRYAD:
  case CARD_ID_GONTI_LORD_OF_LUXURY:
  case CARD_ID_THORN_OF_THE_BLACK_ROSE:
  case CARD_ID_SILAS_RENN_SEEKER_ADEPT:
  case CARD_ID_AETHER_POISONER:
  case CARD_ID_NARNAM_RENEGADE:
  case CARD_ID_BONE_PICKER:
  case CARD_ID_WASTELAND_SCORPION:
  case CARD_ID_RUIN_RAT:
  case CARD_ID_STEADY_HANDED_MOOK:
  case CARD_ID_DEADEYE_BRAWLER:
  case CARD_ID_DIRE_FLEET_POISONER:
  case CARD_ID_TETZIMOC_PRIMAL_DEATH:
  case CARD_ID_VAMPIRE_CHAMPION:
  case CARD_ID_FERAL_ABOMINATION:
  case CARD_ID_SOULBLADE_CORRUPTER:
  case CARD_ID_VIRTUS_THE_VEILED:
  case CARD_ID_ISARETH_THE_AWAKENER:
  case CARD_ID_POISON_TIP_ARCHER:
  case CARD_ID_NIGHT_INCARNATE:
  case CARD_ID_HIRED_POISONER:
  case CARD_ID_OCHRAN_ASSASSIN:
  case CARD_ID_PITILESS_GORGON:
  case CARD_ID_NOXIOUS_GROODION:
  case CARD_ID_ORZHOV_ENFORCER:
  case CARD_ID_GOD_ETERNAL_RHONAS:
  case CARD_ID_KRAUL_STINGER:
  case CARD_ID_PLAGUE_ENGINEER:
  case CARD_ID_VENOMOUS_CHANGELING:
  case CARD_ID_EMBODIMENT_OF_AGONIES:
  case CARD_ID_FOULMIRE_KNIGHT:
  case CARD_ID_OAKHAME_ADVERSARY:
  case CARD_ID_WINTERMOOR_COMMANDER:
  case CARD_ID_MIRE_TRITON:
  case CARD_ID_MOSS_VIPER:
  case CARD_ID_VENOMOUS_HIEROPHANT:
  case CARD_ID_CHEVILL_BANE_OF_MONSTERS:
  case CARD_ID_INSATIABLE_HEMOPHAGE:
  case CARD_ID_VOID_BECKONER:
  case CARD_ID_ZAXARA_THE_EXEMPLARY:
  case CARD_ID_HOODED_BLIGHTFANG:
  case CARD_ID_ORNERY_DILOPHOSAUR:
  case CARD_ID_WITCH_OF_THE_MOORS:
  case CARD_ID_TAJURU_BLIGHTBLADE:
  case CARD_ID_ELVISH_DREADLORD:
    return sp_key & SP_KEYWORD_DEATHTOUCH;

  case CARD_ID_VAMPIRE_NIGHTHAWK:
  case CARD_ID_WURMCOIL_ENGINE:
  case CARD_ID_GIFTED_AETHERBORN:
  case CARD_ID_ARVAD_THE_CURSED:
  case CARD_ID_LEYLINE_PROWLER:
  case CARD_ID_TOMEBOUND_LICH:
  case CARD_ID_VAMPIRE_OF_THE_DIRE_MOON:
  case CARD_ID_YAROK_THE_DESECRATED:
  case CARD_ID_ALELA_ARTFUL_PROVOCATEUR:
  case CARD_ID_NETHROI_APEX_OF_DEATH:
  case CARD_ID_NIGHTHAWK_SCAVENGER:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_DEATHTOUCH);

  case CARD_ID_QUEEN_MARCHESA:
  case CARD_ID_ZAGRAS_THIEF_OF_HEARTBEATS:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_DEATHTOUCH);

  case CARD_ID_ATRAXA_PRAETORS_VOICE:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_VIGILANCE | SP_KEYWORD_DEATHTOUCH);

  case CARD_ID_QUESTING_BEAST:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_VIGILANCE | SP_KEYWORD_DEATHTOUCH);

  case CARD_ID_DARKSTEEL_INGOT:
  case CARD_ID_DARKSTEEL_BRUTE:
  case CARD_ID_DARKSTEEL_CITADEL:
  case CARD_ID_DARKSTEEL_COLOSSUS:
  case CARD_ID_DARKSTEEL_GARGOYLE:
  case CARD_ID_DARKSTEEL_PENDANT:
  case CARD_ID_DARKSTEEL_REACTOR:
  case CARD_ID_MYR_MATRIX:
  case CARD_ID_STUFFY_DOLL:
  case CARD_ID_SAPLING_OF_COLFENOR:
  case CARD_ID_SPEARBREAKER_BEHEMOTH:
  case CARD_ID_ULAMOG_THE_INFINITE_GYRE:
  case CARD_ID_PHYLACTERY_LICH:
  case CARD_ID_DARKSTEEL_AXE:
  case CARD_ID_DARKSTEEL_JUGGERNAUT:
  case CARD_ID_DARKSTEEL_MYR:
  case CARD_ID_BLIGHTSTEEL_COLOSSUS:
  case CARD_ID_DARKSTEEL_PLATE:
  case CARD_ID_DARKSTEEL_RELIC:
  case CARD_ID_CREEPY_DOLL:
  case CARD_ID_PREDATOR_OOZE:
  case CARD_ID_TAJIC_BLADE_OF_THE_LEGION:
  case CARD_ID_COLOSSUS_OF_AKROS:
  case CARD_ID_EREBOS_GOD_OF_THE_DEAD:
  case CARD_ID_HELIOD_GOD_OF_THE_SUN:
  case CARD_ID_NYLEA_GOD_OF_THE_HUNT:
  case CARD_ID_PURPHOROS_GOD_OF_THE_FORGE:
  case CARD_ID_THASSA_GOD_OF_THE_SEA:
  case CARD_ID_EPHARA_GOD_OF_THE_POLIS:
  case CARD_ID_KARAMETRA_GOD_OF_HARVESTS:
  case CARD_ID_MOGIS_GOD_OF_SLAUGHTER:
  case CARD_ID_PHENAX_GOD_OF_DECEPTION:
  case CARD_ID_XENAGOS_GOD_OF_REVELS:
  case CARD_ID_ATHREOS_GOD_OF_PASSAGE:
  case CARD_ID_IROAS_GOD_OF_VICTORY:
  case CARD_ID_KERANOS_GOD_OF_STORMS:
  case CARD_ID_KRUPHIX_GOD_OF_HORIZONS:
  case CARD_ID_PHARIKA_GOD_OF_AFFLICTION:
  case CARD_ID_ULAMOG_THE_CEASELESS_HUNGER:
  case CARD_ID_FRUITCAKE_ELEMENTAL:
  case CARD_ID_STONEHOOF_CHIEFTAIN:
  case CARD_ID_CASCADING_CATARACTS:
  case CARD_ID_KEFNET_THE_MINDFUL:
  case CARD_ID_OKETRA_THE_TRUE:
  case CARD_ID_SERAPH_OF_THE_SUNS:
  case CARD_ID_SYLVAN_AWAKENING:
  case CARD_ID_IMPERVIOUS_GREATWURM:
  case CARD_ID_ATHREOS_SHROUD_VEILED:
  case CARD_ID_EREBOS_BLEAK_HEARTED:
  case CARD_ID_HELIOD_SUN_CROWNED:
  case CARD_ID_KLOTHYS_GOD_OF_DESTINY:
  case CARD_ID_NYLEA_KEEN_EYED:
  case CARD_ID_PURPHOROS_BRONZE_BLOODED:
  case CARD_ID_THASSA_DEEP_DWELLING:
  case CARD_ID_BRASH_TAUNTER:
  case CARD_ID_SKYCLAVE_RELIC:
    return sp_key & SP_KEYWORD_INDESTRUCTIBLE;

  case CARD_ID_TRANSCENDENT_MASTER:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_INDESTRUCTIBLE);

  case CARD_ID_HAZORET_THE_FERVENT:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_INDESTRUCTIBLE);

  case CARD_ID_WESTVALE_ABBEY:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_HASTE | SP_KEYWORD_INDESTRUCTIBLE);

  case CARD_ID_DARKSTEEL_SENTINEL:
  case CARD_ID_AVACYN_ANGEL_OF_HOPE:
  case CARD_ID_ZETALPA_PRIMAL_DAWN:
    return sp_key & (SP_KEYWORD_VIGILANCE | SP_KEYWORD_INDESTRUCTIBLE);

  case CARD_ID_RHONAS_THE_INDOMITABLE:
    return sp_key & (SP_KEYWORD_DEATHTOUCH | SP_KEYWORD_INDESTRUCTIBLE);

  case CARD_ID_BURNING_SHIELD_ASKARI:
  case CARD_ID_CADAVEROUS_KNIGHT:
  case CARD_ID_FEMEREF_KNIGHT:
  case CARD_ID_JOLRAELS_CENTAUR:
  case CARD_ID_MTENDA_HERDER:
  case CARD_ID_SEARING_SPEAR_ASKARI:
  case CARD_ID_SIDAR_JABARI:
  case CARD_ID_TELIMTOR:
  case CARD_ID_ZHALFIRIN_COMMANDER:
  case CARD_ID_ZHALFIRIN_KNIGHT:
  case CARD_ID_FALLEN_ASKARI:
  case CARD_ID_KNIGHT_OF_VALOR:
  case CARD_ID_KNIGHT_OF_THE_MISTS:
  case CARD_ID_TEFERIS_HONOR_GUARD:
  case CARD_ID_ZHALFIRIN_CRUSADER:
  case CARD_ID_SHADOW_RIDER:
  case CARD_ID_BENALISH_CAVALRY:
  case CARD_ID_BLAZING_BLADE_ASKARI:
  case CARD_ID_CAVALRY_MASTER:
  case CARD_ID_GUSTCLOAK_CAVALIER:
  case CARD_ID_KNIGHT_OF_THE_HOLY_NIMBUS:
  case CARD_ID_OUTRIDER_EN_KOR:
  case CARD_ID_PENTARCH_PALADIN:
  case CARD_ID_SKULKING_KNIGHT:
  case CARD_ID_RIFTMARKED_KNIGHT:
  case CARD_ID_BOGARDAN_LANCER:
  case CARD_ID_KNIGHT_OF_SURSI:
  case CARD_ID_SIDAR_KONDO_OF_JAMURAA:
  case CARD_ID_OLD_FOGEY:
    return sp_key & SP_KEYWORD_FLANKING;

  case CARD_ID_SUQATA_LANCER:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_FLANKING);

  case CARD_ID_CINDERBONES:
  case CARD_ID_INESCAPABLE_BRUTE:
  case CARD_ID_JUVENILE_GLOOMWIDOW:
  case CARD_ID_KULRATH_KNIGHT:
  case CARD_ID_LOCKJAW_SNAPPER:
  case CARD_ID_MIDNIGHT_BANSHEE:
  case CARD_ID_OONAS_GATEWARDEN:
  case CARD_ID_RUSTRAZOR_BUTCHER:
  case CARD_ID_SCUZZBACK_SCRAPPER:
  case CARD_ID_SICKLE_RIPPER:
  case CARD_ID_SLINKING_GIANT:
  case CARD_ID_WILDSLAYER_ELVES:
  case CARD_ID_DUERGAR_CAVE_GUARD:
  case CARD_ID_HARVEST_GWYLLION:
  case CARD_ID_HATEFLAYER:
  case CARD_ID_NECROSKITTER:
  case CARD_ID_NEEDLE_SPECTER:
  case CARD_ID_NOXIOUS_HATCHLING:
  case CARD_ID_PUNCTURE_BLAST:
  case CARD_ID_RENDCLAW_TROW:
  case CARD_ID_SMOLDERING_BUTCHER:
  case CARD_ID_STIGMA_LASHER:
  case CARD_ID_TWINBLADE_SLASHER:
    return sp_key & SP_KEYWORD_WITHER;

  case CARD_ID_BOGGART_RAM_GANG:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_WITHER);

  case CARD_ID_BATTLE_MAD_RONIN:
  case CARD_ID_BROTHERS_YAMAZAKI:
  case CARD_ID_BUSHI_TENDERFOOT:
  case CARD_ID_CURSED_RONIN:
  case CARD_ID_DEVOTED_RETAINER:
  case CARD_ID_KITSUNE_BLADEMASTER:
  case CARD_ID_KONDAS_HATAMOTO:
  case CARD_ID_MOTHRIDER_SAMURAI:
  case CARD_ID_NAGAO_BOUND_BY_HONOR:
  case CARD_ID_NEZUMI_RONIN:
  case CARD_ID_NUMAI_OUTCAST:
  case CARD_ID_SAMURAI_ENFORCERS:
  case CARD_ID_SAMURAI_OF_THE_PALE_CURTAIN:
  case CARD_ID_SENSEI_GOLDEN_TAIL:
  case CARD_ID_TAKENO_SAMURAI_GENERAL:
  case CARD_ID_INDEBTED_SAMURAI:
  case CARD_ID_ISAO_ENLIGHTENED_BUSHI:
  case CARD_ID_KENTARO_THE_SMILING_CAT:
  case CARD_ID_OPAL_EYE_KONDAS_YOJIMBO:
  case CARD_ID_RONIN_CLIFFRIDER:
  case CARD_ID_SILVERSTORM_SAMURAI:
  case CARD_ID_TAKENOS_CAVALRY:
  case CARD_ID_TOSHIRO_UMEZAWA:
  case CARD_ID_ARABA_MOTHRIDER:
  case CARD_ID_HAND_OF_CRUELTY:
  case CARD_ID_HAND_OF_HONOR:
  case CARD_ID_IIZUKA_THE_RUTHLESS:
  case CARD_ID_INNER_CHAMBER_GUARD:
  case CARD_ID_KITSUNE_DAWNBLADE:
  case CARD_ID_KUROS_TAKEN:
  case CARD_ID_RONIN_CAVEKEEPER:
  case CARD_ID_SOKENZAN_RENEGADE:
  case CARD_ID_SOKENZAN_SPELLBLADE:
    return sp_key & SP_KEYWORD_BUSHIDO;

  case CARD_ID_RONIN_HOUNDMASTER:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_BUSHIDO);

  case CARD_ID_KONDA_LORD_OF_EIGANJO:
    return sp_key & (SP_KEYWORD_VIGILANCE | SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_BUSHIDO);

  case CARD_ID_TWO_HEADED_DRAGON:
  case CARD_ID_WIND_SPIRIT:
  case CARD_ID_VIASHINO_RUNNER:
  case CARD_ID_VINE_KAMI:
  case CARD_ID_HORDE_OF_BOGGARTS:
  case CARD_ID_STORMBLOOD_BERSERKER:
  case CARD_ID_RIPSCALE_PREDATOR:
  case CARD_ID_PURPHOROSS_EMISSARY:
  case CARD_ID_IRE_SHAMAN:
  case CARD_ID_BOGGART_BRUTE:
  case CARD_ID_GILT_LEAF_WINNOWER:
  case CARD_ID_GEYSERFIELD_STALKER:
  case CARD_ID_SHATTERSKULL_RECRUIT:
  case CARD_ID_GOBLIN_DARK_DWELLERS:
  case CARD_ID_GOBLIN_FREERUNNER:
  case CARD_ID_KOZILEK_THE_GREAT_DISTORTION:
  case CARD_ID_GATSTAF_ARSONISTS:
  case CARD_ID_INSOLENT_NEONATE:
  case CARD_ID_MORKRUT_NECROPOD:
  case CARD_ID_RELENTLESS_DEAD:
  case CARD_ID_SIN_PRODDER:
  case CARD_ID_DERANGED_WHELP:
  case CARD_ID_ARADARA_EXPRESS:
  case CARD_ID_DEPUTIZED_PROTESTER:
  case CARD_ID_MAULFIST_SQUAD:
  case CARD_ID_NOXIOUS_GEARHULK:
  case CARD_ID_PYRETIC_HUNTER:
  case CARD_ID_WAYWARD_GIANT:
  case CARD_ID_IKRA_SHIDIQI_THE_USURPER:
  case CARD_ID_ALLEY_STRANGLER:
  case CARD_ID_GLINT_SLEEVE_SIPHONER:
  case CARD_ID_KARI_ZEV_SKYSHIP_RAIDER:
  case CARD_ID_SWEATWORKS_BRAWLER:
  case CARD_ID_CURSED_MINOTAUR:
  case CARD_ID_FLAMEBLADE_ADEPT:
  case CARD_ID_PLAGUE_BELCHER:
  case CARD_ID_DREAMSTEALER:
  case CARD_ID_GRANITIC_TITAN:
  case CARD_ID_KHENRA_SCRAPPER:
  case CARD_ID_KHERU_MIND_EATER:
  case CARD_ID_MATHAS_FIEND_SEEKER:
  case CARD_ID_DIRE_FLEET_INTERLOPER:
  case CARD_ID_FATHOM_FLEET_CAPTAIN:
  case CARD_ID_FIRE_SHRINE_KEEPER:
  case CARD_ID_RAMPAGING_FEROCIDON:
  case CARD_ID_SKY_TERROR:
  case CARD_ID_GOBLIN_HABERDASHER:
  case CARD_ID_OVERT_OPERATIVE:
  case CARD_ID_GOBLIN_TRAILBLAZER:
  case CARD_ID_TOMB_ROBBER:
  case CARD_ID_VORACIOUS_VAMPIRE:
  case CARD_ID_JOSU_VESS_LICH_KNIGHT:
  case CARD_ID_STRONGHOLD_CONFESSOR:
  case CARD_ID_BRAWL_BASH_OGRE:
  case CARD_ID_TWO_HEADED_ZOMBIE:
  case CARD_ID_LOYAL_SUBORDINATE:
  case CARD_ID_ROT_HULK:
  case CARD_ID_DIMIR_SPYBUG:
  case CARD_ID_KRAUL_RAIDER:
  case CARD_ID_GHOR_CLAN_WRECKER:
  case CARD_ID_RAKDOS_TRUMPETER:
  case CARD_ID_DREADMALKIN:
  case CARD_ID_GOD_ETERNAL_BONTU:
  case CARD_ID_IRON_BULLY:
  case CARD_ID_MASSACRE_GIRL:
  case CARD_ID_CABAL_THERAPIST:
  case CARD_ID_GLUTTONOUS_SLUG:
  case CARD_ID_GREVEN_PREDATOR_CAPTAIN:
  case CARD_ID_BELLE_OF_THE_BRAWL:
  case CARD_ID_ELITE_HEADHUNTER:
  case CARD_ID_LOCTHWAIN_PALADIN:
  case CARD_ID_STORMFIST_CRUSADER:
  case CARD_ID_ATRIS_ORACLE_OF_HALF_TRUTHS:
  case CARD_ID_CALCULATING_LICH:
  case CARD_ID_CAVERN_WHISPERER:
  case CARD_ID_HUNTED_NIGHTMARE:
  case CARD_ID_LABYRINTH_RAPTOR:
  case CARD_ID_SONOROUS_HOWLBONDER:
  case CARD_ID_NIKARA_LAIR_SCAVENGER:
  case CARD_ID_SILVAR_DEVOURER_OF_THE_FREE:
  case CARD_ID_BONE_PIT_BRUTE:
  case CARD_ID_KELS_FIGHT_FIXER:
  case CARD_ID_BLACKBLOOM_ROGUE:
  case CARD_ID_BREECHES_BRAZEN_PLUNDERER:
  case CARD_ID_ROGRAKH_SON_OF_ROHGAHH:
    return sp_key & SP_KEYWORD_MENACE;

  case CARD_ID_NULLPRIEST_OF_OBLIVION:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_MENACE);

  case CARD_ID_VILLAGE_MESSENGER:
  case CARD_ID_CHITTERING_HOST:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_MENACE);

  case CARD_ID_YENNETT_CRYPTIC_SOVEREIGN:
  case CARD_ID_SYR_GWYN_HERO_OF_ASHVALE:
  case CARD_ID_ABOMINATION_OF_LLANOWAR:
    return sp_key & (SP_KEYWORD_VIGILANCE | SP_KEYWORD_MENACE);

  case CARD_ID_KUNOROS_HOUND_OF_ATHREOS:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_VIGILANCE | SP_KEYWORD_MENACE);

  case CARD_ID_KEDEREKT_CREEPER:
  case CARD_ID_DIRE_FLEET_RAVAGER:
  case CARD_ID_PESTILENT_SPIRIT:
    return sp_key & (SP_KEYWORD_DEATHTOUCH | SP_KEYWORD_MENACE);

  case CARD_ID_BONTU_THE_GLORIFIED:
    return sp_key & (SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_MENACE);

  case CARD_ID_FORGOTTEN_CREATION:
  case CARD_ID_FURTIVE_HOMUNCULUS:
  case CARD_ID_PALE_RIDER_OF_TROSTAD:
  case CARD_ID_STARTLED_AWAKE:
  case CARD_ID_UNINVITED_GEIST:
  case CARD_ID_FOGWALKER:
  case CARD_ID_WHARF_INFILTRATOR:
    return sp_key & SP_KEYWORD_SKULK;

  case CARD_ID_FARBOG_REVENANT:
  case CARD_ID_VAMPIRE_CUTTHROAT:
    return sp_key & (SP_KEYWORD_LIFELINK | SP_KEYWORD_SKULK);

  case CARD_ID_RANCID_RATS:
    return sp_key & (SP_KEYWORD_DEATHTOUCH | SP_KEYWORD_SKULK);

  case CARD_ID_TAOIST_HERMIT:
  case CARD_ID_ZUO_CI_THE_MOCKING_SAGE:
  case CARD_ID_PLATED_SLAGWURM:
  case CARD_ID_TROLL_ASCETIC:
  case CARD_ID_SILHANA_LEDGEWALKER:
  case CARD_ID_DROVE_OF_ELVES:
  case CARD_ID_SLIPPERY_BOGLE:
  case CARD_ID_URIL_THE_MISTSTALKER:
  case CARD_ID_SACRED_WOLF:
  case CARD_ID_THRUN_THE_LAST_TROLL:
  case CARD_ID_AVEN_FLEETWING:
  case CARD_ID_DUNGROVE_ELDER:
  case CARD_ID_GLADECOVER_SCOUT:
  case CARD_ID_GEIST_OF_SAINT_TRAFT:
  case CARD_ID_INVISIBLE_STALKER:
  case CARD_ID_LUMBERKNOT:
  case CARD_ID_LONE_REVENANT:
  case CARD_ID_SIGARDA_HOST_OF_HERONS:
  case CARD_ID_PRIMAL_HUNTBEAST:
  case CARD_ID_RUBBLEBACK_RHINO:
  case CARD_ID_LAZAV_DIMIR_MASTERMIND:
  case CARD_ID_ASCENDED_LAWMAGE:
  case CARD_ID_WITCHSTALKER:
  case CARD_ID_BENTHIC_GIANT:
  case CARD_ID_SYLVAN_CARYATID:
  case CARD_ID_BASSARA_TOWER_ARCHER:
  case CARD_ID_MARCHESAS_EMISSARY:
  case CARD_ID_XATHRID_SLYBLADE:
  case CARD_ID_NARSET_ENLIGHTENED_MASTER:
  case CARD_ID_SAGU_MAULER:
  case CARD_ID_WHIRLWIND_ADEPT:
  case CARD_ID_SIEGE_BEHEMOTH:
  case CARD_ID_STORMSURGE_KRAKEN:
  case CARD_ID_SILUMGAR_THE_DRIFTING_DEATH:
  case CARD_ID_BELLTOLL_DRAGON:
  case CARD_ID_CONIFER_STRIDER:
  case CARD_ID_PLATED_CRUSHER:
  case CARD_ID_HEDRON_ALIGNMENT:
  case CARD_ID_SPHINX_OF_THE_FINAL_WORD:
  case CARD_ID_AUTUMNAL_GLOOM:
  case CARD_ID_BASTION_INVENTOR:
  case CARD_ID_SCALED_BEHEMOTH:
  case CARD_ID_STRIPED_RIVERWINDER:
  case CARD_ID_CARNAGE_TYRANT:
  case CARD_ID_JADE_GUARDIAN:
  case CARD_ID_AS_LUCK_WOULD_HAVE_IT:
  case CARD_ID_SOUL_OF_THE_RAPIDS:
  case CARD_ID_COLD_WATER_SNAPPER:
  case CARD_ID_KNIGHT_OF_GRACE:
  case CARD_ID_KNIGHT_OF_MALICE:
  case CARD_ID_LICHS_MASTERY:
  case CARD_ID_VINE_MARE:
  case CARD_ID_ANGLER_TURTLE:
  case CARD_ID_ARBORETUM_ELEMENTAL:
  case CARD_ID_NULLHIDE_FEROX:
  case CARD_ID_HUMONGULUS:
  case CARD_ID_SPHINX_OF_THE_GUILDPACT:
  case CARD_ID_WARDSCALE_CROCODILE:
  case CARD_ID_LOTUS_FIELD:
  case CARD_ID_SLIPPERY_BOGBONDER:
  case CARD_ID_GARRUKS_HARBINGER:
  case CARD_ID_NINE_LIVES:
  case CARD_ID_SPINED_MEGALODON:
  case CARD_ID_SPOREWEB_WEAVER:
  case CARD_ID_NEVINYRRAL_URBORG_TYRANT:
    return sp_key & SP_KEYWORD_HEXPROOF;

  case CARD_ID_CRAGPLATE_BALOTH:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_HEXPROOF);

  case CARD_ID_NIGHTVEIL_PREDATOR:
    return sp_key & (SP_KEYWORD_DEATHTOUCH | SP_KEYWORD_HEXPROOF);

  case CARD_ID_ELUSIVE_TORMENTOR:
    return sp_key & (SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_HEXPROOF);

  case CARD_ID_BLADETUSK_BOAR:
  case CARD_ID_HALO_HUNTER:
  case CARD_ID_BELLOWING_TANGLEWURM:
  case CARD_ID_GETH_LORD_OF_THE_VAULT:
  case CARD_ID_BLIND_ZEALOT:
  case CARD_ID_BRAIN_WEEVIL:
  case CARD_ID_GATSTAF_SHEPHERD:
  case CARD_ID_SPECTRAL_RIDER:
  case CARD_ID_ELBRUS_THE_BINDING_BLADE:
  case CARD_ID_HIGHBORN_GHOUL:
  case CARD_ID_IMMERWOLF:
  case CARD_ID_MIKAEUS_THE_UNHALLOWED:
  case CARD_ID_HEIRS_OF_STROMKIRK:
  case CARD_ID_VELA_THE_NIGHT_CLAD:
  case CARD_ID_SEPULCHRAL_PRIMORDIAL:
  case CARD_ID_ACADEMY_RAIDER:
  case CARD_ID_ACCURSED_SPIRIT:
  case CARD_ID_CYCLOPS_TYRANT:
  case CARD_ID_LIFEBANE_ZOMBIE:
  case CARD_ID_CAVERN_LAMPAD:
  case CARD_ID_FORLORN_PSEUDAMMA:
  case CARD_ID_KRENKOS_ENFORCER:
  case CARD_ID_FLESH_CARVER:
    return sp_key & SP_KEYWORD_INTIMIDATE;

  case CARD_ID_EARL_OF_SQUIRREL:
    return sp_key & SP_KEYWORD_SQUIRRELLINK;

  case CARD_ID_GUAN_YU_SAINTED_WARRIOR:
  case CARD_ID_LIU_BEI_LORD_OF_SHU:
  case CARD_ID_SHU_CAVALRY:
  case CARD_ID_SHU_ELITE_COMPANIONS:
  case CARD_ID_ZHAO_ZILONG_TIGER_GENERAL:
  case CARD_ID_LU_MENG_WU_GENERAL:
  case CARD_ID_LU_XUN_SCHOLAR_GENERAL:
  case CARD_ID_SUN_CE_YOUNG_CONQUERER:
  case CARD_ID_WU_ELITE_CAVALRY:
  case CARD_ID_WU_LIGHT_CAVALRY:
  case CARD_ID_WU_SCOUT:
  case CARD_ID_CAO_REN_WEI_COMMANDER:
  case CARD_ID_WEI_ELITE_COMPANIONS:
  case CARD_ID_WEI_NIGHT_RAIDERS:
  case CARD_ID_WEI_SCOUT:
  case CARD_ID_WEI_STRIKE_FORCE:
  case CARD_ID_XIAHOU_DUN_THE_ONE_EYED:
  case CARD_ID_ZHANG_HE_WEI_GENERAL:
  case CARD_ID_BARBARIAN_GENERAL:
  case CARD_ID_MA_CHAO_WESTERN_WARRIOR:
  case CARD_ID_YELLOW_SCARVES_CAVALRY:
  case CARD_ID_YELLOW_SCARVES_GENERAL:
  case CARD_ID_YUAN_SHAO_THE_INDECISIVE:
  case CARD_ID_LADY_ZHURONG_WARRIOR_QUEEN:
    return sp_key & SP_KEYWORD_HORSEMANSHIP;

  case CARD_ID_LU_BU_MASTER_AT_ARMS:
    return sp_key & (SP_KEYWORD_HASTE | SP_KEYWORD_HORSEMANSHIP);

  case CARD_ID_SHU_GENERAL:
  case CARD_ID_ZHANG_FEI_FIERCE_WARRIOR:
    return sp_key & (SP_KEYWORD_VIGILANCE | SP_KEYWORD_HORSEMANSHIP);

  default:
    return 0;
  }
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void remove_abilities(int player, int card, event_t event, ability_loss_t mode);
void remove_abilities_by_test(int player, event_t event, test_definition_t *test,
                              ability_loss_t mode);

/* ManalinkEh 2022: 0x26df840; original functions/abilities.c:2780. */
int get_special_abilities(int player, int card)
{
  if (player == -1)
    return has_sp_key_by_csvid_impl((unsigned int)cards_data[card].id, 0x6283eb0f);
  card_instance_t *instance = get_card_instance(player, card);
  return get_special_abilities_by_instance(instance);
}

/* ManalinkEh 2022: 0x26df890; original functions/abilities.c:2786. */
int check_for_special_ability(int player, int card, int keyword)
{
  card_instance_t *instance;
  unsigned int special_keywords;
  if (player == -1)
  {
    return has_sp_key_by_csvid_impl((unsigned int)cards_data[card].id, keyword);
  }
  instance = get_card_instance(player, card);
  special_keywords = get_special_abilities_by_instance(instance);
  return special_keywords & keyword;
}

/* ManalinkEh 2022: 0x26e05a0; original functions/abilities.c:2793. */
void remove_special_ability(int player, int card, int sa)
{
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  instance->ext->sp_keywords = instance->ext->sp_keywords & (sa ^ 0xffffffffU);
  return;
}

int fx_menace(int player, int card, event_t event)
{
  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);

    special_abilities(instance->damage_target_player, instance->damage_target_card, event,
                      SP_KEYWORD_MENACE, player, card);

    if (event == EVENT_CLEANUP && is_what(player, card, TYPE_EFFECT))
    {
      card_instance_t *inst = get_card_instance(player, card);
      remove_special_ability(inst->damage_target_player, inst->damage_target_card,
                             SP_KEYWORD_MENACE);
      kill_card(player, card, KILL_EXILE);
    }
  }

  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

void set_special_abilities(int player, int card, int type, int value)
{
  if (card != -1)
  {
    get_card_instance(player, card)->ext->sp_keywords |= value;
    return;
  }
  for (int p = 0; p < 2; ++p)
    if (p == player || player == ANYBODY)
      for (int c = active_cards_count[p] - 1; c >= 0; --c)
        if (in_play(p, c) && is_what(p, c, type))
          get_card_instance(p, c)->ext->sp_keywords |= value;
}

int legacy_negate_ability_and_special_ability(int player, int card, event_t event)
{

  if (event == EVENT_ABILITIES)
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (affect_me(instance->damage_target_player, instance->damage_target_card))
      {
        if (instance->targets[1].player > -1)
        {
          event_result &= ~instance->targets[1].player;
        }
        if (instance->targets[1].card > -1)
        {
          remove_special_ability(instance->damage_target_player, instance->damage_target_card,
                                 instance->targets[1].card);
        }
      }
    }
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int effect_on_all_negate_ability_and_special_ability_legacy_attached_to_me(int player, int card,
                                                                           int effect)
{
  // effect = 0 --> check only
  // effect = 1 -> enable the "control legacy" with the highest timestamp (AKA the most recent)
  // effect = -1 -> disable all "control legacies"
  int result = 0;
  int legacies[100][3];
  int lc = 0;
  card_instance_t *inst;
  int p, c;
  for (p = 0; p <= 1; ++p)
  {
    for (c = active_cards_count[p]; c > -1; c--)
    {
      if ((inst = in_play(p, c)) && inst->damage_target_player == player &&
          inst->damage_target_card == card)
      {
        if (is_what(p, c, TYPE_EFFECT) &&
            inst->info_slot == (intptr_t)legacy_negate_ability_and_special_ability)
        {
          if (effect == 1 && !check_status(p, c, STATUS_CONTROLLED))
          {
            legacies[lc][0] = inst->timestamp;
            legacies[lc][1] = p;
            legacies[lc][2] = c;
            lc++;
            result = 1;
          }
          if (effect == -1)
          {
            remove_status(p, c, STATUS_CONTROLLED);
            result = 1;
          }
        }
      }
    }
  }
  if (lc)
  {
    int max_timestamp = 0;
    int id_of_legacy_to_activate = 0;
    int i;
    for (i = 0; i < lc; i++)
    {
      if (legacies[i][0] > max_timestamp)
      {
        max_timestamp = legacies[i][0];
      }
      id_of_legacy_to_activate = i;
    }
    add_status(legacies[id_of_legacy_to_activate][1], legacies[id_of_legacy_to_activate][2],
               STATUS_CONTROLLED | STATUS_INVISIBLE_FX);
  }
  return result;
}

static int create_negate_ability_and_special_ability_until_eot(int player, int card, int t_player, int t_card, keyword_t keyword,
	int sp_ability)
{
	effect_on_all_negate_ability_and_special_ability_legacy_attached_to_me(t_player, t_card, -1);
	int legacy = create_targetted_legacy_effect(player, card, &legacy_negate_ability_and_special_ability, t_player, t_card);
	card_instance_t *leg = get_card_instance(player, legacy);
	leg->targets[1].player = keyword;
	leg->targets[1].card = sp_ability;
	leg->targets[2].player = 1;
	return legacy;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void squirrellink(int player, int card, event_t event);
void trample_over_planeswalkers(int player, int card, event_t event);
void misc_hexproof(int player, int card, event_t event);
void minblockers_3(int player, int card, event_t event);
void maxblockers_1(int player, int card, event_t event);
void special_abilities2(int player, int card, event_t event, sp_keyword2_t sp_keyword2);

/* Affects all of t_player's creatures if t_card == -1, and all creatures if t_player == 2 as well.  t_player == 2, t_card != -1 is undefined behavior.  Returns
 * created legacy if t_player != 2 and t_card != -1 (i.e., a single creature), otherwise -1. */
static int negate_ability_and_special_ability_until_eot_impl(int player, int card, int t_player, int t_card, keyword_t keyword, int s_keyword){
	int i;
	for(i=0; i<2; i++){
		if( i == t_player || t_player == ANYBODY ){
			if( t_card != -1 ){
				return create_negate_ability_and_special_ability_until_eot(player, card, t_player, t_card, keyword, s_keyword);
			}
			else{
				int count = active_cards_count[i]-1;
				while( count > -1 ){
						if( in_play(i, count) && is_what(i, count, TYPE_CREATURE) ){
							create_negate_ability_and_special_ability_until_eot(player, card, i, count, keyword, s_keyword);
						}
						count--;
				}
			}
		}
	}
	return -1;
}

int negate_ability_until_eot(int player, int card, int t_player, int t_card, keyword_t keyword){
	return negate_ability_and_special_ability_until_eot_impl(player, card, t_player, t_card, keyword, 0);
}

int negate_ability_and_special_ability_until_eot(int player, int card, int t_player, int t_card, keyword_t keyword, int s_keyword){
	return negate_ability_and_special_ability_until_eot_impl(player, card, t_player, t_card, keyword, s_keyword);
}

/* ManalinkEh 2022: 0x26e09a0; original functions/abilities.c:2930. */
void get_protection_source(int8_t can_return_dmg, int *eff_acc, int *eff_ac)
{
  if (-1 < attacking_card)
  {
    get_protection_source_impl(can_return_dmg, eff_acc, eff_ac);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26dd200; original functions/abilities.c:2930. */
static void get_protection_source_impl(int8_t can_return_dmg, int *eff_acc, int *eff_ac)
{
  *eff_acc = attacking_card_controller;
  int c = attacking_card;
  *eff_ac = attacking_card;
  card_instance_t *inst = get_card_instance(*eff_acc, c);
  while (inst)
  {
    int iid = inst->internal_card_id;
    int p;
    if (iid == damage_card)
    {
      if (can_return_dmg)
        return;
      p = inst->damage_source_player;
      c = inst->damage_source_card;
    }
    else if (iid == activation_card)
    {
      p = inst->parent_controller;
      c = inst->parent_card;
    }
    else
    {
      csvid_t csvid = cards_data[iid].id;
      if (csvid == 0x386 && inst->damage_target_player >= 0)
      {
        p = inst->damage_target_player;
        c = inst->damage_target_card;
      }
      else
      {
        if ((unsigned int)csvid - 0x386U > 1U)
          return;
        if (iid == LEGACY_EFFECT_ALADDIN)
        {
          p = inst->targets[0].player;
          c = inst->targets[0].card;
        }
        else
        {
          p = inst->damage_source_player;
          c = inst->damage_source_card;
        }
      }
    }
    inst = get_card_instance(p, c);
    if (!inst)
      return;
    *eff_acc = p;
    *eff_ac = c;
  }
}

void protection_from_subtype(int player, int card, event_t event, subtype_t subt)
{
  int p, c;
  if (event == EVENT_ABILITIES)
  {
    if (affect_me(player, card))
      set_misc_protection_impl(player, card);
  }
  else if (event == EVENT_PROT_MISC && affect_me(player, card))
  {
    if (((unsigned int)event_result >> 24 & 0xff) == 6)
    {
      event_result |= 1;
      return;
    }
    // Native non-query events require a valid attacking-card source.
    if (attacking_card >= 0)
      get_protection_source_impl(1, &p, &c);
    if (has_subtype(p, c, subt))
      event_result |= 1;
  }
}

void protection_from_multicolored(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_misc_protection_impl(player, card);
      return;
    }
  }
  else if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
    {
      event_result = event_result | 1;
      return;
    }
    int p, c;
    /* Native non-query protection events require a nonnegative source card.
     * The original also leaves p/c unwritten when this precondition fails. */
    if (-1 < attacking_card)
    {
      get_protection_source_impl('\x01', &p, &c);
    }
    if (count_colors(p, c) > 1)
    {
      event_result = event_result | 1;
      return;
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26e0db0; original functions/abilities.c:3063. */
void protection_from_everything(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_misc_protection_impl(player, card);
      return;
    }
  }
  else if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    event_result = event_result | 1;
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26e0e10; original functions/abilities.c:3070. */
void protection_from_colored_spells(int player, int card, event_t event)
{
  int player_00;
  char protection_query;
  int attacking_source_card;
  unsigned int source_color;
  card_instance_t *instance;

  player_00 = attacking_card_controller;
  attacking_source_card = attacking_card;
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_misc_protection_impl(player, card);
      return;
    }
  }
  else if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    protection_query = (char)((unsigned int)event_result >> 0x18);
    if (protection_query == '\x06')
    {
      event_result = event_result | 1;
    }
    else
    {
      if (protection_query == '\x01')
      {

        instance = get_card_instance(attacking_card_controller, attacking_card);
        if (instance->internal_card_id != damage_card)
        {
          backtrace_and_abort("functions/abilities.c:3078\nget_card_instance(p, "
                              "c)->internal_card_id == damage_card");
        }
        source_color = get_color(player_00, attacking_source_card);
        if ((source_color & 0x3e) == 0)
        {
          return;
        }
        if (is_being_cast(player_00, attacking_source_card) == 0)
        {
          return;
        }
        instance = get_card_instance(player_00, attacking_source_card);
        if ((instance->targets[3].player & 0x47) != 0)
        {
          return;
        }
      }
      else
      {
        if (protection_query != '\x05')
        {
          return;
        }
        if (is_being_cast(attacking_card_controller, attacking_card) == 0)
        {
          return;
        }
        source_color = get_color(attacking_card_controller, attacking_card);
        if ((source_color & 0x3e) == 0)
        {
          return;
        }
      }
      event_result = event_result | 1;
    }
  }
  return;
}

#ifndef SHANDALAR
/* Recovered October 2022 ability writers and Aura pump consumers. */
static void lure_effect_part(int player, int card, event_t event, int luring_player,
                                  int luring_card, int all_must_block, int subtype, int ability,
                                  const char *prompt);
static void lure_effect(int player, int card, event_t event, int luring_player,
                             int luring_card, int all_must_block, int subtype, int ability,
                             const char *prompt);
static void deathtouch_part(int player, int card, event_t event);
static void hexproof_part(int player, int card, event_t event);
static int attack_if_able_part(int player, int card, event_t event);
static void vigilance_part(int player, int card, event_t event);
static void granted_flanking_part(int s_player, int s_card, int t_player, int t_card, event_t event);

/* ManalinkEh 2022: 0x26dd340; original functions/abilities.c:13. */
int can_use_activated_abilities(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  int mode = instance->targets[17].card;
  return mode <= 0 || !(mode & AL_NM_ACT);
}

/* ManalinkEh 2022: 0x26dd380; original functions/abilities.c:19. */
int can_use_activated_abilities_by_instance(card_instance_t *instance)
{
  int mode = instance->targets[17].card;
  return mode <= 0 || !(mode & AL_NM_ACT);
}

/* ManalinkEh 2022: 0x26dd3a0; original functions/abilities.c:24. */
int can_produce_mana(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  int mode = instance->targets[17].card;
  int result = mode <= 0 || !(mode & AL_MANA);
  if (player == AI && result)
  {
    result = !(instance->ext->special_flags & SF_MANA_PRODUCER_DISABLED_FOR_AI);
  }
  return result;
}

/* ManalinkEh 2022: 0x26dd460; original functions/abilities.c:55. */
void disable_nonmana_activated_abilities(int player, int card, int mode)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (instance->targets[0x11].card < 0)
  {
    instance->targets[0x11].card = 0;
  }
  if (mode != 1)
  {
    if (mode == 0)
    {
      instance->targets[0x11].card = instance->targets[0x11].card & 0xffffffd7;
    }
    return;
  }
  instance->targets[0x11].card = instance->targets[0x11].card | 0x28;
  return;
}

/* ManalinkEh 2022: 0x26dd4d0; original functions/abilities.c:74. */
void disable_all_activated_abilities(int player, int card, int mode)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (instance->targets[0x11].card < 0)
  {
    instance->targets[0x11].card = 0;
  }
  if (mode != 1)
  {
    if (mode == 0)
    {
      instance->targets[0x11].card = instance->targets[0x11].card & 0xffffffc7;
    }
    return;
  }
  instance->targets[0x11].card = instance->targets[0x11].card | 0x38;
  return;
}

static void disable_all_but_sp_keywords(int player, int card, int mode)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->targets[17].card < 0)
    instance->targets[17].card = 0;
  if (mode == 1)
    instance->targets[17].card |= 0x3e;
  else if (mode == 0)
    instance->targets[17].card &= ~0x3e;
}

/* ManalinkEh 2022: 0x26dd890; original functions/abilities.c:302. */
int humiliate_and_set_pt_abilities(int player, int card, int t_player, int t_card, int mode,
                                   test_definition_t *hc)
{
  int legacy_card;
  card_instance_t *instance;
  unsigned int replacement_subtype;

  if ((mode & 6U) == 0)
  {
    instance = get_card_instance(t_player, t_card);
    if (instance->targets[0x11].card < 0)
    {
      instance->targets[0x11].card = 0;
    }
    if (mode == 1)
    {
      instance->targets[0x11].card = instance->targets[0x11].card | 0x7e;
      return 1;
    }
    if (mode == 0)
    {
      instance->targets[0x11].card = instance->targets[0x11].card & 0xffffff81;
      return 0;
    }
  }
  else
  {
    legacy_card = create_targetted_legacy_effect(player, card, humility_legacy, t_player, t_card);
    instance = get_card_instance(player, legacy_card);
    instance->targets[0].player = t_player;
    instance->targets[0].card = t_card;
    instance = get_card_instance(player, legacy_card);
    if (hc == NULL)
    {
      instance->targets[1].player = -1;
      instance->targets[1].card = -1;
      instance->targets[2].player = 0;
      instance->targets[2].card = 0;
      instance->targets[3].player = mode;
      replacement_subtype = 0;
    }
    else
    {
      replacement_subtype = hc->subtype;
      instance->targets[1].player = hc->power;
      instance->targets[1].card = hc->toughness;
      instance->targets[2].player = hc->keyword;
      instance->targets[2].card = hc->color;
      instance->targets[3].player = mode;
    }
    instance->targets[3].card = replacement_subtype;
    mode = legacy_card;
  }
  return mode;
}

/* ManalinkEh 2022: 0x26dda00; original functions/abilities.c:325. */
int humiliate(int src_player, int src_card, int t_player, int t_card, int mode)
{
  int result;
  test_definition_t test;
  default_test_definition(&test, 0xff);
  test.toughness = 1;
  test.power = 1;
  result = humiliate_and_set_pt_abilities(src_player, src_card, t_player, t_card, mode, &test);
  return result;
}

/* ManalinkEh 2022: 0x26dda70; original functions/abilities.c:332. */
void remove_abilities_by_test(int player, event_t event, test_definition_t *test,
                              ability_loss_t mode)
{
  ability_loss_t existing_ability_loss;
  card_instance_t *instance;

  if ((((event == EVENT_ABILITIES) || (event == EVENT_CAST_SPELL)) &&
       (instance = in_play(affected_card_controller, affected_card), instance != NULL)) &&
      (((player == 2 || (affected_card_controller == player)) &&
        ((test == NULL ||
          (new_make_test_in_play(affected_card_controller, affected_card, -1, test) != 0))))))
  {
    instance = get_card_instance(affected_card_controller, affected_card);
    existing_ability_loss = instance->targets[0x11].card;
    if (-1 < (int)existing_ability_loss)
    {
      mode = mode | existing_ability_loss;
    }
    instance->targets[0x11].card = mode;
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26dd080; original functions/abilities.c:651. */
static void deathtouch_part(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  card_ext_t *extension = instance->ext;
  if (player_bits[player] & PB_CANT_HAVE_OR_GAIN_DEATHTOUCH)
    extension->sp_keywords &= ~SP_KEYWORD_DEATHTOUCH;
  else
    extension->sp_keywords |= SP_KEYWORD_DEATHTOUCH;
}

/* ManalinkEh 2022: 0x26de480; original functions/abilities.c:651. */
void deathtouch(int player, int card, event_t event)
{
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    deathtouch_part(player, card, EVENT_ABILITIES);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26dd0c0; original functions/abilities.c:653. */
static void hexproof_part(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  card_ext_t *extension = instance->ext;
  if (player_bits[player] & PB_CANT_HAVE_OR_GAIN_HEXPROOF)
    extension->sp_keywords &= ~SP_KEYWORD_HEXPROOF;
  else
    extension->sp_keywords |= SP_KEYWORD_HEXPROOF;
}

/* ManalinkEh 2022: 0x26de500; original functions/abilities.c:653. */
void hexproof(int player, int card, event_t event)
{
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    hexproof_part(player, card, EVENT_ABILITIES);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26dd110; original functions/abilities.c:655. */
static void indestructible_part(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  instance->ext->sp_keywords |= SP_KEYWORD_INDESTRUCTIBLE;
}

/* ManalinkEh 2022: 0x26de580; original functions/abilities.c:655. */
void indestructible(int player, int card, event_t event)
{
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    indestructible_part(player, card);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26dd150; original functions/abilities.c:803. */
static int attack_if_able_part(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (!(instance->state & STATE_UNKNOWN8000))
  {
    instance->state |= STATE_UNKNOWN8000;
    if (can_attack(player, card))
      must_attack = 1;
  }
  return player;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
void ability_must_block_me(int player, int card, event_t event, int ability, char *prompt);

/* ManalinkEh 2022: 0x26dedc0; original functions/abilities.c:803. */
int attack_if_able(int player, int card, event_t event)
{
  if (((event == EVENT_BEGIN_COMBAT) && (current_turn == player)) && (forbid_attack == 0))
  {
    attack_if_able_part(player, card, EVENT_MUST_ATTACK);
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 pump effects and cleanup helpers. */

/* ManalinkEh 2022: 0x26def40; original functions/abilities.c:861. */
int block_if_able(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  unsigned int keyword = instance->ext->sp_keywords;
  if (!(keyword & SP_KEYWORD_MUST_BLOCK))
    instance->ext->sp_keywords = keyword | SP_KEYWORD_MUST_BLOCK;
  if (current_turn != player && event == EVENT_DECLARE_BLOCKERS)
    select_blocker(player, card, player);
  return 0;
}

/* ManalinkEh 2022: 0x26dcca0; original functions/abilities.c:881. */
static void lure_effect_part(int player, int card, event_t event, int luring_player,
                             int luring_card, int all_must_block, int subtype, int ability,
                             const char *prompt)
{
  uint8_t saved_target_count;
  card_instance_t *luring_creature_or_source;
  card_instance_t *effect_source;
  unsigned int source_csv_id;
  int chooser_or_result;
  target_t tgt;
  target_definition_t td;
  char fullprompt[200];

  if (current_turn == luring_player)
  {
    chooser_or_result = luring_player;
    if (((unsigned char)event_flags & 8) == 0)
    {
      chooser_or_result = 1 - luring_player;
    }
    if (((reason_for_trigger_controller == chooser_or_result) &&
         (luring_creature_or_source = in_play(luring_player, luring_card),
          luring_creature_or_source != NULL)) &&
        ((luring_creature_or_source->state & 4) != 0))
    {
      if ((all_must_block == 0) && (is_unblocked(luring_player, luring_card) == 0))
      {
        return;
      }
      default_target_definition(luring_player, luring_card, &td, 0);
      td.allowed_controller = 1 - luring_player;
      if (subtype < 0)
      {
        subtype = -1;
      }
      td.required_abilities = (int)(ability ^ 0xffffffffU) >> 0x1f & ability;
      td.illegal_abilities = 0;
      td.special = TARGET_SPECIAL_ALLOW_MULTIBLOCKER | TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (intptr_t)target_must_be_able_to_block_me;
      td.who_chooses = chooser_or_result;
      td.preferred_controller = td.allowed_controller;
      td.required_subtype = subtype;
      if (can_target(&td) != 0)
      {
        if (event == EVENT_TRIGGER)
        {
          event_result = event_result | 2;
        }
        else if (event == EVENT_RESOLVE_TRIGGER)
        {
          if (ai_is_speculating == 1)
          {
            fullprompt[0] = '\0';
          }
          else
          {
            if ((prompt == NULL) || (*prompt == '\0'))
            {
              load_text(NULL, "PROMPT_CHOOSEBLOCKERS");
              prompt = text_lines[0];
            }
            if (is_what(player, card, 0x80) == 0)
            {
              source_csv_id = get_id(player, card);
            }
            else
            {
              effect_source = get_card_instance(player, card);
              source_csv_id = (unsigned int)effect_source->display_pic_csv_id;
            }
            scnprintf(fullprompt, 200, "%s: %s", cards_ptr[source_csv_id]->full_name, prompt);
          }
          trigger_condition = 0xffffffff;
          saved_target_count = luring_creature_or_source->number_of_targets;
          chooser_or_result = select_target(luring_player, luring_card, &td, fullprompt, &tgt);
          trigger_condition = 0xda;
          luring_creature_or_source->number_of_targets = saved_target_count;
          if (chooser_or_result != 0)
          {
            block(tgt.player, tgt.card, luring_player, luring_card);
            if (all_must_block == 0)
            {
              return;
            }
            if (can_target(&td) == 0)
            {
              return;
            }
          }
          luring_creature_or_source = get_card_instance(player, card);
          luring_creature_or_source->state = luring_creature_or_source->state & 0xfffffeff;
        }
      }
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26dcfb0; original functions/abilities.c:881. */
static void lure_effect(int player, int card, event_t event, int luring_player,
                             int luring_card, int all_must_block, int subtype, int ability,
                             const char *prompt)
{
  if (trigger_condition != TRIGGER_MUST_BLOCK)
  {
    return;
  }
  if ((((uint32_t)event - (uint32_t)EVENT_TRIGGER < 2U) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    lure_effect_part(player, card, event, luring_player, luring_card, all_must_block, subtype,
                          ability, prompt);
    return;
  }
  return;
}

/* ManalinkEh 2022: 0x26df030; original functions/abilities.c:964. */
void ability_must_block_me(int player, int card, event_t event, int ability, char *prompt)
{
  lure_effect(player, card, event, player, card, 1, -1, ability, prompt);
  return;
}

/* ManalinkEh 2022: 0x26df8e0; original functions/abilities.c:1054. */
static void granted_flanking_part(int s_player, int s_card, int t_player, int t_card, event_t event)
{
  int result;
  int card;
  char marked[2][151];
  result = check_for_special_ability(t_player, t_card, 0x2000);
  if (result != 0)
  {
    memset(marked, 0, sizeof(marked));
    mark_each_creature_blocking_me(t_player, t_card, marked);
    result = 1 - current_turn;
    if (0 < active_cards_count[result])
    {
      card = 0;
      do
      {
        if (marked[result][card] != '\0')
        {
          result = check_for_special_ability(result, card, 0x2000);
          if (result == 0)
          {
            result = pump_until_eot(s_player, s_card, 1 - current_turn, card, -1, -1);
            alternate_legacy_text(1, s_player, result);
          }
        }
        result = 1 - current_turn;
        card = card + 1;
      } while (card < active_cards_count[result]);
    }
  }
  return;
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 activated-ability core. */

/* ManalinkEh 2022: 0x26dfa50; original functions/abilities.c:1054. */
void granted_flanking(int s_player, int s_card, int t_player, int t_card, event_t event)
{
  int result;
  card_instance_t *instance;
  if (event == EVENT_ABILITIES)
  {
    if ((t_card == affected_card) && (t_player == affected_card_controller))
    {
      result = check_for_flanking_removal(t_player, t_card);
      if (result == 0)
      {
        instance = get_card_instance(t_player, t_card);
        instance->ext->sp_keywords = instance->ext->sp_keywords | 0x2000;
      }
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_attacking(t_player, t_card);
    if (result != 0)
    {
      granted_flanking_part(s_player, s_card, t_player, t_card, EVENT_DECLARE_BLOCKERS);
      return;
    }
  }
  return;
}

/* ManalinkEh 2022: 0x26dd1a0; original functions/abilities.c:1140. */
static void vigilance_part(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  instance->ext->sp_keywords |= SP_KEYWORD_VIGILANCE;
  if (player != -1 && card != -1 && is_humiliated(player, card, AL_KEY))
    return;
  instance->state |= STATE_VIGILANCE;
}

/* ManalinkEh 2022: 0x26dfaf0; original functions/abilities.c:519. */
void special_abilities(int player, int card, event_t event, int sp_abilities,
                            int source_player, int source_card)
{
  card_instance_t *instance;
  card_instance_t *target;

  instance = get_card_instance(player, card);
  if ((sp_abilities & SP_KEYWORD_FEAR) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_FEAR;
  }
  if ((sp_abilities & SP_KEYWORD_LIFELINK) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_LIFELINK;
  }
  if ((sp_abilities & SP_KEYWORD_HASTE) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_HASTE;
  }
  if ((sp_abilities & SP_KEYWORD_VIGILANCE) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    vigilance_part(player, card, EVENT_ABILITIES);
  }
  if ((sp_abilities & SP_KEYWORD_UNBLOCKABLE) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_UNBLOCKABLE;
  }
  if ((sp_abilities & SP_KEYWORD_CANNOT_BLOCK) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_CANNOT_BLOCK;
  }
  if ((sp_abilities & SP_KEYWORD_SHADOW) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_SHADOW;
  }
  if ((sp_abilities & SP_KEYWORD_DEATHTOUCH) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    deathtouch_part(player, card, EVENT_ABILITIES);
  }
  if ((sp_abilities & SP_KEYWORD_MUST_ATTACK) && event == EVENT_BEGIN_COMBAT &&
      player == current_turn && forbid_attack == 0)
  {
    attack_if_able_part(player, card, EVENT_MUST_ATTACK);
  }
  if ((sp_abilities & SP_KEYWORD_INDESTRUCTIBLE) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    indestructible_part(player, card);
  }
  if (sp_abilities & SP_KEYWORD_MUST_BE_BLOCKED)
  {
    lure_effect(source_player, source_card, event, player, card, 0, -1, 0, NULL);
  }
  if (sp_abilities & SP_KEYWORD_FLANKING)
  {
    granted_flanking(source_player, source_card, player, card, event);
  }
  if ((sp_abilities & SP_KEYWORD_WITHER) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_WITHER;
  }
  if (sp_abilities & SP_KEYWORD_LURE)
  {
    lure_effect(source_player, source_card, event, player, card, 1, -1, 0, NULL);
  }
  if ((sp_abilities & SP_KEYWORD_HEXPROOF) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    hexproof_part(player, card, EVENT_ABILITIES);
  }
  if ((sp_abilities & SP_KEYWORD_INTIMIDATE) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    target = get_card_instance(player, card);
    target->ext->sp_keywords |= SP_KEYWORD_INTIMIDATE;
  }
  if (sp_abilities & SP_KEYWORD_MUST_BLOCK)
  {
    block_if_able(player, card, event);
  }
  if (sp_abilities & SP_KEYWORD_SHADOW_HOSER)
  {
    instance->ext->sp_keywords |= SP_KEYWORD_SHADOW_HOSER;
  }
  if ((sp_abilities & SP_KEYWORD_MENACE) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords |= SP_KEYWORD_MENACE;
  }
  if ((sp_abilities & SP_KEYWORD_MINBLOCKERS_3) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords |= SP_KEYWORD_MINBLOCKERS_3;
  }
  if ((sp_abilities & SP_KEYWORD_MAXBLOCKERS_1) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords |= SP_KEYWORD_MAXBLOCKERS_1;
  }
  if ((sp_abilities & SP_KEYWORD_SKULK) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords |= SP_KEYWORD_SKULK;
  }
  if ((sp_abilities & SP_KEYWORD_SQUIRRELLINK) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords |= SP_KEYWORD_SQUIRRELLINK;
  }
  if ((sp_abilities & SP_KEYWORD_HORSEMANSHIP) && event == EVENT_ABILITIES &&
      card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    instance->ext->sp_keywords |= SP_KEYWORD_HORSEMANSHIP;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e0300; original functions/abilities.c:229. */
int humility_legacy(int player, int card, event_t event)
{
  int target_card;
  unsigned int mode_or_keywords;
  subtype_t subtype;
  int sp_abilities;
  unsigned int existing_ability_loss;
  card_instance_t *instance;
  unsigned int ability_loss;
  int result;

  if (2 < (unsigned int)player)
  {
    return 0;
  }
  if (0x96 < (unsigned int)card)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  result = (int)instance->damage_target_player;
  if (result < 0)
  {
    return 0;
  }
  target_card = instance->damage_target_card;
  if (target_card == affected_card && result == affected_card_controller)
  {
    switch (event)
    {
    case EVENT_POWER:
      result = instance->targets[1].player;
      if (-1 < result)
        set_base_pt(result);
      return 0;
    case EVENT_TOUGHNESS:
      result = instance->targets[1].card;
      if (-1 < result)
      {
        set_base_pt(result);
      }
      return 0;
    case EVENT_ABILITIES:
      mode_or_keywords = instance->targets[2].player;
      sp_abilities = instance->targets[4].player;
      instance = get_card_instance(result, target_card);
      ability_loss = 0x3e;
      existing_ability_loss = instance->targets[0x11].card;
      if (-1 < (int)existing_ability_loss)
      {
        ability_loss = existing_ability_loss | 0x3e;
      }
      instance->targets[0x11].card = ability_loss;
      instance = get_card_instance(result, target_card);
      instance->state = instance->state & 0xffffdfff;
      instance->ext->sp_keywords = 0;
      if (0 < sp_abilities)
      {
        special_abilities(result, target_card, EVENT_ABILITIES, sp_abilities, player, card);
      }
      event_result = event_result & 0x9f080000U | mode_or_keywords | 0x1f000000;
      return 0;
    default:
      break;
    case EVENT_SET_COLOR:
      result = instance->targets[2].card;
      if (result != 0)
      {
        event_result = result;
      }
      return 0;
    case EVENT_CHANGE_TYPE:
      subtype = instance->targets[3].card;
      if (subtype != SUBTYPE_INVALID)
      {
        force_a_subtype(result, target_card, subtype);
      }
      return 0;
    }
  }
  if ((event != EVENT_CLEANUP) && (event != EVENT_SHOULD_AI_PLAY))
  {
    return 0;
  }
  mode_or_keywords = instance->targets[3].player;
  if ((int)mode_or_keywords < 0)
  {
    return 0;
  }
  if ((mode_or_keywords & 4) == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26df490; original functions/abilities.c:1140. */
void vigilance(int player, int card, event_t event)
{
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    vigilance_part(player, card, EVENT_ABILITIES);
    return;
  }
  return;
}

#endif

#ifndef SHANDALAR
static int has_sp_key_by_iid(int iid, int sp_key)
{
  return has_sp_key_by_csvid_impl(cards_data[iid].id, sp_key);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e09d0; original functions/abilities.c:2981. */
void protection_from_type(int player, int card, event_t event, type_t type)
{
  if ((type & (TYPE_ARTIFACT | TYPE_INSTANT | TYPE_SORCERY | TYPE_CREATURE | TYPE_LAND)) !=
      TYPE_NONE)
  {
    backtrace_and_abort("functions/abilities.c:2983\n!(type & (TYPE_ARTIFACT | TYPE_INSTANT | "
                        "TYPE_SORCERY | TYPE_CREATURE | TYPE_LAND))");
  }
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_misc_protection_impl(player, card);
      return;
    }
  }
  else if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
    {
      event_result = event_result | 1;
      return;
    }
    int p, c;
    get_protection_source(1, &p, &c);
    card_instance_t *inst = get_card_instance(p, c);
    if (inst->internal_card_id == damage_card)
    {
      if ((type & inst->targets[3].player) == TYPE_NONE)
      {
        return;
      }
    }
    else
    {
      if (!is_what(p, c, type))
      {
        return;
      }
    }
    event_result = event_result | 1;
    return;
  }
  return;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26e0b90; original functions/abilities.c:3018. */
void protection_from_colorless(int player, int card, event_t event)
{
  unsigned int source_color;
  int p;
  int c;
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_misc_protection_impl(player, card);
      return;
    }
  }
  else if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
    {
      event_result = event_result | 1;
      return;
    }
    get_protection_source(1, &p, &c);
    source_color = get_color(p, c);
    if ((source_color & 0x3e) == 0)
    {
      event_result = event_result | 1;
      return;
    }
  }
  return;
}

#endif

/* Native colored protection contributes protection bits during EVENT_ABILITIES. */

int generic_protection_from(int player, int card, event_t event, color_test_t cols)
{
  if (event == EVENT_ABILITIES && affect_me(player, card))
  {
    int old_event_result = event_result;
    cols = get_sleighted_color_test(player, card, cols & COLOR_TEST_ANY_COLORED) |
           (cols & COLOR_TEST_ARTIFACT);
    event_result = old_event_result | (cols << 10); // COLOR_TEST_BLACK => KEYWORD_PROT_BLACK, etc.
  }

  return 0;
}

int protection_from_black(int player, int card, event_t event){
	return generic_protection_from(player, card, event, COLOR_TEST_BLACK);
}

int protection_from_red(int player, int card, event_t event){
	return generic_protection_from(player, card, event, COLOR_TEST_RED);
}

int protection_from_blue(int player, int card, event_t event){
	return generic_protection_from(player, card, event, COLOR_TEST_BLUE);
}

int protection_from_green(int player, int card, event_t event){
	return generic_protection_from(player, card, event, COLOR_TEST_GREEN);
}

int protection_from_white(int player, int card, event_t event){
	return generic_protection_from(player, card, event, COLOR_TEST_WHITE);
}

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
/* ManalinkEh 2022: 0x26e0d00; original functions/abilities.c:3048. */
void protection_from_monocolored(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_misc_protection_impl(player, card);
      return;
    }
  }
  else if (((event == EVENT_PROT_MISC) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    if (((unsigned int)event_result >> 24 & 0xff) == '\x06')
    {
      event_result = event_result | 1;
      return;
    }
    int p, c;
    get_protection_source(1, &p, &c);
    if (count_colors(p, c) == 1)
    {
      event_result = event_result | 1;
      return;
    }
  }
  return;
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 private inline definitions. See per-helper limits in the recovery evidence. */
/* ManalinkEh 2022: 0x26e0f50; original functions/abilities.c:3089. */
void cannot_be_target_of_abilities(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      set_misc_protection_impl(player, card);
      return;
    }
  }
  else if ((((event == EVENT_PROT_MISC) && (card == affected_card)) &&
            (player == affected_card_controller)) &&
           (((unsigned int)event_result >> 24 & 0xff) == '\x05'))
  {
    int p, c;
    get_protection_source(1, &p, &c);
    if (is_being_cast(p, c) == 0)
    {
      event_result = event_result | 1;
      return;
    }
  }
  return;
}
#endif
