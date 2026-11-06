#include <limits.h>
#include "manalink.h"

// Functions

/*
Face the Hydra
Lands
0

Creatures
11 Hydra Head
4 Ravenous Brute Head
1 Savage Vigor Head
1 Snapping Fang Head
1 Shrieking Titan Head

Sorceries
5 Disorienting Glower
5 Distract the Hydra
4 Grown from the Stump
4 Hydra's Impenetrable Hide
3 Neck Tangle
4 Noxious Hydra Breath
2 Strike the Weak Spot
5 Swallow the Hero Whole
4 Torn Between Heads
6 Unified Lunge

How to Play

You can battle the Hydra alone or with friends (just replace "you" with "each player" in these rules).
At the end of any turn, if there are no Heads on the battlefield, you win! Use the regular Magic rules with the following exceptions:

    You start with up to two Hero cards on the battlefield. (You don't need a Hero to play.)
    Choose a starting number of Heads. Take that many cards named Hydra Head from the Challenge Deck and place them on the battlefield.
		Shuffle the remaining cards to form the Hydra's library.
    You go first. (Don't draw a card on your first turn.)
    Then the Hydra takes it's turn, and so on.
    You can attack Heads directly with your creatures. Any number of creatures can attack a single Head
    Growing New Heads: Whenever a Head leaves the battlefield, reveal the top two cards of the Hydra's library.
		Put any Heads onto the battlefield and any sorcery cards into the Hydra's graveyard.

The Hydra's Turn

    At the start of the Hydra's turn, untap any tapped Heads.
    Reveal the top card of the Hydra's library. The Hydra casts that card.
		When the spell resolves, if it's a Head, put it onto the battlefield.
		If it's a sorcery, follow its instructions and then put it into the Hydra's graveyard.
    The Hydra deals 1 damage to you for each untapped card named Hydra Head it controls and 2 damage to you for each untapped elite Head it controls.

Special Rules

    If the Hydra would be dealt damage or lose life, instead deal that much damage to a Head of your choice.
    Ignore effects that would cause the Hydra to draw or discard cards, or any impossible actions.
    If a Head would move to any other zone than the graveyard, instead put it into the Hydra's graveyard.
    You make any choices for the Hydra.
*/

int new_creature_with_targeted_bestow(int player, int card, event_t event, target_definition_t *td,
                                      int colorless, int black, int blue, int green, int red,
                                      int white, int pow, int tou, int key, int s_key);

static void bestow(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_MODIFY_COST ){
		int cless = get_updated_casting_cost(player, -1, get_internal_card_id_from_csv_id(get_id(player, card) + 1), event, colorless);
		if( has_mana_multi(player, cless, black, blue, green, red, white) && can_target(&td) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		} else {
			instance->info_slot = 0;
		}
	}
}

static int pay_bestow(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white){

	card_instance_t *instance = get_card_instance(player, card);

	if( played_for_free(player, card) || is_token(player, card) ){
		return 0;
	}
	int double_x = 0;
	if( colorless == -2 ){
		double_x = 1;
		colorless = 0;
	}
	int choice = 0;
	int cless = get_updated_casting_cost(player, -1, get_internal_card_id_from_csv_id(get_id(player, card) + 1), event, colorless);
	if( has_mana_multi(player, cless, black, blue, green, red, white) ){
		if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
			choice = do_dialog(player, player, card, -1, -1, " Pay Bestow cost\n Play the spell normally\n Cancel", 0);
		}
	}
	else{
		choice = 1;
	}
	if( choice == 0 ){
		charge_mana_multi(player, cless, black, blue, green, red, white);
		if( spell_fizzled != 1 ){
			if( double_x ){
				instance->targets[1].player = charge_mana_for_double_x(player, COLOR_COLORLESS);
				if( spell_fizzled == 1 ){
					return 0;
				}
			}
			return 2;
		}
	}
	else if( choice == 1){
			if( charge_mana_from_id(player, card, event, get_id(player, card)) ){
				return 1;
			}
	}
	else{
		spell_fizzled = 1;
	}
	return 0;
}

int creature_with_targeted_bestow(int player, int card, event_t event, target_definition_t *td,
                                  int colorless, int black, int blue, int green, int red, int white,
                                  int pow, int tou, int key, int s_key)
{
  card_instance_t *instance = get_card_instance(player, card);

  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }

  bestow(player, card, event, colorless, black, blue, green, red, white);

  if (event == EVENT_CAN_SKIP_TURN)
  {
    instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE | KEYWORD_RECALC_ABILITIES;
    get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
    get_abilities(player, card, EVENT_ABILITIES, -1);
  }

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && instance->targets[12].card != -1)
  {
    event_result = instance->targets[12].card;
  }

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    instance->targets[13].player = get_id(player, card);
    instance->targets[13].card = get_id(player, card);
    int result = 0;
    if (instance->info_slot == 1)
    {
      result = pay_bestow(player, card, event, colorless, black, blue, green, red, white);
    }
    else
    {
      result = 1;
    }
    if (result == 0)
    {
      spell_fizzled = 1;
    }
    else if (result == 2)
    {
      if (pick_target(td, "TARGET_CREATURE"))
      {
        instance->number_of_targets = 1;
        instance->info_slot = 66;
        true_transform(player, card);
        instance->backup_internal_card_id = instance->internal_card_id =
            instance->targets[12]
                .card; // Since it won't get EVENT_CHANGE_TYPE events while on the stack
      }
    }
  }

  if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->number_of_targets > 0)
    {
      if (valid_target(td))
      {
        instance->damage_target_player = instance->targets[0].player;
        instance->damage_target_card = instance->targets[0].card;
      }
      else
      {
        true_transform(player, card);
      }
    }
  }

  if (in_play(player, card) && instance->damage_target_player > -1)
  {
    modify_pt_and_abilities(instance->damage_target_player, instance->damage_target_card, event,
                            pow, tou, key);
    special_abilities(instance->damage_target_player, instance->damage_target_card, event, s_key,
                      player, card);
    other_leaves_play(player, card, instance->damage_target_player, instance->damage_target_card,
                      event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY && instance->info_slot == 66)
    {
      instance->info_slot = 2;
      instance->damage_target_player = instance->damage_target_card = -1;
      true_transform(player, card);
    }
  }

  return 0;
}

int generic_creature_with_bestow(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white,
								int pow, int tou, int key, int s_key
  ){

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return creature_with_targeted_bestow(player, card, event, &td, colorless, black, blue, green, red, white, pow, tou, key, s_key);
}

/* ManalinkEh 2022: 0x2630ea0; original cards/theros.c:150. */
int new_generic_creature_with_bestow(int player, int card, event_t event, int colorless, int black,
                                     int blue, int green, int red, int white, int pow, int tou,
                                     int key, int s_key)
{
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  result = new_creature_with_targeted_bestow(player, card, event, &td, colorless, black, blue,
                                             green, red, white, pow, tou, key, s_key);
  return result;
}

/* ManalinkEh 2022: 0x2630a30; original cards/theros.c:161. */
int new_creature_with_targeted_bestow(int player, int card, event_t event, target_definition_t *td,
                                      int colorless, int black, int blue, int green, int red,
                                      int white, int pow, int tou, int key, int s_key)
{
  unsigned short aura_csv_id;
  unsigned int ability_recheck_flags;
  card_instance_t *instance;
  card_instance_t *aura_in_play;
  int card_id_or_bestow_result;
  int original_csv_id;
  unsigned char card_type;
  int aura_iid;
  bool apply_attachment = false;

  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }
  bestow(player, card, event, colorless, black, blue, green, red, white);
  if (event == EVENT_CAN_SKIP_TURN)
  {
    instance->regen_status = instance->regen_status | 0x9000000;
    get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
    get_abilities(player, card, EVENT_ABILITIES, -1);
  }
  else if (event == EVENT_CHANGE_TYPE)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (instance->info_slot == 0x42))
    {

      card_id_or_bestow_result = instance->internal_card_id;
      card_type = cards_data[card_id_or_bestow_result].type & 0xfd;
      if ((cards_data[card_id_or_bestow_result].type & 2) == 0)
      {
        return 0;
      }
      if ((instance->targets[0xd].player != card_id_or_bestow_result) ||
          (aura_iid = instance->dummy3, cards_data[aura_iid].type != card_type))
      {

        aura_iid = create_a_card_type(card_id_or_bestow_result);
        if (aura_iid == -1)
        {
          return 0;
        }
        aura_csv_id = cards_data[aura_iid].id;
        cards_data[aura_iid].type = card_type;
        cards_ptr[aura_csv_id]->subtype1 = 0x4000;
        instance->dummy3 = aura_iid;
        ability_recheck_flags = instance->regen_status;
        instance->targets[0xd].player = card_id_or_bestow_result;
        instance->regen_status = ability_recheck_flags | 0x1000000;
      }
      event_result = aura_iid;
    }
    apply_attachment = (uint32_t)(event - EVENT_POWER) < 3;
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      card_id_or_bestow_result = get_id(player, card);
      instance->targets[0xd].player = card_id_or_bestow_result;
      original_csv_id = get_id(player, card);
      card_id_or_bestow_result = instance->info_slot;
      instance->targets[0xd].card = original_csv_id;
      if (card_id_or_bestow_result == 1)
      {
        card_id_or_bestow_result =
            pay_bestow(player, card, EVENT_CAST_SPELL, colorless, black, blue, green, red, white);
        if (card_id_or_bestow_result == 0)
        {
          spell_fizzled = 1;
        }
        else if ((card_id_or_bestow_result == 2) &&
                 (card_id_or_bestow_result = pick_target(td, "TARGET_CREATURE"),
                  card_id_or_bestow_result != 0))
        {

          card_id_or_bestow_result = instance->internal_card_id;
          instance->number_of_targets = '\x01';
          instance->info_slot = 0x42;
          original_csv_id = get_id(player, card);
          cards_data[instance->internal_card_id].rarity =
              (uint8_t)cards_ptr[original_csv_id]->subtype2;
          card_type = cards_data[card_id_or_bestow_result].type;
          card_id_or_bestow_result = create_a_card_type(card_id_or_bestow_result);
          if (card_id_or_bestow_result == -1)
          {
            return 0;
          }
          cards_data[card_id_or_bestow_result].type = card_type & 0xfd;
          instance->targets[0xd].player = card_id_or_bestow_result;
          instance->regen_status = instance->regen_status | 0x1000000;
          instance->dummy3 = card_id_or_bestow_result;
          instance->internal_card_id = card_id_or_bestow_result;
          instance->backup_internal_card_id = (uint16_t)card_id_or_bestow_result;
          add_a_subtype_permanently(player, card, player, card, SUBTYPE_AURA_CREATURE);
        }
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    if (instance->number_of_targets != '\0')
    {
      if (valid_target(td) == 0)
      {
        card_id_or_bestow_result = instance->targets[0xd].card;
        if (card_id_or_bestow_result != -1)
        {
          instance->targets[0xd].player = card_id_or_bestow_result;
          instance->internal_card_id = card_id_or_bestow_result;
          instance->backup_internal_card_id = (uint16_t)card_id_or_bestow_result;
        }
      }
      else
      {
        instance->damage_target_player = (int8_t)instance->targets[0].player;
        instance->damage_target_card = instance->targets[0].card;
      }
    }
  }
  else
    apply_attachment = (uint32_t)(event - EVENT_POWER) < 3;
  if (!apply_attachment && trigger_condition != TRIGGER_LEAVE_PLAY)
  {
    return 0;
  }
  aura_in_play = in_play(player, card);
  if ((aura_in_play != NULL) && (-1 < instance->damage_target_player))
  {
    modify_pt_and_abilities((int)instance->damage_target_player, instance->damage_target_card,
                            event, pow, tou, key);
    special_abilities((int)instance->damage_target_player, instance->damage_target_card, event,
                      s_key, player, card);
    other_leaves_play(player, card, (int)instance->damage_target_player,
                      instance->damage_target_card, event, 0, 0);
    if ((event == EVENT_LEAVES_PLAY_ABILITY) && (instance->info_slot == 0x42))
    {
      card_id_or_bestow_result = instance->targets[0xd].card;
      instance->info_slot = 2;
      instance->damage_target_card = -1;
      instance->damage_target_player = -1;
      if (card_id_or_bestow_result != -1)
      {
        instance->internal_card_id = card_id_or_bestow_result;
        instance->backup_internal_card_id = (uint16_t)card_id_or_bestow_result;
      }
    }
  }
  return 0;
}

// Same as heroic, but not necessarily RESOLVE_TRIGGER_MANDATORY.

int heroic_mode(int player, int card, event_t event, resolve_trigger_t trigger_mode){

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		const target_t* tgt;
		if ((tgt = becomes_target_of_spell(player, card, event, player, card, player, trigger_mode))){
			// (Does anything with heroic actually rely on targets[1] being set like specific_cip() does?)
			get_card_instance(player, card)->targets[1] = *tgt;	// struct copy
			return 1;
		}
	}

	return 0;
}

int heroic(int player, int card, event_t event){
	return heroic_mode(player, card, event, RESOLVE_TRIGGER_MANDATORY);
}

// Exactly the same as chroma(player, -1, clr1, clr2), except sleights clr1 and clr2 unless card is -1.

int devotion(int player, int card, int clr1, int clr2)
{
  if (card >= 0)
	{
	  if (clr1)
		clr1 = get_sleighted_color(player, card, clr1);

	  if (clr2)
		clr2 = get_sleighted_color(player, card, clr2);
	}

  return chroma(player, -1, clr1, clr2);
}

void generic_creature_with_devotion(int player, int card, event_t event, int dev_color1, int dev_color2, int amount){

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_SKIP_TURN){
		instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE|KEYWORD_RECALC_ABILITIES;
		get_abilities(player, card, EVENT_CHANGE_TYPE, -1);
		get_abilities(player, card, EVENT_ABILITIES, -1);
	}

	if (event == EVENT_RESOLVE_SPELL && instance->targets[12].card != -1){
		/* During EVENT_RESOLVE_SPELL, STATE_INVISIBLE will still be set if this was put into play rather than cast, which would prevent devotion() from
		 * counting this card; so temporarily remove */
		int was_invisible = instance->state & STATE_INVISIBLE;
		int dev = devotion(player, card, dev_color1, dev_color2);
		instance->state |= was_invisible;
		if (dev >= amount && instance->internal_card_id != instance->targets[12].card){
			/* We can only possibly get away with changing internal_card_id during EVENT_RESOLVE_SPELL because all gods have the same function for both forms.
			 * It might still break things; cards shouldn't change type except during EVENT_CHANGE_TYPE. */
			instance->backup_internal_card_id = instance->internal_card_id = instance->targets[12].card;
			instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE|KEYWORD_RECALC_ABILITIES;
			if (!was_invisible){
				// Was cast.  Play the creature sound in addition to the enchantment sound already playing.
				play_sound_effect(WAV_SUMMON);
			}
		}
	}

	if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && in_play(player, card) && instance->targets[12].card != -1){
		int dev = devotion(player, card, dev_color1, dev_color2);
		if (dev >= amount && instance->internal_card_id != instance->targets[12].card){
			event_result = instance->targets[12].card;
			instance->regen_status |= KEYWORD_RECALC_CHANGE_TYPE|KEYWORD_RECALC_ABILITIES;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->targets[13].player = get_id(player, card);
		instance->targets[13].card = instance->targets[13].player+1;
		instance->targets[12].card = get_internal_card_id_from_csv_id(get_id(player, card)+1);
	}
}

int is_monstrous(int player, int card){
	if( check_special_flags2(player, card, SF2_MONSTROUS) ){
		return 1;
	}
	return 0;
}

int monstrosity(int player, int card, event_t event, int colorless, int black, int blue, int green,
                int red, int white, int c_amount)
{
  int p;
  int ability_flags_or_cost;
  card_instance_t *instance;
  int parent_card_or_available_mana;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (check_special_flags2(player, card, 0x800) != 0)
      return 0;
  }
  else if (event != EVENT_ACTIVATE)
  {
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      p = instance->parent_controller;
      parent_card_or_available_mana = instance->parent_card;
      instance = in_play(p, parent_card_or_available_mana);
      if ((instance != NULL) &&
          (check_special_flags2(p, parent_card_or_available_mana, 0x800) == 0))
      {
        set_special_flags2(p, parent_card_or_available_mana, 0x800);
        add_counters(player, card, p, parent_card_or_available_mana, COUNTER_P1_P1, c_amount);
        call_card_function(p, parent_card_or_available_mana, EVENT_BECAME_MONSTROUS);
      }
    }
    else if (((uint32_t)(event - EVENT_POW_BOOST) < 2) &&
             (ability_flags_or_cost = check_special_flags2(player, card, 0x800),
              ability_flags_or_cost == 0))
    {
      if (colorless == -2)
      {
        ability_flags_or_cost =
            get_cost_mod_for_activated_abilities(player, card, 0, black, blue, green, red, white);
        if (has_mana_multi(player, ability_flags_or_cost, black, blue, green, red, white) != 0)
        {
          parent_card_or_available_mana = has_mana(player, 7, -1);
          parent_card_or_available_mana =
              parent_card_or_available_mana -
              (black + ability_flags_or_cost + blue + green + red + white);
          if (1 < parent_card_or_available_mana)
          {
            return parent_card_or_available_mana / 2;
          }
        }
      }
      else if (colorless == -1)
      {
        ability_flags_or_cost =
            get_cost_mod_for_activated_abilities(player, card, 0, black, blue, green, red, white);
        if (has_mana_multi(player, ability_flags_or_cost, black, blue, green, red, white) != 0)
        {
          parent_card_or_available_mana = has_mana(player, 7, -1);
          return parent_card_or_available_mana -
                 (black + ability_flags_or_cost + blue + green + red + white);
        }
      }
      else
      {
        ability_flags_or_cost = can_use_activated_abilities(player, card);
        if ((ability_flags_or_cost != 0) &&
            (ability_flags_or_cost = has_mana_for_activated_ability(player, card, colorless, black,
                                                                    blue, green, red, white),
             ability_flags_or_cost != 0))
        {
          return c_amount;
        }
      }
    }
    return 0;
  }
  if (ai_is_speculating == 1)
  {
    ability_flags_or_cost = 0x2000;
  }
  else
  {
    ability_flags_or_cost = 0;
    if (player == 1)
    {
      ability_flags_or_cost = (unsigned int)(((unsigned char)trace_mode & 2) == 0) << 0xd;
    }
  }
  return generic_activated_ability(player, card, event, ability_flags_or_cost, colorless, black,
                                   blue, green, red, white, 0, NULL, NULL);
}

static int ordeal(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0)
  {
    instance = get_card_instance(player, card);
    store_attackers(player, card, event, 0, instance->damage_target_player,
                    instance->damage_target_card, NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      add_counters(player, card, instance->damage_target_player, instance->damage_target_card,
                   COUNTER_P1_P1, 1);
      if (count_1_1_counters(instance->damage_target_player, instance->damage_target_card) >= 3)
        kill_card(player, card, KILL_SACRIFICE);
    }
  }

  // When you sacrifice ~, [...]
  if (event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(player, card) &&
      (instance = in_play(player, card)) && instance->kill_code == KILL_SACRIFICE &&
      !check_special_flags(affected_card_controller, affected_card, SF_KILL_STATE_BASED_ACTION))
    instance->targets[11].player = 66;

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS)
    return vanilla_aura(player, card, event, player);
  return 0;
}

// Cards

// White

int card_battlewise_valor(int player, int card, event_t event){
	/* Battlewise Valor	|1|W
	 * Instant
	 * Target creature gets +2/+2 until end of turn. Scry 1. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 2, 2, VANILLA_PUMP_SCRY_1, 0);
}

int card_cavalry_pegasus(int player, int card, event_t event)
{
/* CARD_ID_CAVALRY_PEGASUS	13400
Cavalry Pegasus	|1|W
Creature - Pegasus 1/1
Flying
Whenever ~ attacks, each attacking Human gains flying until end of turn. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  test_definition_t this_test;
	  default_test_definition(&this_test, TYPE_CREATURE);
	  this_test.subtype = SUBTYPE_HUMAN;
	  this_test.state = STATE_ATTACKING;
	  pump_creatures_until_eot(player, card, player, 0, 0, 0, KEYWORD_FLYING, 0, &this_test);
	}

  return 0;
}

int card_celestial_archon(int player, int card, event_t event){
/* CARD_ID_CELESTIAL_ARCHON	13403
Celestial Archon	|3|W|W
Enchantment Creature - Archon 4/4
Bestow |5|W|W (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
Flying, first strike
Enchanted creature gets +4/+4 and has flying and first strike. */

	return generic_creature_with_bestow(player, card, event, 5, 0, 0, 0, 0, 2, 4, 4, KEYWORD_FLYING+KEYWORD_FIRST_STRIKE, 0);
}

int card_chained_to_the_rocks(int player, int card, event_t event){
/* CARD_ID_CHAINED_TO_THE_ROCKS	13406
Chained to the Rocks	|W
Enchantment - Aura
Enchant |H2Mountain you control
When ~ enters the battlefield, exile target creature an opponent controls until ~ leaves the battlefield. (That creature returns under its owner's control.) */

	return_from_oblivion(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.allowed_controller = 1-player;
		td1.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if (instance->damage_target_player >= 0 && can_target(&td1)){
			if (new_pick_target(&td1, "TARGET_CREATURE", 1, 0)){
				obliviation(player, card, instance->targets[1].player, instance->targets[1].card);
			}
		}
		instance->number_of_targets = 0;
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN);

	return targeted_aura_custom_prompt(player, card, event, &td,
										get_hacked_land_text(player, card, "Select target %s you control.", SUBTYPE_MOUNTAIN));
}

int card_chosen_by_heliod(int player, int card, event_t event){
/* CARD_ID_CHOSEN_BY_HELIOD	13407
Chosen by Heliod	|1|W
Enchantment - Aura
Enchant creature
When ~ enters the battlefield, draw a card.
Enchanted creature gets +0/+2. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return generic_aura(player, card, event, player, 0, 2, 0, 0, 0, 0, 0);
}

int card_coastline_chimera(int player, int card, event_t event)
{
/* CARD_ID_COASTLINE_CHIMERA	13409
Coastline Chimera	|3|U
Creature - Chimera 1/5
Flying
|1|W: ~ can block an additional creature this turn. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  can_block_additional_until_eot(instance->parent_controller, instance->parent_card,
									instance->parent_controller, instance->parent_card, 1);
	}

  return generic_activated_ability(player, card, event, GAA_NONE, MANACOST_XW(1,1), 0, NULL, NULL);
}

int card_dauntless_onslaught(int player, int card, event_t event){
/* CARD_ID_DAUNTLESS_ONSLAUGHT	13417
Dauntless Onslaught	|2|W
Instant
Up to two target creatures each get +2/+2 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i, successes = 0;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 2, 2);
				++successes;
			}
		}
		if (successes == 0 && instance->number_of_targets > 0){
			spell_fizzled = 1;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

static const char* target_noncombat_damage(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
	card_instance_t* inst = get_card_instance(player, card);
	if (inst->internal_card_id != damage_card){
		return ",card type";
	}
	if (!(inst->token_status & (STATUS_COMBAT_DAMAGE | STATUS_FIRST_STRIKE_DAMAGE))){
		return ",combat damage";
	}
	return NULL;
}

int card_decorated_griffin(int player, int card, event_t event){
	/* Decorated Griffin	|4|W
	 * Creature - Griffin 2/3
	 * Flying
	 * |1|W: Prevent the next 1 combat damage that would be dealt to you this turn. */

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_NONE);
	td.preferred_controller = ANYBODY;	// damage cards attached to a player are internally controlled by that player
	td.extra = target_predicate_extra(target_noncombat_damage);
	td.illegal_abilities = 0;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER | TARGET_SPECIAL_EXTRA_FUNCTION;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION_ME, MANACOST_XW(1, 1), 0, &td, "TARGET_DAMAGE");
}

int card_elspeth_suns_champion(int player, int card, event_t event){

	/* Elspeth, Sun's Champion	|4|W|W
	 * Planeswalker - Elspeth (4)
	 * +1: Put three 1/1 |Swhite Soldier creature tokens onto the battlefield.
	 * -3: Destroy all creatures with power 4 or greater.
	 * -7: You get an emblem with "Creatures you control get +2/+2 and have flying." */
	check_legend_rule(player, card, event);

	if (IS_ACTIVATING(event)){

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.power = 3;
		this_test.power_flag = 2;

		int priority_kill = 0;
		int priority_emblem = 0;
		if( event == EVENT_ACTIVATE ){
			priority_kill = (check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test)-
							check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test))*4;
			priority_kill += ((count_counters(player, card, COUNTER_LOYALTY)*4)-12);
			if( ! check_battlefield_for_id(player, CARD_ID_ELSPETH_SUNS_CHAMPION_EMBLEM) ){
				priority_emblem = 5*count_subtype(player, TYPE_CREATURE, -1);
				priority_emblem += ((count_counters(player, card, COUNTER_LOYALTY)*4)-21);
			}
		}

		enum{
			CHOICE_SOLDIERS = 1,
			CHOICE_KILL_MIGHTIES,
			CHOICE_EMBLEM
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Generate 3 Soldiers", 1, 10, 1,
						"Kill all creatures with POW >= 4", 1, priority_kill, -3,
						"Emblem", 1, priority_emblem, -7);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		{
			switch (choice)
			{
				case CHOICE_SOLDIERS:
					generate_tokens_by_id(player, card, CARD_ID_SOLDIER, 3);
					break;

				case CHOICE_KILL_MIGHTIES:
					new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
					break;

				case CHOICE_EMBLEM:
					generate_token_by_id(player, card, CARD_ID_ELSPETH_SUNS_CHAMPION_EMBLEM);
					break;
			}
		}
	}

	return planeswalker(player, card, event, 4);
}

int card_elspeth_suns_champion_emblem(int player, int card, event_t event){
  /* Elspeth, Sun's Champion Emblem	""
   * Emblem
   * Creatures you control get +2/+2 and have flying. */

	boost_subtype(player, card, event, -1, 2,2, KEYWORD_FLYING,0, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);
	return 0;
}

int card_epharas_warden(int player, int card, event_t event){
/* CARD_ID_EPHARAS_WARDEN	13429
Ephara's Warden	|3|W
Creature - Human Cleric 1/2
|T: Tap target creature with power 3 or less. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 3 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance= get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creature with POW 3 or lesser.");
}

int card_evangel_of_heliod(int player, int card, event_t event){
	/* Evangel of Heliod	|4|W|W
	 * Creature - Human Cleric 1/3
	 * When ~ enters the battlefield, put a number of 1/1 |Swhite Soldier creature tokens onto the battlefield equal to your devotion to |Swhite. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_SOLDIER, devotion(player, card, COLOR_WHITE, 0));
	}

	return 0;
}

int card_fabled_hero(int player, int card, event_t event){

	/* Akroan Skyguard	|1|W
	 * Creature - Human Soldier 1/1
	 * Flying
	 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

	/* Fabled Hero	|1|W|W
	 * Creature - Human Soldier 2/2
	 * Double strike
	 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

	/* Lagonna-Band Trailblazer	|W
	 * Creature - Centaur Scout 0/4
	 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

	/* Satyr Hoplite	|R
	 * Creature - Satyr Soldier 1/1
	 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

	/* War-Wing Siren	|2|U
	 * Creature - Siren Soldier 1/3
	 * Flying
	 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

	/* Wingsteed Rider	|1|W|W
	 * Creature - Human Knight 2/2
	 * Flying
	 * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~. */

	if( heroic(player, card, event) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	return 0;
}

int card_favored_hoplite(int player, int card, event_t event){
/* CARD_ID_FAVORED_HOPLITE	13439
Favored Hoplite	|W
Creature - Human Soldier 1/2
Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on ~ and prevent all damage that would be dealt to it this turn. */

	if( heroic(player, card, event) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		prevent_the_next_n_damage(player, card, player, card, 0, PREVENT_INFINITE, 0, 0);
	}

	return 0;
}

static int gift_of_immortality_legacy(int player, int card, event_t event)
{

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int owner = instance->targets[0].player;
    int iid = instance->targets[0].card;
    int result =
        seek_grave_for_id_to_reanimate(owner, -1, owner, cards_data[iid].id, REANIMATE_DEFAULT);
    if (result == -1)
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
    instance->targets[2].player = owner;
    instance->targets[2].card = result;
  }

  if (eot_trigger(player, card, event))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (in_play(instance->targets[2].player, instance->targets[2].card))
    {
      int owner = instance->targets[1].player;
      int iid = instance->targets[1].card;
      seek_grave_for_id_to_reanimate(instance->targets[2].player, instance->targets[2].card, owner,
                                     cards_data[iid].id, REANIMATE_ATTACH_AS_AURA);
    }
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_gift_of_immortality(int player, int card, event_t event){
/* CARD_ID_GIFT_OF_IMMORTALITY	13448
Gift of Immortality	|2|W
Enchantment - Aura
Enchant creature
When enchanted creature dies, return that card to the battlefield under its owner's control.
Return ~ to the battlefield attached to that creature at the beginning of the next end step. */

	if( ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;

		if( event == EVENT_GRAVEYARD_FROM_PLAY && affect_me(p, c) ){
			if( is_this_dying(p, c) && !is_token(p, c) && !check_for_death_trigger_removal(p, c) ){
				int legacy = create_legacy_effect(player, card, &gift_of_immortality_legacy);
				card_instance_t *leg = get_card_instance(player, legacy);
				leg->targets[0].player = get_owner(p, c);
				leg->targets[0].card = get_original_internal_card_id(p, c);
				leg->targets[1].player = get_owner(player, card);
				leg->targets[1].card = get_original_internal_card_id(player, card);
				leg->targets[11].player = 1; // Otherwise, "resolve_gfp_ability" won't trigger
			}
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		vanilla_aura(player, card, event, player);
		if( spell_fizzled != 1 ){
			int mod = (instance->targets[0].player == player ? 24 : -24);
			int iid = get_original_internal_card_id(instance->targets[0].player, instance->targets[0].card);
			if( cards_data[iid].extra_ability & EA_COUNTERS ){
				mod += (instance->targets[0].player == player ? 24 : -24);
			}
			ai_modifier += mod;
		}
		return 0;
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_glare_of_heresy(int player, int card, event_t event){
/* CARD_ID_GLARE_OF_HERESY	13449
Glare of Heresy	|1|W
Sorcery
Exile target |Swhite permanent. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT );
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target %s permanent card.", COLOR_WHITE), 1, NULL);
}

int card_gods_willing(int player, int card, event_t event){
/* CARD_ID_GODS_WILLING	13450
Gods Willing	|W
Instant
Target creature you control gains protection from the color of your choice until end of turn. Scry 1. (Look at the top card of your library.
You may put that card on the bottom of your library.) */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int kw = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, kw, 0);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_heliod_god_of_the_sun(int player, int card, event_t event){
	/* Heliod, God of the Sun	|3|W
	 * Legendary Enchantment Creature - God 0/0
	 * Indestructible
	 * As long as your devotion to |Swhite is less than five, Heliod isn't a creature.
	 * Other creatures you control have vigilance.
	 * |2|W|W: Put a 2/1 |Swhite Cleric enchantment creature token onto the battlefield. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_WHITE, 0, 5);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(instance->parent_controller, instance->parent_card, CARD_ID_CLERIC, &token);
		token.pow = 2;
		token.tou = 1;
		token.color_forced = COLOR_TEST_WHITE;
		token.special_flags2 = SF2_ENCHANTED_EVENING;
		generate_token(&token);
	}

	if (event == EVENT_ABILITIES
		&& affected_card_controller == player
		&& affected_card != card
		&& is_what(affected_card_controller, affected_card, TYPE_CREATURE)){
		vigilance(affected_card_controller, affected_card, event);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XW(2, 2), 0, NULL, NULL);
}

int card_heliods_emissary(int player, int card, event_t event)
{
/* CARD_ID_HELIODS_EMISSARY	13456
Heliod's Emissary	|3|W
Enchantment Creature - Elk 3/3
Bestow |6|W (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
Whenever ~ or enchanted creature attacks, tap target creature an opponent controls.
Enchanted creature gets +3/+3. */

	card_instance_t* instance = in_play(player, card);
	if (instance){
		instance = get_card_instance(player, card);
		int attacker_player = player, attacker_card = card;
		if (instance->damage_target_player >= 0){
			attacker_player = instance->damage_target_player;
			attacker_card = instance->damage_target_card;
		}
		store_attackers(player, card, event, 0, attacker_player, attacker_card, NULL);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;
		instance = get_card_instance(player, card);
		if (can_target(&td) && new_pick_target(&td, "TARGET_CREATURE_OPPONENT_CONTROLS", 1, 0)){
			tap_card(instance->targets[1].player, instance->targets[1].card);
		}
		instance->number_of_targets = 1;
	}
	return generic_creature_with_bestow(player, card, event, MANACOST_XW(6,1), 3,3, 0,0);
}

int card_hopeful_eidolon(int player, int card, event_t event){
/* CARD_ID_HOPEFUL_EIDOLON	13459
Hopeful Eidolon	|W
Enchantment Creature - Spirit 1/1
Bestow |3|W (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
Lifelink (Damage dealt by this creature also causes you to gain that much life.)
Enchanted creature gets +1/+1 and has lifelink. */

	lifelink(player, card, event);

	return generic_creature_with_bestow(player, card, event, MANACOST_XW(3, 1), 1, 1, 0, SP_KEYWORD_LIFELINK);
}

int card_hundred_handed_one(int player, int card, event_t event){
/* CARD_ID_HUNDRED_HANDED_ONE	13463
Hundred-Handed One	|2|W|W
Creature - Giant 3/5
Vigilance
|3|W|W|W: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
As long as ~ is monstrous, it has reach and can block an additional ninety-nine creatures each combat. */

	vigilance(player, card, event);
	if( is_monstrous(player, card) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_REACH);
		creature_can_block_additional(player, card, event, 99);
	}
	return monstrosity(player, card, event, MANACOST_XW(3, 3), 3);
}

int card_lagonna_band_elder(int player, int card, event_t event){
/* CARD_ID_LAGONNA_BAND_ELDER	13472
Lagonna-Band Elder	|2|W
Creature - Centaur Advisor 3/2
When ~ enters the battlefield, if you control an enchantment, you gain 3 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( count_subtype(player, TYPE_ENCHANTMENT, -1) ){
			gain_life(player, 3);
		}
	}
	return 0;
}

int card_last_breath(int player, int card, event_t event){
/* CARD_ID_LAST_BREATH	4518
Last Breath	|1|W
Instant
Exile target creature with power 2 or less. Its controller gains 4 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
			gain_life(instance->targets[0].player, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with POW 2 or lesser.", 1, NULL);
}

int card_leonin_snarecaster(int player, int card, event_t event){
/* CARD_ID_LEONIN_SNARECASTER	13476
Leonin Snarecaster	|1|W
Creature - Cat Soldier 2/1
When ~ enters the battlefield, you may tap target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_observant_alseid(int player, int card, event_t event){
/* CARD_ID_OBSERVANT_ALSEID	13501
Observant Alseid	|2|W
Enchantment Creature - Nymph 2/2
Bestow |4|W (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
Vigilance
Enchanted creature gets +2/+2 and has vigilance. */

	vigilance(player, card, event);

	return generic_creature_with_bestow(player, card, event, MANACOST_XW(4, 1), 2, 2, 0, SP_KEYWORD_VIGILANCE);
}

int card_ordeal_of_heliod(int player, int card, event_t event){
/* CARD_ID_ORDEAL_OF_HELIOD	13506
Ordeal of Heliod	|1|W
Enchantment - Aura
Enchant creature
Whenever enchanted creature attacks, put a +1/+1 counter on it. Then if it has three or more +1/+1 counters on it, sacrifice ~.
When you sacrifice ~, you gain 10 life. */

	if (resolve_graveyard_trigger(player, card, event)){
		get_card_instance(player, card)->targets[11].player = -1;
		gain_life(player, 10);
	}

	return ordeal(player, card, event);
}

int card_phalanx_leader(int player, int card, event_t event)
{
  /* Phalanx Leader	|W|W
   * Creature - Human Soldier 1/1
   * Heroic - Whenever you cast a spell that targets ~, put a +1/+1 counter on each creature you control. */

  if (heroic(player, card, event))
	manipulate_type(player, card, player, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_P1_P1, 1));

  return 0;
}

int card_ray_of_dissolution(int player, int card, event_t event){
/* CARD_ID_RAY_OF_DISSOLUTION	13531
Ray of Dissolution	|2|W
Instant
Destroy target enchantment. You gain 3 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			gain_life(player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ENCHANTMENT", 1, NULL);
}

int card_scholar_of_athreos(int player, int card, event_t event){
  /* Scholar of Athreos	|2|W
   * Creature - Human Cleric 1/4
   * |2|B: Each opponent loses 1 life. You gain life equal to the life lost this way. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int result = lose_life(1-player, 1);
		gain_life(player, result);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XB(2, 1), 0, NULL, NULL);
}

int card_setessan_battle_priest(int player, int card, event_t event){
  /* Setessan Battle Priest	|1|W
   * Creature - Human Cleric 1/3
   * Heroic - Whenever you cast a spell that targets this creature, you gain 2 life. */

	if( heroic(player, card, event) ){
		gain_life(player, 2);
	}

	return 0;
}

int card_setessan_griffin(int player, int card, event_t event){
  /* Setessan Griffin	|4|W
   * Creature - Griffin 3/2
   * Flying
   * |2|G|G: This creature gets +2/+2 until end of turn. Activate only once each turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_ONCE_PER_TURN, MANACOST_XW(2, 2), 0, NULL, NULL) ){
			return 2;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade(player, card, event, 0, MANACOST_XW(2, 2), 2, 2, 0, 0);
	}

	return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_XW(2, 2), 0, NULL, NULL);
}

// Silent Artisan --> vanilla

int card_soldier_of_the_pantheon(int player, int card, event_t event){
  /* Soldier of the Pantheon	|W
   * Creature - Human Soldier 2/1
   * Protection from multicolored
   * Whenever an opponent casts a multicolored spell, you gain 1 life. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color_flag = F3_MULTICOLORED;
		sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		gain_life(player, 1);
	}
	protection_from_multicolored(player, card, event);
	return 0;
}

static const char* target_damaged_me_this_turn(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
	int good = who_chooses == 0 ? check_special_flags2(player, card, SF2_HAS_DAMAGED_PLAYER0) :
							check_special_flags2(player, card, SF2_HAS_DAMAGED_PLAYER1);
	return good ? NULL : "dealt damage to you this turn";
}

int card_spear_of_heliod(int player, int card, event_t event)
{
  /* Spear of Heliod	|1|W|W
   * Legendary Enchantment Artifact
   * Creatures you control get +1/+1.
   * |1|W|W, |T: Destroy target creature that dealt damage to you this turn. */

	check_legend_rule(player, card, event);

	if (event == EVENT_CAN_CAST)
		return 1;

	// Creatures you control get +1/+1.
	boost_creature_type(player, card, event, -1, 1, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	// |1|W|W, |T: Destroy target creature that dealt damage to you this turn.
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(target_damaged_me_this_turn);

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (valid_target(&td))
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_XW(1,2), 0,
									&td, "Select target creature that dealt damage to you this turn.");
}

// Travelling philosopher --> vanilla

int card_vanquish_the_foul(int player, int card, event_t event){
  /* Vanquish the Foul	|5|W
   * Sorcery
   * Destroy target creature with power 4 or greater. Scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 4 | TARGET_PT_GREATER_OR_EQUAL;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with POW 4 or higher.", 1, NULL);
}

// wingsteed rider --> fabled hero

// yoked ox --> vanilla

// Blue

int card_aqueous_form(int player, int card, event_t event)
{
  /* Aqueous Form	|U
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't be blocked.
   * Whenever enchanted creature attacks, scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

	card_instance_t* instance = get_card_instance(player, card);
	if (instance->damage_target_player >= 0){
		instance = get_card_instance(player, card);
		store_attackers(player, card, event, 0, instance->damage_target_player, instance->damage_target_card, NULL);
		dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
		if (event == EVENT_DAT_ABILITY){
			scry(player, 1);
		}
	}
	return generic_aura(player, card, event, player, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE, 0, 0, 0);
}

static int effect_artisan_of_forms(int player, int card, event_t event)
{
  if (effect_follows_control_of_attachment(player, card, event))
	return 0;

  card_instance_t* instance = get_card_instance(player, card);
  int p = instance->damage_target_player, c = instance->damage_target_card;

  if (becomes_target_of_spell(player, card, event, p, c, player, RESOLVE_TRIGGER_AI(player)))
	{
	  target_definition_t td;
	  default_target_definition(p, c, &td, TYPE_CREATURE);
	  td.preferred_controller = ANYBODY;

	  instance->number_of_targets = 0;
	  load_text(0, "TARGET_CREATURE");
	  if (select_target(p, c-1000, &td, text_lines[0], &instance->targets[0]))
		{
		  if (instance->targets[0].card == c && instance->targets[0].player == p)	// picked self
			return 0;

		  cloning_and_verify_legend(p, c, instance->targets[0].player, instance->targets[0].card);
		}
	}

  return 0;
}

int card_artisan_of_forms(int player, int card, event_t event)
{
	/* Artisan of Forms	|1|U	0x200ae30
	 * Creature - Human Wizard 1/1
	 * Heroic - Whenever you cast a spell that targets ~, you may have ~ become a copy of target creature, except it has this ability. */
	cloning_card(player, card, event);

	if (event == EVENT_RESOLVE_SPELL){
		create_targetted_legacy_effect(player, card, &effect_artisan_of_forms, player, card);
	}

	return 0;
}

// benthic giant --> aven_fleetwing

int card_bident_of_thassa(int player, int card, event_t event){

	/* Bident of Thassa	|2|U|U
	 * Legendary Enchantment Artifact
	 * Whenever a creature you control deals combat damage to a player, you may draw a card.
	 * |1|U, |T: Creatures your opponents control attack this turn if able. */

	check_legend_rule(player, card, event);

	if (event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if (subtype_deals_damage(player, card, event, player, -1, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRIGGER_OPTIONAL)){
		draw_some_cards_if_you_want(player, card, player, get_card_instance(player, card)->targets[1].card);
	}

	if( event == EVENT_ACTIVATE && current_turn == player ){
		ai_modifier-=100;
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, 1-player, -1, 0, 0, 0, SP_KEYWORD_MUST_ATTACK);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_XU(1, 1), 0, NULL, NULL);
}

int card_breaching_hippocamp(int player, int card, event_t event){
  /* Breaching Hippocamp	|3|U
   * Creature - Horse Fish 3/2
   * Flash (You may cast this spell any time you could cast an instant.)
   * When this creature enters, untap another target creature you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = td.preferred_controller = player;

		card_instance_t* instance = get_card_instance(player, card);

		state_untargettable(player, card, 1);
		if (can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
		state_untargettable(player, card, 0);
		instance->number_of_targets = 0;
	}

	return flash(player, card, event);
}

// Coastline Chimera --> multiblock.c

int card_crackling_triton(int player, int card, event_t event){
	/* Crackling Triton	|2|U	0x200ae3f
	 * Creature - Merfolk Wizard 2/3
	 * |2|R, Sacrifice ~: ~ deals 2 damage to any target. */
	/* Scaldkin	|3|U	0x200ae3f
	 * Creature - Elemental 2/2
	 * Flying
	 * |2|R, Sacrifice ~: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_XR(2, 1), 0,
									&td, "TARGET_ANY");
}

int card_curse_of_the_swine(int player, int card, event_t event){
  /* Curse of the Swine	|X|U|U
   * Sorcery
   * Exile X target creatures. For each creature exiled this way, its controller creates a 2/2 green Boar creature token. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		int i, num[2] = {0};
		for (i = 0; i < instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_EXILE);
				++num[instance->targets[i].player];
			}
		}

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_BOAR, &token);
		token.pow = token.tou = 2;
		for (i = 0; i < 2; ++i){
			if (num[i] > 0){
				token.t_player = i;
				token.qty = num[i];
				generate_token(&token);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return x_spell_with_x_targets(player, card, event, &td, "TARGET_CREATURE", 0);
}

int card_dissolve(int player, int card, event_t event){
  /* Dissolve	|1|U|U
   * Instant
   * Counter target spell. Scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

	if( event == EVENT_RESOLVE_SPELL ){
		if (counterspell_validate(player, card, 0, 0)){
			card_instance_t *instance = get_card_instance(player, card);
			if (pre_counterspell(player, card, instance->targets[0].player, instance->targets[0].card)){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
			}
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_fate_foretold(int player, int card, event_t event){
	/* Fate Foretold	|1|U
	 * Enchantment - Aura
	 * Enchant creature
	 * When ~ enters the battlefield, draw a card.
	 * When enchanted creature dies, its controller draws a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if (attached_creature_dies_trigger_for_controller(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
		draw_cards(get_card_instance(player, card)->damage_target_player, 1);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }
	return vanilla_aura(player, card, event, player);
}

int card_gainsay(int player, int card, event_t event){
  /* Gainsay	|1|U
   * Instant
   * Counter target blue spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, 0);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);

	return counterspell(player, card, event, &td, 0);
}

int card_horizon_scholar(int player, int card, event_t event){
  /* Horizon Scholar	|5|U
   * Creature - Sphinx 4/4
   * Flying
   * When this creature enters, scry 2. (Look at the top two cards of your library, then put any number of them on the bottom and the rest on top in any order.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		scry(player, 2);
	}

	return 0;
}

int card_lost_in_a_labyrinth(int player, int card, event_t event){
	/* Lost in a Labyrinth	|U
	 * Instant
	 * Target creature gets -3/-0 until end of turn. Scry 1. */
	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -3, 0, VANILLA_PUMP_SCRY_1, 0);
}

int card_master_of_waves(int player, int card, event_t event){
	/* Master of Waves	|3|U
	 * Creature - Merfolk Wizard 2/1
	 * Protection from |Sred
	 * Elemental creatures you control get +1/+1.
	 * When ~ enters the battlefield, put a number of 1/0 |Sblue Elemental creature tokens onto the battlefield equal to your devotion to |Sblue. */

	boost_creature_type(player, card, event, SUBTYPE_ELEMENTAL, 1, 1, 0, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.qty = devotion(player, card, COLOR_BLUE, 0);
		token.pow = 1;
		token.tou = 0;
		token.color_forced = COLOR_TEST_BLUE;
		generate_token(&token);
	}

	return 0;
}

int card_meletis_charlatan(int player, int card, event_t event){
  /* Meletis Charlatan	|2|U
   * Creature - Human Wizard 2/3
   * |2|U, |T: The controller of target instant or sorcery spell copies it. That player may choose new targets for the copy. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_fork;
	counterspell_target_definition(player, card, &td_fork, TYPE_INSTANT|TYPE_SORCERY);
	td_fork.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, &td_fork, 0) ){
			copy_spell_from_stack(instance->targets[0].player, instance->targets[0].player, instance->targets[0].card);
		}
	}


	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_NONSICK | GAA_SPELL_ON_STACK, MANACOST_XU(2, 1), 0, &td_fork, 0);
}

int card_nimbus_naiad(int player, int card, event_t event){
  /* Nimbus Naiad	|2|U
   * Enchantment Creature - Nymph 2/2
   * Bestow |4|U (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached.)
   * Flying
   * Enchanted creature gets +2/+2 and has flying. */

	return generic_creature_with_bestow(player, card, event, MANACOST_XU(4, 1), 2, 2, KEYWORD_FLYING, 0);
}

// omenspeaker --> horizon scholar

int card_ordeal_of_thassa(int player, int card, event_t event){
  /* Ordeal of Thassa	|1|U
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature attacks, put a +1/+1 counter on it. Then if it has three or more +1/+1 counters on it, sacrifice this Aura.
   * When you sacrifice this Aura, draw two cards. */

	if (resolve_graveyard_trigger(player, card, event)){
		get_card_instance(player, card)->targets[11].player = -1;
		draw_cards(player, 2);
	}

	return ordeal(player, card, event);
}

int card_prescient_chimera(int player, int card, event_t event){
  /* Prescient Chimera	|3|U|U
   * Creature - Chimera 3/4
   * Flying
   * Whenever you cast an instant or sorcery spell, scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		scry(player, 1);
	}
	return 0;
}

int card_prognostic_sphinx(int player, int card, event_t event)
{
  /* Prognostic Sphinx	|3|U|U
   * Creature - Sphinx 3/5
   * Flying
   * Discard a card: This creature gains hexproof until end of turn. Tap it.
   * Whenever this creature attacks, scry 3. */

  // Whenever ~ attacks, scry 3.
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	scry(player, 3);

  // Discard a card: ~ gains hexproof until end of turn. Tap it.
  else if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
							0, 0, 0, SP_KEYWORD_HEXPROOF);
	  tap_card(instance->parent_controller, instance->parent_card);
	}

  return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST0, 0, NULL, NULL);
}

int card_sea_gods_revenge(int player, int card, event_t event){
  /* Sea God's Revenge	|5|U
   * Sorcery
   * Return up to three target creatures your opponents control to their owners' hands. Scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		int i, good = 0;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				good++;
				bounce_permanent(instance->targets[i].player, instance->targets[i].card);
			}
		}
		if( good || instance->number_of_targets == 0){
			scry(player, 1);
		} else {
			spell_fizzled = 1;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 3, NULL);
}

static int sealock_monster_legacy(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  int p = instance->damage_target_player;
  int c = instance->damage_target_card;
  if (p >= 0)
  {
    if (event == EVENT_STATIC_EFFECTS)
    {
      if (c < 0 || !in_play(p, c))
      {
        kill_card(player, card, KILL_EXILE);
        recalculate_all_cards_in_play();
      }
    }
    else if (event == EVENT_CAN_ACTIVATE)
      return mana_producer(p, c, event);
    else if (event == EVENT_ACTIVATE)
    {
      tap_card(p, c);
      color_t color =
          single_color_test_bit_to_color(basic_subtype_to_color_test(instance->targets[1].card));
      produce_mana_tapped(p, c, color, 1);
    }
  }
  attached_is_additional_subtype(player, card, event, instance->targets[1].card);
  return 0;
}

int card_sealock_monster(int player, int card, event_t event)
{
  /* Sealock Monster	|3|U|U
   * Creature - Octopus 5/5
   * This creature can't attack unless defending player controls an Island.
   * |5|U|U: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
   * When this creature becomes monstrous, target land becomes an Island in addition to its other types. */

  if (event == EVENT_ATTACK_LEGALITY && affect_me(player, card) &&
      !is_humiliated(player, card, AL_STATIC))
  {
    if (!basiclandtypes_controlled[1 - player][get_hacked_color(player, card, COLOR_BLUE)])
    {
      event_result = 1;
    }
  }

  if (event == EVENT_BECAME_MONSTROUS)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_LAND);

    if (can_target(&td) && pick_target(&td, "TARGET_LAND"))
    {
      card_instance_t *instance = get_card_instance(player, card);

      if (instance->targets[0].player == player)
      {
        int legacy = create_targetted_legacy_activate(player, card, &sealock_monster_legacy, player,
                                                      instance->targets[0].card);
        get_card_instance(player, legacy)->targets[1].card =
            get_hacked_subtype(player, card, SUBTYPE_ISLAND);
      }
      else
      {
        int fake = add_card_to_hand(instance->targets[0].player, instance->internal_card_id);
        --hand_count[instance->targets[0].player];
        int legacy = create_targetted_legacy_activate(
            instance->targets[0].player, fake, &sealock_monster_legacy, instance->targets[0].player,
            instance->targets[0].card);
        get_card_instance(instance->targets[0].player, legacy)->targets[1].card =
            get_hacked_subtype(player, card, SUBTYPE_ISLAND);
        obliterate_card(instance->targets[0].player, fake);
      }
    }
  }

  return monstrosity(player, card, event, MANACOST_XU(5, 2), 3);
}

int card_shipbreaker_kraken(int player, int card, event_t event){
  /* Shipbreaker Kraken	|4|U|U
   * Creature - Kraken 6/6
   * |6|U|U: Monstrosity 4. (If this creature isn't monstrous, put four +1/+1 counters on it and it becomes monstrous.)
   * When this creature becomes monstrous, tap up to four target creatures. Those creatures don't untap during their controllers' untap steps for as long as you control this creature. */

	if( event == EVENT_BECAME_MONSTROUS ){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		instance->number_of_targets = 0;
		int trgs;
		for (trgs = 0; trgs < 4 && can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", trgs, 0); ++trgs){
			state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
		}
		for (trgs = 0; trgs < instance->number_of_targets; ++trgs){
			state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 0);
			does_not_untap_effect(player, card, instance->targets[trgs].player, instance->targets[trgs].card,
								  EDNT_TAP_TARGET|EDNT_REMAIN_TAPPED_UNTIL_SOURCE_LEAVES_PLAY, 0);
		}
	}

	return monstrosity(player, card, event, MANACOST_XU(6, 2), 4);
}

int card_stymied_hopes(int player, int card, event_t event){
	/* Stymied Hopes	|1|U
	 * Instant
	 * Counter target spell unless its controller pays |1. Scry 1. */

	if( event == EVENT_RESOLVE_SPELL ){
		if (counterspell_resolve_unless_pay_x(player, card, NULL, 0, 1)){
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 0, NULL);
}

int card_swan_song(int player, int card, event_t event){
	/* Swan Song	|U
	 * Instant
	 * Counter target enchantment, instant, or sorcery spell. Its controller puts a 2/2 |Sblue Bird creature token with flying onto the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ENCHANTMENT | TYPE_SPELL);

	if (event == EVENT_RESOLVE_SPELL){
		if (counterspell_validate(player, card, &td, 0)){
			card_instance_t* instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_BIRD, &token);
			token.t_player = instance->targets[0].player;
			token.pow = token.tou = 2;
			token.color_forced = COLOR_TEST_BLUE;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, &td, NULL, 1, NULL);
}

int card_thassa_god_of_the_sea(int player, int card, event_t event){
/* CARD_ID_THASSA_GOD_OF_THE_SEA	13572
Thassa, God of the Sea	|2|U
Legendary Enchantment Creature - God 0/0
Indestructible
As long as your devotion to |Sblue is less than five, ~ isn't a creature. (Each |U in the mana costs of permanents you control counts toward your devotion to |Sblue.)
At the beginning of your upkeep, scry 1.
|1|U: Target creature you control can't be blocked this turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_BLUE, 0, 5);

	upkeep_trigger_ability(player, card, event, player);

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
		scry(player, 1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		if( current_turn == player && current_phase < PHASE_DECLARE_BLOCKERS && can_target(&td) ){
			ai_modifier+=15;
		}
		else{
			ai_modifier-=50;
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XU(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_thassas_bounty(int player, int card, event_t event){
/* CARD_ID_THASSAS_BOUNTY	13574
Thassa's Bounty	|5|U
Sorcery
Draw three cards. Target player puts the top three cards of his or her library into his or her graveyard. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier_for_targeted_card_drawing(player, card, 0, 3);
		}
		return 0;
	}


	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(player, 3);
			mill(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_thassas_emissary(int player, int card, event_t event){
/* CARD_ID_THASSAS_EMISSARY	13575
Thassa's Emissary	|3|U
Enchantment Creature - Crab 3/3
Bestow |5|U (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
Whenever ~ or enchanted creature deals combat damage to a player, draw a card.
Enchanted creature gets +3/+3. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(player, card, TYPE_CREATURE) ){
			check_damage_test(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS,
								player, card, 0, NULL);
		}
		else{
			card_instance_t *instance = get_card_instance(player, card);

			int p = instance->damage_target_player;
			int c = instance->damage_target_card;

			check_damage_test(p, c, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS, player, card,
							0, NULL);
		}
	}

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE|DDBM_MUST_DAMAGE_PLAYER|DDBM_TRACE_DAMAGED_PLAYERS,
								player, card) )
	{
		card_instance_t *instance = get_card_instance(player, card);
		int result = instance->targets[1].player;
		instance->targets[1].player = 0;
		if( result ){
			draw_cards(player, BYTE0(result) + BYTE1(result));
		}
	}

	return generic_creature_with_bestow(player, card, event, MANACOST_XU(5, 1), 3, 3, 0, 0);
}

int card_triton_fortune_hunter(int player, int card, event_t event){
/* CARD_ID_TRITON_FORTUNE_HUNTER	13583
Triton Fortune Hunter	|2|U
Creature - Merfolk Soldier 2/2
Heroic - Whenever you cast a spell that targets ~, draw a card. */

	if( heroic(player, card, event) ){
		draw_cards(player, 1);
	}

	return 0;
}

// Triton Shorethief --> vanilla

static int triton_tactics_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	if( instance->targets[0].player > -1 ){
		modify_pt_and_abilities(instance->targets[0].player, instance->targets[0].card, event, 0, 3, 0);
		if (event == EVENT_DECLARE_BLOCKERS && current_turn != player && instance->targets[1].player != 66){
			card_instance_t* blocker = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if (blocker->blocking != 255){
				card_instance_t* blocked = get_card_instance(1-player, blocker->blocking);
				if (blocked->blocking == 255){	// attacking alone
					does_not_untap_effect(player, card, 1-player, blocker->blocking, EDNT_TAP_AT_END_OF_COMBAT, 1);
				} else {
					int c;
					for (c = 0; c < active_cards_count[1-player]; ++c){	// attacking in a band
						if (in_play(1-player, c) && get_card_instance(1-player, c)->blocking == blocker->blocking){
							does_not_untap_effect(player, card, 1-player, c, EDNT_TAP_AT_END_OF_COMBAT, 1);
						}
					}
				}
				instance->targets[1].player = 66;
			}
		}
	}

	if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_triton_tactics(int player, int card, event_t event){
/* CARD_ID_TRITON_TACTICS	13585
Triton Tactics	|U
Instant
Up to two target creatures each get +0/+3 until end of turn. Untap those creatures. At this turn's next end of combat,
tap each creature that was blocked by one of those creatures this turn and it doesn't untap during its controller's next untap step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				untap_card(instance->targets[i].player, instance->targets[i].card);
				create_targetted_legacy_effect(player, card, &triton_tactics_legacy, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

// vaporkin --> cloud sprite

int card_voyages_end(int player, int card, event_t event){
/* CARD_ID_VOYAGES_END	13593
Voyage's End	|1|U
Instant
Return target creature to its owner's hand. Scry 1. (Look at the top card of your library. You may put that card on the bottom of your library.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_wavecrash_triton(int player, int card, event_t event){
/* CARD_ID_WAVECRASH_TRITON	13597
Wavecrash Triton	|2|U
Creature - Merfolk Wizard 1/4
Heroic - Whenever you cast a spell that targets ~, tap target creature an opponent controls. That creature doesn't untap during its controller's next untap step.*/

	if( heroic(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1 - player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "TARGET_CREATURE", 0, 0) ){
			effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

// Black

int card_abhorrent_overlord(int player, int card, event_t event){
	/* Abhorrent Overlord	|5|B|B
	 * Creature - Demon 6/6
	 * Flying
	 * When ~ enters the battlefield, put a number of 1/1 |Sblack Harpy creature tokens with flying onto the battlefield equal to your devotion to |Sblack.
	 * At the beginning of your upkeep, sacrifice a creature. */

	upkeep_trigger_ability(player, card, event, player);

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HARPY, &token);
		token.qty = devotion(player, card, COLOR_BLACK, 0);
		token.color_forced = COLOR_TEST_BLACK;
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);
	}

	return 0;
}

int card_agent_of_the_fates(int player, int card, event_t event){
/* CARD_ID_AGENT_OF_THE_FATES	13368
Agent of the Fates	|1|B|B
Creature - Human Assassin 3/2
Deathtouch
Heroic - Whenever you cast a spell that targets ~, each opponent sacrifices a creature. */

	deathtouch(player, card, event);

	if( heroic(player, card, event) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}

	return 0;
}

int card_asphodel_wanderer(int player, int card, event_t event){
/* CARD_ID_ASPHODEL_WANDERER	13383
Asphodel Wanderer	|B
Creature - Skeleton Soldier 1/1
|2|B: Regenerate ~. */

	return regeneration(player, card, event, MANACOST_XB(2, 1));
}

int card_baleful_eidolon(int player, int card, event_t event){
/* CARD_ID_BALEFUL_EIDOLON_BESTOWED	13385
Baleful Eidolon Bestowed	|1|B
Enchantment Creature - Spirit 0/0
Bestow |4|B (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
Deathtouch (Any amount of damage this deals to a creature is enough to destroy it.)
Enchanted creature gets +1/+1 and has deathtouch. */

	deathtouch(player, card, event);
	return generic_creature_with_bestow(player, card, event, MANACOST_XB(4, 1), 1, 1, 0, SP_KEYWORD_DEATHTOUCH);
}

int card_blood_toll_harpy(int player, int card, event_t event){
/* CARD_ID_BLOOD_TOLL_HARPY	13390
Blood-Toll Harpy	|2|B
Creature - Harpy 2/1
Flying
When ~ enters the battlefield, each player loses 1 life. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		lose_life(player, 1);
		lose_life(1-player, 1);
	}

	return 0;
}

int card_boon_of_erebos(int player, int card, event_t event)
{
	/* Boon of Erebos	|B
	 * Instant
	 * Target creature gets +2/+0 until end of turn. Regenerate it. You lose 2 life. */

	int rval = vanilla_instant_pump(player, card, event, ANYBODY, player, 2,0, VANILLA_PUMP_REGENERATE_DONT_KILL_CARD,0);
	if (rval && event == EVENT_RESOLVE_SPELL){
		lose_life(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}
	return rval;
}

int card_cavern_lampad(int player, int card, event_t event){
/* CARD_ID_CAVERN_LAMPAD	13401
Cavern Lampad	|3|B
Enchantment Creature - Nymph 2/2
Bestow |5|B (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached to a creature.)
Intimidate (This creature can't be blocked except by artifact creatures and/or creatures that share a color with it.)
Enchanted creature gets +2/+2 and has intimidate. */

	intimidate(player, card, event);
	return generic_creature_with_bestow(player, card, event, MANACOST_XB(5, 1), 2, 2, 0, SP_KEYWORD_INTIMIDATE);
}

int card_cutthroat_maneuver(int player, int card, event_t event){
/* CARD_ID_CUTTHROAT_MANEUVER	13415
Cutthroat Maneuver	|3|B
Instant
Up to two target creatures each get +1/+1 and gain lifelink until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 1, 1, 0, SP_KEYWORD_LIFELINK);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_dark_betrayal(int player, int card, event_t event){
/* CARD_ID_DARK_BETRAYAL	13416
Dark Betrayal	|B
Instant
Destroy target |Sblack creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET|GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target %s creature", COLOR_BLACK), 1, NULL);
}

int card_disciple_of_phenax(int player, int card, event_t event){
/* CARD_ID_DISCIPLE_OF_PHENAX	13423
Disciple of Phenax	|2|B|B
Creature - Human Cleric 1/3
When ~ enters the battlefield, target player reveals a number of cards from his or her hand equal to your devotion to |Sblack.
You choose one of them. That player discards that card. (Each |B in the mana costs of permanents you control counts toward your devotion to |Sblack.)*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && new_pick_target(&td, "TARGET_PLAYER", 0, 0) ){
			ec_definition_t ec;
			default_ec_definition(instance->targets[0].player, player, &ec);
			ec.cards_to_reveal = devotion(player, card, COLOR_BLACK, 0);

			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_ANY);

			new_effect_coercion(&ec, &this_test);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_erebos_god_of_the_dead(int player, int card, event_t event){
  /* Erebos, God of the Dead	|3|B
   * Legendary Enchantment Creature - God 5/7
   * Indestructible
   * As long as your devotion to black is less than five, Erebos isn't a creature. (Each |B in the mana costs of permanents you control counts toward your devotion to black.)
   * Your opponents can't gain life.
   * |1|B, Pay 2 life: Draw a card. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_BLACK, 0, 5);

	if( in_play(player, card) && event == EVENT_STATIC_EFFECTS && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) )
	{
		player_bits[1-player] |= PB_CANT_GAIN_LIFE;
	}

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XB(1, 1), 2, NULL, NULL);
}

int card_ereboss_emissary(int player, int card, event_t event){
/* Erebos's Emissary	|3|B
 * Enchantment Creature - Snake 3/3
 * Bestow |5|B
 * Discard a creature card: ~ gets +2/+2 until end of turn. If ~ is an Aura, enchanted creature gets +2/+2 until end of turn instead.
 * Enchanted creature gets +3/+3. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		if( ! has_subtype(player, card, SUBTYPE_AURA) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
			this_test.zone = TARGET_ZONE_HAND;

			int num_can_discard = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
			if (num_can_discard <= 0)
				return 0;

			return generic_shade_amt_can_pump(player, card, 2, 0, MANACOST0, num_can_discard);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return generic_creature_with_bestow(player, card, event, MANACOST_XB(5, 1), 3, 3, 0, 0);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			discard_card(player, selected);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( has_subtype(instance->parent_controller, instance->parent_card, SUBTYPE_AURA) && instance->damage_target_player > -1 ){
			pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
											instance->damage_target_player, instance->damage_target_card, 2, 2);
		}
		else{
			return generic_shade(player, card, event, 0, MANACOST0, 2, 2, 0, 0);
		}
	}

	return 0;
}

// fellhide minotaur --> vanilla

int card_fleshmad_steed(int player, int card, event_t event){
  /* Fleshmad Steed	|1|B
   * Creature - Horse 2/2
   * Whenever another creature dies, tap this creature. */

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.not_me = 1;
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	if( in_play(player, card) && resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		tap_card(player, card);
	}
	return 0;
}

int card_gray_merchant_of_asphodel(int player, int card, event_t event){
  /* Gray Merchant of Asphodel	|3|B|B
   * Creature - Zombie 2/4
   * When this creature enters, each opponent loses X life, where X is your devotion to black. You gain life equal to the life lost this way. (Each |B in the mana costs of permanents you control counts toward your devotion to black.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int result = lose_life(1-player, devotion(player, card, COLOR_BLACK, 0));
		gain_life(player, result);
	}

	return 0;
}

// hero's downfall --> dreadbore

int card_hythonia_the_cruel(int player, int card, event_t event){
  /* Hythonia the Cruel	|4|B|B
   * Legendary Creature - Gorgon 4/6
   * Deathtouch
   * |6|B|B: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
   * When Hythonia becomes monstrous, destroy all non-Gorgon creatures. */

	check_legend_rule(player, card, event);

	deathtouch(player, card, event);

	if( event == EVENT_BECAME_MONSTROUS ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_GORGON;
		this_test.subtype_flag = DOESNT_MATCH;

		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
	}

	return monstrosity(player, card, event, MANACOST_XB(6, 2), 3);
}

// insatiable harpy --> child of night

int card_keepsake_gorgon(int player, int card, event_t event){
  /* Keepsake Gorgon	|3|B|B
   * Creature - Gorgon 2/5
   * Deathtouch
   * |5|B|B: Monstrosity 1. (If this creature isn't monstrous, put a +1/+1 counter on it and it becomes monstrous.)
   * When this creature becomes monstrous, destroy target non-Gorgon creature an opponent controls. */

	deathtouch(player, card, event);

	if( event == EVENT_BECAME_MONSTROUS ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.required_subtype = SUBTYPE_GORGON;
		td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

		card_instance_t *instance = get_card_instance(player, card);

		instance->number_of_targets = 0;
		if (can_target(&td) && select_target(player, card, &td, "Select target non-Gorgon creature.", &(instance->targets[0]))){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return monstrosity(player, card, event, MANACOST_XB(5, 2), 1);
}

int card_lash_of_the_whip(int player, int card, event_t event){
	/* Lash of the Whip	|4|B
	 * Instant
	 * Target creature gets -4/-4 until end of turn. */
	return vanilla_instant_pump(player, card, event, ANYBODY, 1-player, -4, -4, 0, 0);
}

int card_loathsome_catoblepas(int player, int card, event_t event){
  /* Loathsome Catoblepas	|5|B
   * Creature - Beast 3/3
   * |2|G: This creature must be blocked this turn if able.
   * When this creature dies, target creature an opponent controls gets -3/-3 until end of turn. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance= get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			alternate_legacy_text(1, instance->damage_source_player,
								  pump_ability_until_eot(instance->damage_source_player, instance->damage_source_card,
														 instance->targets[0].player, instance->targets[0].card, -3, -3, 0, 0));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance= get_card_instance(player, card);
		alternate_legacy_text(2, instance->parent_controller,
							  pump_ability_until_eot(player, card, instance->parent_controller, instance->parent_card, 0, 0, 0, SP_KEYWORD_MUST_BE_BLOCKED));
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(2, 1), 0, NULL, NULL);
}

// march of the returned --> morbid plunder

int card_mogiss_marauder(int player, int card, event_t event){
  /* Mogis's Marauder	|2|B
   * Creature - Human Berserker 2/2
   * When this creature enters, up to X target creatures each gain intimidate and haste until end of turn, where X is your devotion to black. (A creature with intimidate can't be blocked except by artifact creatures and/or creatures that share a color with it. Each |B in the mana costs of permanents you control counts toward your devotion to black.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;

		card_instance_t *instance = get_card_instance( player, card );

		int max = devotion(player, card, COLOR_BLACK, 0);
		int trgs = 0;
		while( trgs < max && can_target(&td) ){
				if( select_target(player, card, &td, " Select target creature.", &(instance->targets[trgs])) ){
					state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
					trgs++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<trgs; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 0, 0, 0, SP_KEYWORD_HASTE+SP_KEYWORD_INTIMIDATE);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_nighthowler(int player, int card, event_t event){
  /* Nighthowler	|1|B|B
   * Enchantment Creature - Horror 0/0
   * Bestow |2|B|B (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached.)
   * This creature and enchanted creature each get +X/+X, where X is the number of creature cards in all graveyards. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && player > -1 && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result += count_graveyard_by_type(ANYBODY, TYPE_CREATURE);
	}
	return generic_creature_with_bestow(player, card, event, 2, 2, 0, 0, 0, 0,
										count_graveyard_by_type(ANYBODY, TYPE_CREATURE), count_graveyard_by_type(ANYBODY, TYPE_CREATURE),
										0, 0);
}

int card_ordeal_of_erebos(int player, int card, event_t event){
  /* Ordeal of Erebos	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature attacks, put a +1/+1 counter on it. Then if it has three or more +1/+1 counters on it, sacrifice this Aura.
   * When you sacrifice this Aura, target player discards two cards. */

	if (resolve_graveyard_trigger(player, card, event)){
		card_instance_t *instance = get_card_instance(player, card);
		instance->targets[11].player = -1;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		instance->number_of_targets = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
			new_multidiscard(instance->targets[0].player, 2, 0, player);
		}
		// Avoid confusing generic_aura()
		instance->targets[0].player = instance->damage_target_player;
		instance->targets[0].card = instance->damage_target_card;
	}

	return ordeal(player, card, event);
}

// Pharika's Cure --> vicious hunger

int card_read_the_bones(int player, int card, event_t event){
  /* Read the Bones	|2|B	=>theros.c:card_read_the_bones
   * Sorcery
   * Scry 2, then draw two cards. You lose 2 life. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += ((life[player]-2) > 6 ? 48 : -48);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		scry(player, 2);
		draw_cards(player, 2);
		lose_life(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int rescue_from_the_underworld_legacy(int player, int card, event_t event){
	if (current_turn == player && upkeep_trigger(player, card, event) && count_upkeeps(player) > 0){
		card_instance_t* leg = get_card_instance(player, card);

		// First the targeted creature
		int selected = validate_target_from_grave(player, card, player, 0);
		if (selected != -1){
			reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			if (leg->targets[2].player == player
				&& leg->targets[0].player < leg->targets[1].player){
				// This will almost always be true, since the targeted creature was already in the grave before the sacrifice
				--leg->targets[1].player;
			}
		}

		if (leg->targets[1].player >= 0){
			selected = validate_target_from_grave(player, card, leg->targets[2].player, 1);
			if (selected != -1){
				int card_maybe_added = card;
				if (leg->targets[2].player != player){
					// reanimate_permanent() will create an empty effect card attached to cards animated from opponent's graveyard; make sure it's not blank
					card_maybe_added = add_card_to_hand(player, get_internal_card_id_from_csv_id(CARD_ID_RESCUE_FROM_THE_UNDERWORLD));
					get_card_instance(player, card_maybe_added)->state |= STATE_INVISIBLE;
					--hand_count[player];
				}
				reanimate_permanent(player, card_maybe_added, leg->targets[2].player, selected, REANIMATE_DEFAULT);
				if (card_maybe_added != card){
					obliterate_card(player, card_maybe_added);
				}
			}
		}

		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_rescue_from_the_underworld(int player, int card, event_t event){
/* Rescue from the Underworld	|4|B
 * Instant
 * As an additional cost to cast ~, sacrifice a creature.
 * Choose target creature card in your graveyard. Return that card and the sacrificed card to the battlefield under your control at
 *		the beginning of your next upkeep. Exile ~. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select target creature card.");
	this_test.ai_selection_mode = AI_MAX_CMC;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_CAN_CAST ){
		if( can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 0, &this_test);
		}
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test2;
		new_default_test_definition(&test2, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test2);
		if (!sac){
			cancel = 1;
			return 0;
		}
		if( select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &this_test, 1) != -1 ){
			instance->targets[2].player = get_owner(BYTE2(sac), BYTE3(sac));
			instance->targets[2].card = cards_data[get_original_internal_card_id(BYTE2(sac), BYTE3(sac))].id;
			kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
		}
		else{
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			spell_fizzled = 1;
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 1);
		if( selected != -1 ){
			int legacy = create_legacy_effect(player, card, &rescue_from_the_underworld_legacy);
			card_instance_t *leg = get_card_instance( player, legacy );
			leg->targets[0].player = selected;
			leg->targets[0].card = get_grave(player)[selected];

			selected = seek_grave_for_id_to_reanimate(player, card, instance->targets[2].player, instance->targets[2].card, REANIMATEXTRA_LEAVE_IN_GRAVEYARD);
			if (selected == -1){
				leg->targets[1].player = leg->targets[1].card = -1;
			} else {
				leg->targets[1].player = selected;
				leg->targets[1].card = get_grave(instance->targets[2].player)[selected];
				leg->targets[2].player = instance->targets[2].player;
			}
			kill_card(player, card, KILL_EXILE);
		} else {
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return 0;
}

int card_returned_centaur(int player, int card, event_t event){
  /* Returned Centaur	|3|B	=>theros.c:card_returned_centaur
   * Creature - Zombie Centaur 2/4
   * When ~ enters the battlefield, target player puts the top four cards of his or her library into his or her graveyard. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, 0);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_PLAYER")){
			mill(instance->targets[0].player, 4);
		}

		instance->number_of_targets = 0;
	}

	return 0;
}

int card_returned_phalanx(int player, int card, event_t event){
  /* Returned Phalanx	|1|B
   * Creature - Zombie Soldier 3/3
   * Defender
   * |1|U: This creature can attack this turn as though it didn't have defender. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t* instance = get_card_instance(player, card);
		create_targetted_legacy_effect(player, instance->parent_card, &effect_defender_can_attack_until_eot, player, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, 0, 0);
}

int card_scourgemark(int player, int card, event_t event){
  /* Scourgemark	|1|B
   * Enchantment - Aura
   * Enchant creature
   * When this Aura enters, draw a card.
   * Enchanted creature gets +1/+0. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	return generic_aura(player, card, event, player, 1, 0, 0, 0, 0, 0, 0);
}

int card_sip_of_hemlock(int player, int card, event_t event){
  /* Sip of Hemlock	|4|B|B
   * Sorcery
   * Destroy target creature. Its controller loses 2 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			lose_life(instance->targets[0].player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_tormented_hero(int player, int card, event_t event){
  /* Tormented Hero	|B
   * Creature - Human Warrior 2/1
   * This creature enters tapped.
   * Heroic - Whenever you cast a spell that targets this creature, each opponent loses 1 life. You gain life equal to the life lost this way. */

	comes_into_play_tapped(player, card, event);

	if( heroic(player, card, event) ){
		int result = lose_life(1-player, 1);
		gain_life(player, result);
	}

	return 0;
}

int card_vipers_kiss(int player, int card, event_t event){
  /* Viper's Kiss	|B
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -1/-1, and its activated abilities can't be activated. */

	return generic_aura(player, card, event, 1 - player, -1, -1, 0, 0, 0, 0, GA_FORBID_ALL_ACTIVATED_ABILITIES);
}

int card_whip_of_erebos(int player, int card, event_t event){
  /* Whip of Erebos	|2|B|B
   * Legendary Enchantment Artifact
   * Creatures you control have lifelink.
   * |2|B|B, |T: Return target creature card from your graveyard to the battlefield. It gains haste. Exile it at the beginning of the next end step. If it would leave the battlefield, exile it instead of putting it anywhere else. Activate only as a sorcery. */

	check_legend_rule(player, card, event);

	damage_effects(player, card, event);

	if (event == EVENT_ABILITIES
		&& ! is_humiliated(player, card, AL_STATIC)
		&& in_play(player, card)
		&& affected_card_controller == player
		&& is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
	{
		lifelink(affected_card_controller, affected_card, event);
	}

	if( event == EVENT_CAN_CAST ){
		return global_enchantment(player, card, event);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			card_instance_t* instance = get_card_instance(player, card);
			reanimate_permanent(player, instance->parent_card, player, selected, REANIMATE_UNEARTH);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED|GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XB(2, 2), 0,
															NULL, NULL, 0, &this_test, GS_GRAVE_RECYCLER);
}

// Red

int card_akroan_crusader(int player, int card, event_t event){
	/* Akroan Crusader	|R
	 * Creature - Human Soldier 1/1
	 * Heroic - Whenever you cast a spell that targets ~, put a 1/1 |Sred Soldier creature token with haste onto the battlefield. */

	if( heroic(player, card, event) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.pow = token.tou = 1;
		token.s_key_plus = SP_KEYWORD_HASTE;
		token.color_forced = COLOR_TEST_RED;
		generate_token(&token);
	}

	return 0;
}

int card_anger_of_the_gods(int player, int card, event_t event){
  /* Anger of the Gods	|1|R|R
   * Sorcery
   * ~ deals 3 damage to each creature. If a creature dealt damage this way would die this turn, exile it instead. */

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 3, NDA_EXILE_IF_FATALLY_DAMAGED | NDA_ALL_CREATURES, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_arena_athlete(int player, int card, event_t event){
  /* Arena Athlete	|1|R
   * Creature - Human 2/1
   * Heroic - Whenever you cast a spell that targets this creature, target creature an opponent controls can't block this turn. */

	if (heroic(player, card, event)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_CANNOT_BLOCK);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

// borderland minotaur --> vanilla

int card_boulderfall(int player, int card, event_t event){
	/* Boulderfall	|6|R|R	0x200af61
	 * Instant
	 * ~ deals 5 damage divided as you choose among any number of targets. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int trgs;
		for (trgs = 0; trgs < 5; ++trgs){
			if (!new_pick_target(&td, "TARGET_ANY", trgs, 1)){
				return 0;
			}
		}
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_coordinated_assault(int player, int card, event_t event){
  /* Coordinated Assault	|R
   * Instant
   * Up to two target creatures each get +1/+0 and gain first strike until end of turn. (They deal combat damage before creatures without first strike.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 1, 0, KEYWORD_FIRST_STRIKE, 0);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

int card_deathbellow_raider(int player, int card, event_t event){
  /* Deathbellow Raider	|1|R
   * Creature - Minotaur Berserker 2/3
   * This creature attacks each combat if able.
   * |2|B: Regenerate this creature. */

	attack_if_able(player, card, event);
	return regeneration(player, card, event, MANACOST_XB(2, 1));
}

int card_dragon_mantle(int player, int card, event_t event){
	/* Dragon Mantle	|R
	 * Enchantment - Aura
	 * Enchant creature
	 * When ~ enters the battlefield, draw a card.
	 * Enchanted creature has "|R: This creature gets +1/+0 until end of turn." */

	if (IS_GAA_EVENT(event)){
		card_instance_t *instance = get_card_instance(player, card);

		if( instance->damage_target_player > -1 ){
			int t_player = instance->damage_target_player;
			int t_card = instance->damage_target_card;
			if( event == EVENT_RESOLVE_ACTIVATION ){
				pump_until_eot_merge_previous(player, card, t_player, t_card, 1, 0);
			}
			return granted_generic_activated_ability(player, card, t_player, t_card, event, 0,
													 MANACOST_R(1), 0, NULL, NULL);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_cards(player, 1);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_ember_swallower(int player, int card, event_t event){
  /* Ember Swallower	|2|R|R
   * Creature - Elemental 4/5
   * |5|R|R: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
   * When this creature becomes monstrous, each player sacrifices three lands of their choice. */

	if( event == EVENT_BECAME_MONSTROUS ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		test.qty = 3;
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);
	}

	return monstrosity(player, card, event, 5, 0, 0, 0, 2, 0, 3);
}

int card_fanatic_of_mogis(int player, int card, event_t event){
  /* Fanatic of Mogis	|3|R
   * Creature - Minotaur Shaman 4/2
   * When this creature enters, it deals damage to each opponent equal to your devotion to red. (Each |R in the mana costs of permanents you control counts toward your devotion to red.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		damage_player(1-player, devotion(player, card, COLOR_RED, 0), player, card);
	}

	return 0;
}

int card_firedrinker_satyr(int player, int card, event_t event){
  /* Firedrinker Satyr |R
   * Creature — Satyr Shaman 2/1
   * Whenever Firedrinker Satyr is dealt damage, it deals that much damage to you.
   * {1}{R}: Firedrinker Satyr gets +1/+0 until end of turn and deals 1 damage to you. */

	if (!is_humiliated(player, card, AL_TRIG) && damage_dealt_to_me_arbitrary(player, card, event, DDBM_0, player, card)){
		card_instance_t* instance = get_card_instance(player, card);
		int amount = instance->targets[7].card;
		instance->targets[7].card = 0;
		damage_player(player, amount, player, card);
	}

	if (event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 1, 0);
		damage_player(player, 1, player, instance->parent_card);
		return 0;
	}

	return generic_shade(player, card, event, 0, MANACOST_XR(1, 1), 1, 0, 0, 0);
}

int card_flamespeaker_adept(int player, int card, event_t event)
{
  /* Flamespeaker Adept	|2|R
   * Creature - Human Shaman 2/3
   * Whenever you scry, ~ gets +2/+0 and gains first strike until end of turn. */

  if (xtrigger_condition() == XTRIGGER_SCRY && affect_me(player, card) && reason_for_trigger_controller == player
	  && trigger_cause_controller == player && !is_humiliated(player, card, AL_TRIG))
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER)
		pump_ability_until_eot(player, card, player, card, 2, 0, KEYWORD_FIRST_STRIKE, 0);
	}

  return 0;
}

int card_hammer_of_purphoros(int player, int card, event_t event){
  /* Hammer of Purphoros	|1|R|R
   * Legendary Enchantment Artifact
   * Creatures you control have haste.
   * |2|R, |T, Sacrifice a land: Create a 3/3 colorless Golem enchantment artifact creature token. */

	check_legend_rule(player, card, event);

	boost_subtype(player, card, event, -1, 0, 0, 0, SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY|BCT_INCLUDE_SELF);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_GOLEM, &token);
		token.special_flags2 = SF2_ENCHANTED_EVENING;
		generate_token(&token);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_UNTAPPED, MANACOST_XR(2, 1), 0, NULL, NULL,
															0, &test, 0);
}

int card_ill_tempered_cyclops(int player, int card, event_t event){
  /* Ill-Tempered Cyclops	|3|R
   * Creature - Cyclops 3/3
   * Trample
   * |5|R: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.) */

	return monstrosity(player, card, event, MANACOST_XR(5, 1), 3);
}

int card_labyrinth_champion(int player, int card, event_t event){
	/* Labyrinth Champion	|3|R	0x200af93
	 * Creature - Human Warrior 2/2
	 * Heroic - Whenever you cast a spell that targets ~, ~ deals 2 damage to any target. */
	if( heroic(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, 2);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

// lightning striker --> lighting bolt

int card_messengers_speed(int player, int card, event_t event){
  /* Messenger's Speed	|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has trample and haste. */

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_TRAMPLE, SP_KEYWORD_HASTE, 0, 0, 0);
}

int card_minotaur_skullcleaver(int player, int card, event_t event){
  /* Minotaur Skullcleaver	|2|R
   * Creature - Minotaur Berserker 2/2
   * Haste
   * When this creature enters, it gets +2/+0 until end of turn. */

	haste(player, card, event);

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		pump_until_eot(player, card, player, card, 2, 0);
	}

	return 0;
}

int card_ordeal_of_purphoros(int player, int card, event_t event){
	/* Ordeal of Purphoros	|1|R	0x200f584
	 * Enchantment - Aura
	 * Enchant creature
	 * Whenever enchanted creature attacks, put a +1/+1 counter on it. Then if it has three or more +1/+1 counters on it, sacrifice ~.
	 * When you sacrifice ~, it deals 3 damage to any target. */
	if (resolve_graveyard_trigger(player, card, event)){
		card_instance_t* instance = get_card_instance(player, card);
		instance->targets[11].player = -1;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;

		instance->number_of_targets = 0;
		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, 3);
		}
		// Avoid confusing generic_aura()
		instance->targets[0].player = instance->damage_target_player;
		instance->targets[0].card = instance->damage_target_card;
	}

	return ordeal(player, card, event);
}

int card_peak_eruption(int player, int card, event_t event){
  /* Peak Eruption	|2|R
   * Sorcery
   * Destroy target Mountain. ~ deals 3 damage to that land's controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			damage_player(instance->targets[0].player, 3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, get_hacked_land_text(player, card, "Select target %s.", SUBTYPE_MOUNTAIN),
						1, NULL);
}

int card_portent_of_betrayal(int player, int card, event_t event){
  /* Portent of Betrayal	|3|R
   * Sorcery
   * Gain control of target creature until end of turn. Untap that creature. It gains haste until end of turn. Scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return aot_like_spell(player, card, event, &td, "TARGET_CREATURE", 0);
}

int card_priest_of_iroas(int player, int card, event_t event){
  /* Priest of Iroas	|R
   * Creature - Human Cleric 1/1
   * |3|W, Sacrifice this creature: Destroy target enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST_XW(3, 1), 0, &td, "TARGET_ENCHANTMENT");
}

int card_purphoros_god_of_the_forge(int player, int card, event_t event){
  /* Purphoros, God of the Forge	|3|R
   * Legendary Enchantment Creature - God 6/5
   * Indestructible
   * As long as your devotion to red is less than five, Purphoros isn't a creature.
   * Whenever another creature you control enters, Purphoros deals 2 damage to each opponent.
   * |2|R: Creatures you control get +1/+0 until end of turn. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_RED, 0, 5);

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;
		etb_ability_global(player, event, RESOLVE_TRIGGER_MANDATORY, 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		damage_player(1-player, 2, player, card);
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 1, 0, 0, 0);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XR(2, 1), 0, NULL, NULL);
}

int card_purphoross_emissary(int player, int card, event_t event){
	/* Purphoros's Emissary	|3|R
	 * Enchantment Creature - Ox 3/3
	 * Bestow |6|R
	 * Menace
	 * Enchanted creature gets +3/+3 and has menace. */

	menace(player, card, event);

	if( get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		menace(instance->damage_target_player, instance->damage_target_card, event);
	}

	return generic_creature_with_bestow(player, card, event, MANACOST_XR(6, 1), 3, 3, 0, 0);
}

int card_rage_of_purphoros(int player, int card, event_t event){
  /* Rage of Purphoros	|4|R
   * Sorcery
   * ~ deals 4 damage to target creature. It can't be regenerated this turn. Scry 1. (Look at the top card of your library. You may put that card on the bottom.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			cannot_regenerate_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
			damage_target0(player, card, 4);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_rageblood_shaman(int player, int card, event_t event){
  /* Rageblood Shaman	|1|R|R
   * Creature - Minotaur Shaman 2/3
   * Trample
   * Other Minotaur creatures you control get +1/+1 and have trample. */

	return boost_creature_type(player, card, event, SUBTYPE_MINOTAUR, 1, 1, KEYWORD_TRAMPLE, BCT_CONTROLLER_ONLY);
}

// Satyr rambler --> vanilla

int card_spark_jolt(int player, int card, event_t event){
	/* Spark Jolt	|R	0x200afca
	 * Instant
	 * ~ deals 1 damage to any target. Scry 1. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
			scry(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_spearpoint_oread(int player, int card, event_t event){
  /* Spearpoint Oread	|2|R
   * Enchantment Creature - Nymph 2/2
   * Bestow |5|R (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached.)
   * First strike
   * Enchanted creature gets +2/+2 and has first strike. */

	return generic_creature_with_bestow(player, card, event, MANACOST_XR(5, 1), 2, 2, KEYWORD_FIRST_STRIKE, 0);
}

static int effect_creatures_without_flying_cant_block_this_turn(int player, int card, event_t event)
{
  if (event == EVENT_BLOCK_LEGALITY && affected_card_controller != player && !check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING))
	event_result = 1;

  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_stoneshock_giant(int player, int card, event_t event)
{
  /* Stoneshock Giant	|3|R|R
   * Creature - Giant 5/4
   * |6|R|R: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
   * When this creature becomes monstrous, creatures without flying your opponents control can't block this turn. */

  if (event == EVENT_BECAME_MONSTROUS)
	create_legacy_effect(player, card, effect_creatures_without_flying_cant_block_this_turn);

  return monstrosity(player, card, event, MANACOST_XR(6, 2), 3);
}

int card_stormbreath_dragon(int player, int card, event_t event){
  /* Stormbreath Dragon	|3|R|R
   * Creature - Dragon 4/4
   * Flying, haste, protection from white
   * |5|R|R: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
   * When this creature becomes monstrous, it deals damage to each opponent equal to the number of cards in that player's hand. */

	haste(player, card, event);

	if( event == EVENT_BECAME_MONSTROUS ){
		if( hand_count[1-player] > 0 ){
			damage_player(1-player, hand_count[1-player], player, card);
		}
	}

	return monstrosity(player, card, event, MANACOST_XR(5, 2), 3);
}

static const char* can_use_titan_of_eternal_fire_ability(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(	generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_R(1), 0, &td1, NULL) ){
		return NULL;
	}
	return "you can't activate this.";
}

int card_titan_of_eternal_fire(int player, int card, event_t event){
	/* Titan of Eternal Fire	|5|R	0x200afd9
	 * Creature - Giant 5/6
	 * Each Human creature you control has "|R, |T: This creature deals 1 damage to any target." */
	if( ! in_play(player, card) || is_humiliated(player, card, AL_STATIC) || ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;
	td.required_subtype = SUBTYPE_HUMAN;
	td.extra = target_predicate_extra(can_use_titan_of_eternal_fire_ability);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.illegal_abilities = 0;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		return can_target(&td) ? 1 : 0;
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		if( new_pick_target(&td, "Select a Human creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
			charge_mana_for_activated_ability(player, card, MANACOST_R(1));
			td1.illegal_abilities = instance->info_slot = get_protections_from(instance->targets[0].player, instance->targets[0].card);
			if( can_target(&td1) && new_pick_target(&td1, "TARGET_ANY", 1, 0) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		td1.illegal_abilities = instance->info_slot;
		if( valid_target(&td1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, 1, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_titans_strength(int player, int card, event_t event){
	/* Titan's Strength	|R
	 * Instant
	 * Target creature gets +3/+1 until end of turn. Scry 1. */
	return vanilla_instant_pump(player, card, event, ANYBODY, player, 3, 1, VANILLA_PUMP_SCRY_1, 0);
}

// two-headed cerberus --> vanilla

// wild celebrants --> batterhorn

// Green

int card_agent_of_horizons(int player, int card, event_t event){
  /* Agent of Horizons	|2|G
   * Creature - Human Rogue 3/2
   * |2|U: This creature can't be blocked this turn. */

	return generic_shade(player, card, event, 0, MANACOST_XU(2, 1), 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
}

int card_anthousa_setessan_hero(int player, int card, event_t event){
  /* Anthousa, Setessan Hero	|3|G|G
   * Legendary Creature - Human Warrior 4/5
   * Heroic - Whenever you cast a spell that targets Anthousa, up to three target lands you control each become 2/2 Warrior creatures until end of turn. They're still lands. */

	check_legend_rule(player, card, event);

	if( heroic(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.illegal_type = TYPE_CREATURE;
		td.preferred_controller = player;
		td.allowed_controller = player;

		card_instance_t *instance = get_card_instance(player, card);

		int trgs;
		for (trgs = 0; trgs < 3 && can_target(&td) && pick_target(&td, "TARGET_LAND"); ++trgs){
			int legacy = land_animation2(player, card, instance->targets[0].player, instance->targets[0].card, 1, 2, 2, 0, 0, 0, NULL);
			get_card_instance(player, legacy)->targets[9].card = SUBTYPE_WARRIOR;
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_arbor_colossus(int player, int card, event_t event){
  /* Arbor Colossus	|2|G|G|G
   * Creature - Giant 6/6
   * Reach
   * |3|G|G|G: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
   * When this creature becomes monstrous, destroy target creature with flying an opponent controls. */

	if (event == EVENT_BECAME_MONSTROUS){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.required_abilities = KEYWORD_FLYING;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_CREATURE")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return monstrosity(player, card, event, MANACOST_XG(3, 3), 3);
}

int card_artisans_sorrow(int player, int card, event_t event){
  /* Artisan's Sorrow	|3|G
   * Instant
   * Destroy target artifact or enchantment. Scry 2. (Look at the top two cards of your library, then put any number of them on the bottom and the rest on top in any order.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			scry(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_boon_satyr(int player, int card, event_t event){
  /* Boon Satyr	|1|G|G
   * Enchantment Creature - Satyr 4/2
   * Flash
   * Bestow |3|G|G (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached.)
   * Enchanted creature gets +4/+2. */

	return generic_creature_with_bestow(player, card, event, MANACOST_XG(3, 2), 4, 2, 0, 0);
}

int card_bow_of_nylea(int player, int card, event_t event){
  /* Bow of Nylea	|1|G|G
   * Legendary Enchantment Artifact
   * Attacking creatures you control have deathtouch.
   * |1|G, |T: Choose one -
   * * Put a +1/+1 counter on target creature.
   * * ~ deals 2 damage to target creature with flying.
   * * You gain 3 life.
   * * Put up to four target cards from your graveyard on the bottom of your library in any order. */

	check_legend_rule(player, card, event);

	if (event == EVENT_ABILITIES && ! is_humiliated(player, card, AL_STATIC) ){
		if (affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
			&& is_attacking(affected_card_controller, affected_card) && in_play(player, card)){
			deathtouch(affected_card_controller, affected_card, event);
		}
	}

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		target_definition_t td_counter;
		default_target_definition(player, card, &td_counter, TYPE_CREATURE);
		td_counter.preferred_controller = player;

		target_definition_t td_flying;
		default_target_definition(player, card, &td_flying, TYPE_CREATURE);
		td_flying.required_abilities = KEYWORD_FLYING;

		card_instance_t* instance = get_card_instance(player, card);

		typedef enum{
			CHOICE_COUNTER = 1,
			CHOICE_DAMAGE = 2,
			CHOICE_LIFE = 3,
			CHOICE_GRAVEYARD = 4
		} Choice;

		Choice choice = DIALOG(player, card, event,
							   "+1/+1 counter", can_target(&td_counter), 3,
							   "Damage a flying creature", can_target(&td_flying), 4,
							   "Gain 3 life", 1, life[player] < 6 ? 5 : 2,
							   "Recycle graveyard", !graveyard_has_shroud(player), count_graveyard(player) > 0 && count_deck(player) < 2 ? 6 : 1);

		if (event == EVENT_CAN_ACTIVATE){
			return choice && generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_NONSICK, MANACOST_XG(1, 1), 0, 0, 0);
		} else if (event == EVENT_ACTIVATE){
			instance->number_of_targets = 0;

			if (charge_mana_for_activated_ability(player, card, MANACOST_XG(1, 1))){
				switch (choice){
					case CHOICE_COUNTER:	pick_target(&td_counter, "TARGET_CREATURE");	break;
					case CHOICE_DAMAGE:		pick_target(&td_flying, "TARGET_CREATURE");		break;
					case CHOICE_LIFE:		break;
					case CHOICE_GRAVEYARD:
						select_multiple_cards_from_graveyard(player, player, 0, AI_MAX_VALUE, NULL, 4, &instance->targets[0]);
						break;
				}
				if (spell_fizzled != 1){
					tap_card(player, card);
				}
			}
		} else {	// EVENT_RESOLVE_ACTIVATION
			switch (choice){
				case CHOICE_COUNTER:
					if (valid_target(&td_counter)){
						add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P1_P1, 1);
					}
					break;

				case CHOICE_DAMAGE:
					if (valid_target(&td_flying)){
						damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, instance->parent_card);
					}
					break;

				case CHOICE_LIFE:
					gain_life(player, 3);
					break;

				case CHOICE_GRAVEYARD:{
					int i, num_validated = 0;
					for (i = 0; i < 4; ++i){
						int selected = validate_target_from_grave(player, card, player, i);
						if (selected != -1){
							from_graveyard_to_deck(player, selected, 2);
							++num_validated;
						}
					}
					if (num_validated > 1){
						rearrange_bottom_x(player, player, num_validated);
					}
					break;
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_centaur_battlemaster(int player, int card, event_t event){
  /* Centaur Battlemaster	|3|G|G
   * Creature - Centaur Warrior 3/3
   * Heroic - Whenever you cast a spell that targets this creature, put three +1/+1 counters on this creature. */

	if( heroic(player, card, event) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 3);
	}

	return 0;
}

int card_commune_with_the_gods(int player, int card, event_t event){
  /* Commune with the Gods	|1|G
   * Sorcery
   * Reveal the top five cards of your library. You may put a creature or enchantment card from among them into your hand. Put the rest into your graveyard. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		int amount = 5;
		if( amount > count_deck(player) ){
			amount = count_deck(player);
		}

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_ENCHANTMENT, "Select a creature or enchantment card.");
		this_test.create_minideck = amount;
		this_test.no_shuffle = 1;

		if( amount > 0 ){
			if( player == AI ){
				show_deck( HUMAN, deck_ptr[player], amount, "Cards revealed by Commune with the Gods.", 0, gs_done_008b40e0 );
			}
			if( new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test) >= 0 ){
				amount--;
			}
			if( amount > 0 ){
				mill(player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_defend_the_hearth(int player, int card, event_t event){
  /* Defend the Hearth |1|G
   * Instant
   * Prevent all combat damage that would be dealt to players this turn. */

	if( event == EVENT_RESOLVE_SPELL ){
		fog_special(player, card, ANYBODY, FOG_ONLY_DAMAGE_TO_T_PLAYER | FOG_COMBAT_DAMAGE_ONLY);
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_fog(player, card, event);
}

int card_fade_into_antiquity(int player, int card, event_t event){
  /* Fade into Antiquity	|2|G
   * Sorcery
   * Exile target artifact or enchantment. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_feral_invocation(int player, int card, event_t event){
  /* Feral Invocation	|2|G
   * Enchantment - Aura
   * Flash (You may cast this spell any time you could cast an instant.)
   * Enchant creature
   * Enchanted creature gets +2/+2. */

	flash(player, card, event);
	return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
}

int card_hunt_the_hunter(int player, int card, event_t event){
	/* Hunt the Hunter	|G
	 * Sorcery
	 * Target |Sgreen creature you control gets +2/+2 until end of turn. It fights target |Sgreen creature an opponent controls. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = 1-player;
	td2.preferred_controller = 1-player;
	td2.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td2) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		char msg[100];
		scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target %s creature you control.", COLOR_GREEN));
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, msg, 1, NULL);
		if( spell_fizzled != 1 ){
			scnprintf(msg, 100, "%s", get_sleighted_color_text(player, card, "Select target %s creature your opponent controls.", COLOR_GREEN));
			new_pick_target(&td2, msg, 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
			if( validate_target(player, card, &td2, 1) ){
				fight(instance->targets[0].player, instance->targets[0].card, instance->targets[1].player, instance->targets[1].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_karametras_acolyte(int player, int card, event_t event){
  /* Karametra's Acolyte	|3|G
   * Creature - Human Druid 1/4
   * |T: Add an amount of |G equal to your devotion to green. (Each |G in the mana costs of permanents you control counts toward your devotion to green.) */

	if( event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if( event == EVENT_ACTIVATE ){
		produce_mana_tapped(player, card, COLOR_GREEN, devotion(player, card, COLOR_GREEN, 0));
	}

	if( event == EVENT_COUNT_MANA ){
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			declare_mana_available(player, COLOR_GREEN, devotion(player, card, COLOR_GREEN, 0));
		}
	}

	return 0;
}

int card_leafcrown_dryad(int player, int card, event_t event){
  /* Leafcrown Dryad	|1|G
   * Enchantment Creature - Nymph Dryad 2/2
   * Bestow |3|G (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached.)
   * Reach
   * Enchanted creature gets +2/+2 and has reach. */

	return generic_creature_with_bestow(player, card, event, MANACOST_XG(3, 1), 2, 2, KEYWORD_REACH, 0);
}

int card_mistcutter_hydra(int player, int card, event_t event){
  /* Mistcutter Hydra	|X|G
   * Creature - Hydra 0/0
   * This spell can't be countered.
   * Haste, protection from blue
   * This creature enters with X +1/+1 counters on it. */

	cannot_be_countered(player, card, event);
	haste(player, card, event);
	return card_ivy_elemental(player, card, event);
}

int card_nemesis_of_mortals(int player, int card, event_t event){
  /* Nemesis of Mortals	|4|G|G
   * Creature - Snake 5/5
   * This spell costs |1 less to cast for each creature card in your graveyard.
   * |7|G|G: Monstrosity 5. This ability costs |1 less to activate for each creature card in your graveyard. (If this creature isn't monstrous, put five +1/+1 counters on it and it becomes monstrous.) */

	if (event == EVENT_MODIFY_COST){
		COST_COLORLESS -= count_graveyard_by_type(player, TYPE_CREATURE);
	}

	if ((event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST) && !is_monstrous(player, card) )
	{
		int amount = 7 - count_graveyard_by_type(player, TYPE_CREATURE);
		if (amount < 0){
			amount = 0;
		}
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_XG(amount, 2), 0, NULL, NULL) ){
			return 5;
		}
	}

	if( IS_GAA_EVENT(event) ){
		int amount = 7 - count_graveyard_by_type(player, TYPE_CREATURE);
		if (amount < 0){
			amount = 0;
		}
		return monstrosity(player, card, event, MANACOST_XG(amount, 2), 5);
	}

	return 0;
}

int card_nessian_asp(int player, int card, event_t event){
  /* Nessian Asp	|4|G
   * Creature - Snake 4/5
   * Reach
   * |6|G: Monstrosity 4. (If this creature isn't monstrous, put four +1/+1 counters on it and it becomes monstrous.) */

	return monstrosity(player, card, event, MANACOST_XG(6, 1), 4);
}

int card_nylea_god_of_the_hunt(int player, int card, event_t event){
  /* Nylea, God of the Hunt	|3|G
   * Legendary Enchantment Creature - God 6/6
   * Indestructible
   * As long as your devotion to green is less than five, Nylea isn't a creature. (Each |G in the mana costs of permanents you control counts toward your devotion to green.)
   * Other creatures you control have trample.
   * |3|G: Target creature gets +2/+2 until end of turn. */

	check_legend_rule(player, card, event);

	indestructible(player, card, event);

	generic_creature_with_devotion(player, card, event, COLOR_GREEN, 0, 5);

	boost_creature_type(player, card, event, -1, 0, 0, KEYWORD_TRAMPLE, BCT_CONTROLLER_ONLY);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(3, 1), 0, &td, "TARGET_CREATURE");
}

int card_nyleas_disciple(int player, int card, event_t event){
  /* Nylea's Disciple	|2|G|G
   * Creature - Centaur Archer 3/3
   * When this creature enters, you gain life equal to your devotion to green. (Each |G in the mana costs of permanents you control counts toward your devotion to green.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, devotion(player, card, COLOR_GREEN, 0));
	}

	return 0;
}

int card_nyleas_emissary(int player, int card, event_t event){
  /* Nylea's Emissary	|3|G
   * Enchantment Creature - Cat 3/3
   * Bestow |5|G (If you cast this card for its bestow cost, it's an Aura spell with enchant creature. It becomes a creature again if it's not attached.)
   * Trample
   * Enchanted creature gets +3/+3 and has trample. */

	return generic_creature_with_bestow(player, card, event, MANACOST_XG(5, 1), 3, 3, KEYWORD_TRAMPLE, 0);
}

int card_nyleas_presence(int player, int card, event_t event)
{
  /* Nylea's Presence	|1|G
   * Enchantment - Aura
   * Enchant land
   * When this Aura enters, draw a card.
   * Enchanted land is every basic land type in addition to its other types. */

  card_instance_t *instance;
  int enchanted_land;
  int mana_ability_check;
  int enchanted_player;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
    draw_cards(player, 1);
  instance = in_play(player, card);
  if (instance != NULL &&
      (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    enchanted_player = (int)instance->damage_target_player;
    enchanted_land = instance->damage_target_card;
    if (event == EVENT_CAN_ACTIVATE)
    {
      mana_ability_check = is_tapped(enchanted_player, enchanted_land);
      if (mana_ability_check == 0 &&
          (mana_ability_check = is_animated_and_sick(enchanted_player, enchanted_land),
           mana_ability_check == 0) &&
          (mana_ability_check = can_produce_mana(enchanted_player, enchanted_land),
           mana_ability_check != 0))
        return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      produce_mana_tapped_all_one_color(enchanted_player, enchanted_land, COLOR_TEST_ANY_COLORED,
                                        1);
      dispatch_event(enchanted_player, enchanted_land, 0x81);
    }
    else if (event == EVENT_COUNT_MANA)
    {
      if (card == affected_card && player == affected_card_controller &&
          (mana_ability_check = is_tapped(enchanted_player, enchanted_land),
           mana_ability_check == 0) &&
          (mana_ability_check = is_animated_and_sick(enchanted_player, enchanted_land),
           mana_ability_check == 0) &&
          (mana_ability_check = can_produce_mana(enchanted_player, enchanted_land),
           mana_ability_check != 0))
        mana_producer(player, card, EVENT_COUNT_MANA);
    }
    set_special_flags2(enchanted_player, enchanted_land, 0x20);
    leaves_play(player, card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
      remove_special_flags2(enchanted_player, enchanted_land, 0x20);
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  default_target_definition(player, card, &td, 1);
  td.allowed_controller = player;
  td.preferred_controller = player;
  return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

int card_ordeal_of_nylea(int player, int card, event_t event){

	/* Ordeal of Nylea	|1|G
	 * Enchantment - Aura
	 * Enchant creature
	 * Whenever enchanted creature attacks, put a +1/+1 counter on it. Then if it has three or more +1/+1 counters on it, sacrifice ~.
	 * When you sacrifice ~, search your library for up to two basic land cards, put them onto the battlefield tapped, then shuffle your library. */

	if (resolve_graveyard_trigger(player, card, event)){
		get_card_instance(player, card)->targets[11].player = -1;

		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 2);
	}

	return ordeal(player, card, event);
}

// pheres-band centaur --> vanilla

int card_polukranos_world_eater(int player, int card, event_t event){
  /* Polukranos, World Eater	|2|G|G
   * Legendary Creature - Hydra 5/5
   * |X|X|G: Monstrosity X. (If this creature isn't monstrous, put X +1/+1 counters on it and it becomes monstrous.)
   * When Polukranos becomes monstrous, it deals X damage divided as you choose among any number of target creatures your opponents control. Each of those creatures deals damage equal to its power to Polukranos. */

	check_legend_rule(player, card, event);

	if (event == EVENT_BECAME_MONSTROUS){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.preferred_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		int i = 0;
		while (i < instance->info_slot && can_target(&td)){
				int pos = instance->number_of_targets;
				char buffer[100];
				scnprintf(buffer, 100, "Select target creature an opponent controls (%d of %d).", i+1, instance->info_slot);
				if( new_pick_target(&td, buffer, pos, GS_LITERAL_PROMPT) ){
					add_state(instance->targets[pos].player, instance->targets[pos].card, STATE_TARGETTED);
					i++;
				}
		}

		for (i = 0; i < active_cards_count[1 - player]; ++i){
			if (in_play(1 - player, i) && is_what(1 - player, i, TYPE_CREATURE) && check_state(1 - player, i, STATE_TARGETTED)){
				remove_state(1 - player, i, STATE_TARGETTED);
				int q, dmg = 0;
				for(q=0; q<instance->number_of_targets; q++){
					if( instance->targets[q].card == i ){
						++dmg;
					}
				}
				damage_creature(1 - player, i, dmg, player, card);
				damage_creature(player, card, get_power(1 - player, i), 1 - player, i);
			}
		}
	}

	return monstrosity(player, card, event, MANACOST_XG(-2, 1), get_card_instance(player, card)->info_slot);
}

int card_reverent_hunter(int player, int card, event_t event){
  /* Reverent Hunter	|2|G
   * Creature - Human Archer 1/1
   * When this creature enters, put a number of +1/+1 counters on it equal to your devotion to green. (Each |G in the mana costs of permanents you control counts toward your devotion to green.) */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		add_counters(player, card, player, card, COUNTER_P1_P1, devotion(player, card, COLOR_GREEN, 0));
	}

	return 0;
}

int card_satyr_hedonist(int player, int card, event_t event){
  /* Satyr Hedonist	|1|G
   * Creature - Satyr 2/1
   * |R, Sacrifice this creature: Add |R|R|R. */

	if( event == EVENT_CAN_ACTIVATE && can_produce_mana(player, card) && get_card_instance(player, card)->targets[1].player != 66 &&
		has_mana(player, COLOR_RED, 1) )
	{
		return can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance= get_card_instance(player, card);
		instance->targets[1].player = 66;
		charge_mana(player, COLOR_RED, 1);
		if( spell_fizzled != 1 ){
			produce_mana(player, COLOR_RED, 3);
			tapped_for_mana_color = -2;
			kill_card(player, card, KILL_SACRIFICE);
		}
		instance->targets[1].player = 0;
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card) &&
		get_card_instance(player, card)->targets[1].player != 66 && has_mana(player, COLOR_RED, 1) )
	{
		declare_mana_available(player, COLOR_RED, 3);
	}

	return 0;
}

int card_satyr_piper(int player, int card, event_t event){
  /* Satyr Piper	|2|G
   * Creature - Satyr Rogue 2/1
   * |3|G: Target creature must be blocked this turn if able. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_MUST_BE_BLOCKED);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XG(3, 1), 0, &td, "TARGET_CREATURE");
}

// sedge scorpion --> Deathgaze Cockatrice

int card_shredding_winds(int player, int card, event_t event){
  /* Shredding Winds	|2|G
   * Instant
   * ~ deals 7 damage to target creature with flying. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_abilities = KEYWORD_FLYING;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 7);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature with flying.", 1, NULL);
}

int card_staunch_hearted_warrior(int player, int card, event_t event){

	/* Pheres-Band Thunderhoof	|4|G
	 * Creature - Centaur Warrior 3/4
	 * Heroic - Whenever you cast a spell that targets ~, put two +1/+1 counters on ~. */

	/* Setessan Oathsworn	|1|G|G
	 * Creature - Satyr Warrior 1/1
	 * Heroic - Whenever you cast a spell that targets ~, put two +1/+1 counters on ~. */

	/* Staunch-Hearted Warrior	|3|G
	 * Creature - Human Warrior 2/2
	 * Heroic - Whenever you cast a spell that targets ~, put two +1/+1 counters on ~. */


	if( heroic(player, card, event) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 2);
	}

	return 0;
}

int card_sylvan_caryatid(int player, int card, event_t event){
  /* Sylvan Caryatid	|1|G
   * Creature - Plant 0/3
   * Defender, hexproof
   * |T: Add one mana of any color. */

	hexproof(player, card, event);
	return mana_producing_creature_all_one_color(player, card, event, 12, COLOR_TEST_ANY_COLORED, 1);
}

static int time_to_feed_legacy(int player, int card, event_t event)
{

  card_instance_t *instance = get_card_instance(player, card);
  if (instance->damage_target_player >= 0)
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_GRAVEYARD_FROM_PLAY)
    {
      int c = instance->damage_target_card;
      int p = instance->damage_target_player;
      if (affect_me(p, c) && !check_for_death_trigger_removal(p, c) && is_this_dying(p, c))
      {
        instance->damage_target_card = -1;
        instance->damage_target_player = -1;
        instance->targets[11].player = 66;
      }
    }
  }

  if (resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY))
  {
    gain_life(player, 3);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_time_to_feed(int player, int card, event_t event){
/* Time to Feed	|2|G
 * Sorcery
 * Choose target creature an opponent controls. When that creature dies this turn, you gain 3 life.
 * Target creature you control fights that creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = 1-player;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = td2.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td2) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature an opponent controls.", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td2, "Select target creature you control.", 1, 1 | GS_LITERAL_PROMPT);
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			create_targetted_legacy_effect(player, card, &time_to_feed_legacy, instance->targets[0].player, instance->targets[0].card);
			if( validate_target(player, card, &td2, 1) ){
				fight(instance->targets[1].player, instance->targets[1].card, instance->targets[0].player, instance->targets[0].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// voyaging satyr --> seeker of skybreak

// vulpine goliath --> vanilla

static int effect_warriors_lesson(int player, int card, event_t event)
{
  if (IS_DDBM_EVENT(event) || event == EVENT_GRAVEYARD_FROM_PLAY || event == EVENT_CHANGE_TYPE)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int packets;
    if (instance->damage_target_card >= 0 &&
        !is_humiliated(instance->damage_target_player, instance->damage_target_card, AL_TRIG) &&
        (packets = equipped_creature_deals_damage(
             player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_MUST_DAMAGE_PLAYER)))
      draw_cards(player, packets);
  }

  if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY)
    kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_warriors_lesson(int player, int card, event_t event){

	/* Warriors' Lesson	|G
	 * Instant
	 * Until end of turn, up to two target creatures you control each gain "Whenever this creature deals combat damage to a player, draw a card." */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_SPELL){
		int i;
		for (i = 0; i < instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				create_targetted_legacy_effect(player, card, &effect_warriors_lesson, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_OPTIONAL_TARGET, &td, "TARGET_CREATURE", 2, NULL);
}

// Gold

int card_akroan_hoplite(int player, int card, event_t event)
{
  /* Akroan Hoplite	|R|W
   * Creature - Human Soldier 1/2
   * Whenever this creature attacks, it gets +X/+0 until end of turn, where X is the number of attacking creatures you control. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    pump_until_eot(player, card, player, card, count_attackers(player), 0);
  return 0;
}

int card_anax_and_cymede(int player, int card, event_t event){
  /* Anax and Cymede	|1|R|W
   * Legendary Creature - Human Soldier 3/2
   * First strike, vigilance
   * Heroic - Whenever you cast a spell that targets ~, creatures you control get +1/+1 and gain trample until end of turn. */

	check_legend_rule(player, card, event);

	vigilance(player, card, event);

	if( heroic(player, card, event) ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, KEYWORD_TRAMPLE, 0);
	}

	return 0;
}

int card_ashen_rider(int player, int card, event_t event){
  /* Ashen Rider	|4|W|W|B|B
   * Creature - Archon 5/5
   * Flying
   * When this creature enters or dies, exile target permanent. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if (can_target(&td) && pick_target(&td, "TARGET_PERMANENT")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;
		card_instance_t *instance = get_card_instance(player, card);
		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT")){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

static void ashiok_exiledby_remember(int player, int card, int t_player, int iid, int* ret_leg, int* ret_idx)
{
  if (is_what(-1, iid, TYPE_CREATURE))
	exiledby_remember(player, card, t_player, iid, ret_leg, ret_idx);
}

int card_ashiok_nightmare_weaver(int player, int card, event_t event)
{
  /* Ashiok, Nightmare Weaver	|1|U|B
   * Planeswalker - Ashiok (3)
   * +2: Exile the top three cards of target opponent's library.
   * -X: Put a creature card with converted mana cost X exiled with ~ onto the battlefield under your control. That creature is a Nightmare in addition to its
   * other types.
   * -10: Exile all cards from all opponents' hands and graveyards. */

	check_legend_rule(player, card, event);


  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	ai_modifier += 48;

  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int loyalty = count_counters(player, card, COUNTER_LOYALTY);

	  target_definition_t td_opponent;
	  default_target_definition(player, card, &td_opponent, 0);
	  td_opponent.zone = TARGET_ZONE_PLAYERS;
	  td_opponent.allowed_controller = 1 - player;

	  int can_library = can_target(&td_opponent);
	  int priority_library = 50;

	  int priority_nightmare = 1, can_nightmare;
	  if (player == AI || ai_is_speculating == 1)
		{
		  priority_nightmare = exiledby_choose(player, card, CARD_ID_ASHIOK_NIGHTMARE_WEAVER, EXBY_MAX_VALUE|EXBY_TEST_CMC_LE, loyalty, "creature", 1);
		  can_nightmare = priority_nightmare > INT_MIN;
		}
	  else
		can_nightmare = exiledby_choose(player, card, CARD_ID_ASHIOK_NIGHTMARE_WEAVER, EXBY_FIRST_FOUND|EXBY_TEST_CMC_LE, loyalty, "creature", 1);

	  int priority_hand_gy = 200;

	  enum
	  {
		CHOICE_LIBRARY = 1,
		CHOICE_NIGHTMARE = 2,
		CHOICE_HAND_GY = 3,
		CHOICE_SHOW_EXILED = 4
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Exile top 3 cards of library", can_library, priority_library, +2,
						"Nightmare creature", can_nightmare, priority_nightmare, 0,
						"Exile hand and graveyard", 1, priority_hand_gy, -10,
						"(Show exiled creatures)", 1, -100, 999);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice && planeswalker(player, card, event, 3);
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;
		  ai_modifier += 48;

		  if (choice == CHOICE_SHOW_EXILED || charge_mana_for_activated_ability(player, card, MANACOST_X(0)))
			switch (choice)
			  {
				case CHOICE_LIBRARY:
				  instance->targets[0].player = 1 - player;
				  instance->targets[0].card = -1;
				  instance->number_of_targets = 1;
				  break;

				case CHOICE_NIGHTMARE:
				  {
					int rval = exiledby_choose(player, card, CARD_ID_ASHIOK_NIGHTMARE_WEAVER, EXBY_CHOOSE|EXBY_TEST_CMC_LE, loyalty, "creature", 1);
					int* loc = (int*)rval;
					if (!loc)
					  spell_fizzled = 1;
					else
					  {
						instance->targets[0].player = -1;
						int exiled_card = *loc;
						instance->targets[0].card = exiled_card;
						int iid = exiled_card & ~0x80000000;
						add_counters_as_cost(player, card, COUNTER_LOYALTY, -get_cmc_by_id(cards_data[iid].id));
					  }
					break;
				  }

				case CHOICE_HAND_GY:
				  break;

				case CHOICE_SHOW_EXILED:
				  exiledby_choose(player, card, CARD_ID_ASHIOK_NIGHTMARE_WEAVER, EXBY_CHOOSE, 0, "creature", 0);
				  spell_fizzled = 1;
				  return 0;	// And don't fall through to planeswalker(), at least not until it honors spell_fizzled.
			  }
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		{
		  switch (choice)
			{
			  case CHOICE_LIBRARY:
				{
				  int i, leg = 0, idx = 0;
				  for (i = 0; i < 3 && deck_ptr[1-player][0] != -1; ++i)
					{
					  ashiok_exiledby_remember(player, instance->parent_card, 1-player, deck_ptr[1-player][0], &leg, &idx);
					  rfg_card_in_deck(1-player, 0);
					}
				  break;
				}

			  case CHOICE_NIGHTMARE:
				{
				  int* loc = exiledby_find(player, instance->parent_card, instance->targets[0].card, NULL, NULL);
				  if (!loc)
					spell_fizzled = 1;
				  else
					{
					  *loc = -1;
					  int owner = (instance->targets[0].card & 0x80000000) ? 1 : 0;
					  int iid = instance->targets[0].card & ~0x80000000;
					  if (remove_card_from_rfg(owner, cards_data[iid].id))
						{
						  int nightmare = add_card_to_hand(player, iid);
						  if (player != owner)
							get_card_instance(player, nightmare)->state ^= STATE_OWNED_BY_OPPONENT;
						  create_targetted_legacy_effect(player, instance->parent_card, empty, player, nightmare);
						  add_a_subtype_permanently(player, card, player, nightmare, SUBTYPE_NIGHTMARE);
						  put_into_play(player, nightmare);
						}
					  else
						spell_fizzled = 1;
					}
				  break;
				}

			  case CHOICE_HAND_GY:
				{
				  int i, leg = 0, idx = 0;
				  for (i = 0; i < active_cards_count[1-player]; ++i)
					if (in_hand(1-player, i))
					  {
						ashiok_exiledby_remember(player, instance->parent_card, 1-player, get_card_instance(1-player, i)->internal_card_id, &leg, &idx);
						kill_card(1-player, i, KILL_EXILE);
					  }

				  for (i = 0; i < 500; ++i)
					ashiok_exiledby_remember(player, instance->parent_card, 1-player, get_grave(1-player)[i], &leg, &idx);

				  rfg_whole_graveyard(1-player);
				  break;
				}

			  case CHOICE_SHOW_EXILED:
				break;
			}
		}
	}

	return planeswalker(player, card, event, 3);
}

int card_battlewise_hoplite(int player, int card, event_t event){
  /* Battlewise Hoplite	|W|U
   * Creature - Human Soldier 2/2
   * Heroic - Whenever you cast a spell that targets this creature, put a +1/+1 counter on this creature, then scry 1. (To scry 1, look at the top card of your library, then you may put that card on the bottom.) */

	if( heroic(player, card, event) ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
		scry(player, 1);
	}

	return 0;
}

int card_chronicler_of_heroes(int player, int card, event_t event){

	/* Chronicler of Heroes	|1|G|W
	 * Creature - Centaur Wizard 3/3
	 * When ~ enters the battlefield, draw a card if you control a creature with a +1/+1 counter on it. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( has_any_counters(player, COUNTER_P1_P1, TYPE_CREATURE) ){
			draw_a_card(player);
		}
	}

	return 0;
}

int card_daxos_of_meletis(int player, int card, event_t event)
{
/* Daxos of Meletis	|1|W|U
 * Legendary Creature - Human Soldier 2/2
 * ~ can't be blocked by creatures with power 3 or greater.
 * Whenever ~ deals combat damage to a player, exile the top card of that player's library. You gain life equal to that card's converted
 * 		mana cost. Until end of turn, you may cast that card and you may spend mana as though it were mana of any color to cast it. */

	check_legend_rule(player, card, event);

	if (event == EVENT_BLOCK_LEGALITY && ! is_humiliated(player, card, AL_STATIC)
		&& attacking_card_controller == player && attacking_card == card
		&& get_power(affected_card_controller, affected_card) >= 3)
		event_result = 1;

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int times_damaged[2] = {BYTE0(result), BYTE1(result)};
		APNAP(p,{
					while( times_damaged[p] ){
							int iid = deck_ptr[p][0];
							if (iid != -1)
							{
								int csvid = cards_data[iid].id;
								gain_life(player, get_cmc_by_id(csvid));
								rfg_top_card_of_deck(p);
								if (!is_what(-1, iid, TYPE_LAND))
									create_may_play_card_from_exile_effect(player, card, p, csvid, MPCFE_UNTIL_EOT | MPCFE_FOR_CMC);
							}
							times_damaged[p]--;
					}
				}
		);
	}

	return 0;
}

int card_destructive_revelry(int player, int card, event_t event){
  /* Destructive Revelry	|R|G
   * Instant
   * Destroy target artifact or enchantment. ~ deals 2 damage to that permanent's controller. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			damage_player(instance->targets[0].player, 2, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "DISENCHANT", 1, NULL);
}

int card_fleecemane_lion(int player, int card, event_t event){
  /* Fleecemane Lion	|G|W
   * Creature - Cat 3/3
   * |3|G|W: Monstrosity 1. (If this creature isn't monstrous, put a +1/+1 counter on it and it becomes monstrous.)
   * As long as this creature is monstrous, it has hexproof and indestructible. */

	if( is_monstrous(player, card) ){
		hexproof(player, card, event);
		indestructible(player, card, event);
	}
	return monstrosity(player, card, event, MANACOST_XGW(3, 1, 1), 1);
}

int card_horizon_chimera(int player, int card, event_t event){

	/* Horizon Chimera	|2|G|U
	 * Creature - Chimera 3/2
	 * Flash
	 * Flying, trample
	 * Whenever you draw a card, you gain 1 life. */

	if (card_drawn_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY)){
		gain_life(player, 1);
	}
	return flash(player, card, event);
}

int card_kragma_warcaller(int player, int card, event_t event)
{
  /* Kragma Warcaller	|3|B|R
   * Creature - Minotaur Warrior 2/3
   * Minotaur creatures you control have haste.
   * Whenever a Minotaur you control attacks, it gets +2/+0 until end of turn. */

	boost_subtype(player, card, event, SUBTYPE_MINOTAUR, 0, 0, 0, SP_KEYWORD_HASTE, BCT_CONTROLLER_ONLY | BCT_INCLUDE_SELF);
	if (event == EVENT_DECLARE_ATTACKERS){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.subtype = SUBTYPE_MINOTAUR;
		store_attackers(player, card, event, DAT_TRACK, player, -1, &test);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)&instance->targets[2].player;
		for (int i = instance->info_slot - 1; i >= 0; --i){
			if (in_play(current_turn, attackers[i])){ pump_until_eot(player, card, current_turn, attackers[i], 2, 0); }
		}
	}
	return 0;
}

int card_medomai_the_ageless(int player, int card, event_t event){
  /* Medomai the Ageless	|4|W|U
   * Legendary Creature - Sphinx 4/4
   * Flying
   * Whenever Medomai deals combat damage to a player, take an extra turn after this one.
   * Medomai can't attack during extra turns. */

	check_legend_rule(player, card, event);
	if (event == EVENT_ABILITIES && in_play(player, card) && !is_humiliated(player, card, AL_STATIC) && get_trap_condition(player, TRAP_TURN_IS_EXTRA)){
		get_card_instance(player, card)->token_status |= STATUS_CANT_ATTACK;
	}
	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if (result){
		int amount = BYTE0(result) + BYTE1(result);
		while (amount){
			time_walk_effect(player, card);
			--amount;
		}
	}
	return 0;
}

int card_pharikas_mender(int player, int card, event_t event){
  /* Pharika's Mender	|3|B|G
   * Creature - Gorgon 4/3
   * When this creature enters, you may return target creature or enchantment card from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_ENCHANTMENT, "Select a creature or enchantment card.");
		if( new_special_count_grave(player, &this_test) && !graveyard_has_shroud(player) ){
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		}
	}

	return 0;
}

static void polish_crusher_damage_ability(int player, int card, int t_player){
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
	td.allowed_controller = td.preferred_controller = t_player;
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( can_target(&td) ){
		if( t_player == player ){
			new_pick_target(&td, "Select target enchantment you control.", 0, GS_LITERAL_PROMPT);
		}
		else{
			new_pick_target(&td, "Select target enchantment your opponent controls.", 0, GS_LITERAL_PROMPT);
		}
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
	}
	instance->number_of_targets = 0;
}

int card_polis_crusher(int player, int card, event_t event){
  /* Polis Crusher	|2|R|G
   * Creature - Cyclops 4/4
   * Trample, protection from enchantments
   * |4|R|G: Monstrosity 3. (If this creature isn't monstrous, put three +1/+1 counters on it and it becomes monstrous.)
   * Whenever this creature deals combat damage to a player, if this creature is monstrous, destroy target enchantment that player controls. */

	check_legend_rule(player, card, event);
	protection_from_type(player, card, event, TYPE_ENCHANTMENT);

	if( is_monstrous(player, card) ){
		int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
		if( result ){
			int times_damaged[2] = {BYTE0(result), BYTE1(result)};
			APNAP(p,{
						while( times_damaged[p] ){
								polish_crusher_damage_ability(player, card, p);
								times_damaged[p]--;
						}
					}
			);
		}
	}

	return monstrosity(player, card, event, MANACOST_XRG(4, 1, 1), 3);
}

int card_prophet_of_kruphix(int player, int card, event_t event){
  /* Prophet of Kruphix	|3|G|U
   * Creature - Human Wizard 2/3
   * Untap all creatures and lands you control during each other player's untap step.
   * You may cast creature spells as though they had flash. */

	if( ! in_play(player, card) || is_humiliated(player, card, AL_STATIC) ){
		return 0;
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && is_what(player, i, TYPE_CREATURE) && has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, i)) ){
				return 1;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int pc[2][hand_count[player]];
		int pcc = 0;
		int i;
		for(i=0; i<active_cards_count[player]; i++){
			if( in_hand(player, i) && is_what(player, i, TYPE_CREATURE) && has_mana_to_cast_iid(player, event, get_original_internal_card_id(player, i)) ){
				pc[0][pcc] = get_original_internal_card_id(player, i);
				pc[1][pcc] = i;
				pcc++;
			}
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to play.");
		int selected = select_card_from_zone(player, player, pc[0], pcc, 0, AI_MAX_VALUE, -1, &this_test);
		if( selected == -1 ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_from_id(player, pc[1][selected], event, get_id(player, pc[1][selected])) ){
			play_card_in_hand_for_free(player, pc[1][selected]);
			cant_be_responded_to = 1;	// The spell will be respondable to, but this (fake) activation won't
		}
		else{
			spell_fizzled = 1;
		}
	}

	{
      int last_untap = (int)get_card_instance(player, card)->info_slot;
      untap_permanents_during_opponents_untap(player, card, TYPE_CREATURE|TYPE_LAND, &last_untap);
      get_card_instance(player, card)->info_slot = last_untap;
    }
	return 0;
}

int card_psychic_intrusion(int player, int card, event_t event){
/* Psychic Intrusion	|3|U|B
 * Sorcery
 * Target opponent reveals his or her hand. You choose a nonland card from that player's graveyard or hand and exile it.
 * You may cast that card for as long as it remains exiled, and you may spend mana as though it were mana of any color to cast that spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int result = -1;
			test_definition_t test;
			if (hand_count[instance->targets[0].player] > 0){
				new_default_test_definition(&test, TYPE_LAND, "Opponent's hand: Select a card to exile.");
				test.type_flag = DOESNT_MATCH;
				result = new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_HAND, TUTOR_RFG, 0, AI_MAX_VALUE, &test);
			}
			if (result < 0 && get_grave(instance->targets[0].player)[0] != -1){
				new_default_test_definition(&test, TYPE_LAND, "Opponent's graveyard: Select a card to exile.");
				test.type_flag = DOESNT_MATCH;
				result = new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_RFG, 0, AI_MAX_VALUE, &test);
			}
			if( result > -1 ){
				create_may_play_card_from_exile_effect(player, card, instance->targets[0].player, result, MPCFE_FOR_CMC);
			}
			else {
				spell_fizzled = 1;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

static int reaper_of_the_wilds_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_REAPER_OF_THE_WILDS)
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
            return 0;
          }
        }
        count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount)
    {
      scry(player, 1);
      amount--;
    }
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_reaper_of_the_wilds(int player, int card, event_t event){
  /* Reaper of the Wilds |2|B|G
   * Creature — Gorgon 4/5
   * Whenever another creature dies, scry 1. (Look at the top card of your library. You may put that card on the bottom of your library.)
   * {B}: Reaper of the Wilds gains deathtouch until end of turn.
   * {1}{G}: Reaper of the Wilds gains hexproof until end of turn. */

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)reaper_of_the_wilds_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &reaper_of_the_wilds_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && ! affect_me(player, card) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		count_for_gfp_ability(player, card, event, ANYBODY, 0, &this_test);
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			int amount = BYTE0(result);
			while( amount ){
					scry(player, 1);
					amount--;
			}
		}
	}

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);

		int ai_priority_deathtouch = (check_for_special_ability(player, card, SP_KEYWORD_DEATHTOUCH) ? -1
									  : current_phase == PHASE_AFTER_BLOCKING && ((is_attacking(player, card) && !is_unblocked(player, card))
																				  || (current_turn == 1-player && instance->blocking != 255)) ? 10
									  : -1);
		int ai_priority_hexproof = check_for_special_ability(player, card, SP_KEYWORD_HEXPROOF) ? -1 : 5;

		enum {
			CHOICE_DEATHTOUCH = 1,
			CHOICE_HEXPROOF = 2
		} choice = DIALOG(player, card, event,
						  "Gain deathtouch",	1, ai_priority_deathtouch,	DLG_MANA(MANACOST_B(1)),
						  "Gain hexproof",		1, ai_priority_hexproof,	DLG_MANA(MANACOST_XG(1,1)));

		if (event == EVENT_CAN_ACTIVATE){
			return can_use_activated_abilities(player, card) && choice;
		} else if (event == EVENT_RESOLVE_ACTIVATION){
			switch (choice){
				case CHOICE_DEATHTOUCH:
					pump_ability_until_eot(player, instance->parent_card, player, instance->parent_card, 0, 0, 0, SP_KEYWORD_DEATHTOUCH);
					break;
				case CHOICE_HEXPROOF:
					pump_ability_until_eot(player, instance->parent_card, player, instance->parent_card, 0, 0, 0, SP_KEYWORD_HEXPROOF);
					break;
			}
		}
	}

	return 0;
}

int card_sentry_of_the_underworld(int player, int card, event_t event){
  /* Sentry of the Underworld	|3|W|B
   * Creature - Griffin Skeleton 3/3
   * Flying, vigilance
   * |W|B, Pay 3 life: Regenerate this creature. */

	vigilance(player, card, event);

	if( event == EVENT_RESOLVE_ACTIVATION )
		return regeneration(player, card, event, MANACOST_WB(1, 1));

	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST_WB(1, 1), 3, NULL, NULL);
}

int card_shipwreck_singer(int player, int card, event_t event){
  /* Shipwreck Singer	|U|B
   * Creature - Siren 1/2
   * Flying
   * |1|U: Target creature an opponent controls attacks this turn if able.
   * |1|B, |T: Attacking creatures get -1/-1 until end of turn. */

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;

		int can_siren = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_XU(1,1), 0, &td, "TARGET_CREATURE");
		int ai_priority_siren = current_turn == 1-player && current_phase <= PHASE_MAIN1 ? 1 : -1;

		int can_weaken = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED|GAA_NONSICK, MANACOST_XB(1,1), 0, NULL, NULL);
		int ai_priority_weaken = current_turn == 1-player && current_phase > PHASE_MAIN1 && current_phase < PHASE_MAIN2 ? 1 : -1;

		enum {
			CHOICE_SIREN = 1,
			CHOICE_WEAKEN = 2
		} choice = DIALOG(player, card, event,
						  "Creature attacks if able", can_siren, ai_priority_siren,
						  "Attacking creatures get -1/-1", can_weaken, ai_priority_weaken);

		if (event == EVENT_CAN_ACTIVATE){
			return choice;
		} else if (event == EVENT_ACTIVATE){
			instance->number_of_targets = 0;
			switch (choice){
				case CHOICE_SIREN:
					if (charge_mana_for_activated_ability(player, card, MANACOST_XU(1,1))){
						pick_target(&td, "TARGET_CREATURE");
					}
					break;
				case CHOICE_WEAKEN:
					if (charge_mana_for_activated_ability(player, card, MANACOST_XB(1,1))){
						tap_card(player, card);
					}
					break;
			}
		} else {	// EVENT_RESOLVE_ACTIVATION
			switch (choice){
				case CHOICE_SIREN:
					pump_ability_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
										   0, 0, 0, SP_KEYWORD_MUST_ATTACK);
					break;
				case CHOICE_WEAKEN:{
					test_definition_t this_test;
					default_test_definition(&this_test, TYPE_CREATURE);
					this_test.state = STATE_ATTACKING;
					pump_creatures_until_eot(player, instance->parent_card, current_turn, 0, -1, -1, 0, 0, &this_test);
					break;
				}
			}
		}
	}
	return 0;
}

int card_spellheart_chimera(int player, int card, event_t event)
{
  /* Enigma Drake	|1|U|R
   * Creature - Drake 100/4
   * Flying
   * ~'s power is equal to the number of instant and sorcery cards in your graveyard. */

  int newval;
  if ((((event == EVENT_POWER) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    newval = count_graveyard_by_type(player, 0x18);
    set_characteristic_pt(newval);
  }
  return 0;
}

int card_steam_augury(int player, int card, event_t event)
{
  /* Steam Augury	|2|U|R
   * Instant
   * Reveal the top five cards of your library and separate them into two piles. An opponent chooses one of those piles. Put that pile into your hand and the other into your graveyard. */

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
		ai_modifier += 48;

	if (event == EVENT_RESOLVE_SPELL)
	{
		effect_fof(player, 1-player, 5, TUTOR_GRAVE);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

const char* target_has_fate_counter(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  return count_counters(player, card, COUNTER_FATE) <= 0 ? "no fate counter" : NULL;
}

const char* target_not_has_fate_counter(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  return count_counters(player, card, COUNTER_FATE) > 0 ? "fate counter" : NULL;
}

int card_triad_of_fates(int player, int card, event_t event)
{
  /* Triad of Fates	|2|W|B
   * Legendary Creature - Human Wizard 3/3
   * |1, |T: Put a fate counter on another target creature.
   * |W, |T: Exile target creature that has a fate counter on it, then return it to the battlefield under its owner's control.
   * |B, |T: Exile target creature that has a fate counter on it. Its controller draws two cards. */

  check_legend_rule(player, card, event);

  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
	  if (event == EVENT_CAN_ACTIVATE
		  && (is_tapped(player, card) || is_sick(player, card) || !can_use_activated_abilities(player, card)))
		return 0;	// otherwise, check more carefully below

	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td_notme;
	  default_target_definition(player, card, &td_notme, TYPE_CREATURE);
	  td_notme.preferred_controller = player;
	  td_notme.special = TARGET_SPECIAL_NOT_ME;

	  target_definition_t td_fated;
	  default_target_definition(player, card, &td_fated, TYPE_CREATURE);
	  td_fated.preferred_controller = player;
	  td_fated.extra = target_predicate_extra(target_has_fate_counter);
	  td_fated.special = TARGET_SPECIAL_EXTRA_FUNCTION;

	  target_definition_t td_destroyed;
	  target_definition_t td_unfated;

	  int can_fate = has_mana_for_activated_ability(player, card, MANACOST_X(1)) && can_target(&td_notme);
	  int can_blink, can_exile;
	  if (can_target(&td_fated))
		{
		  can_blink = has_mana_for_activated_ability(player, card, MANACOST_W(1));
		  can_exile = has_mana_for_activated_ability(player, card, MANACOST_B(1));
		}
	  else
		can_blink = can_exile = 0;

	  int ai_priority_fate, ai_priority_blink, ai_priority_exile;

	  if (player == AI || ai_is_speculating == 1)
		{
		  /* This seems a poor idea - among other things, it'll let the ai blink a creature after it's done combat damage but before the combat damage dealt to
		   * it destroys it, like when combat damage used the stack.  On the other hand, it doesn't seem to work anyway. */
		  default_target_definition(player, card, &td_destroyed, TYPE_CREATURE);
		  td_destroyed.allowed_controller = player;
		  td_destroyed.preferred_controller = player;
		  td_destroyed.extra = target_predicate_extra(target_has_fate_counter);
		  td_destroyed.special = TARGET_SPECIAL_EXTRA_FUNCTION;
		  td_destroyed.required_state = TARGET_STATE_DESTROYED;
		  td_destroyed.allow_cancel = 0;

		  default_target_definition(player, card, &td_unfated, TYPE_CREATURE);
		  td_unfated.allowed_controller = player;
		  td_unfated.preferred_controller = player;
		  td_unfated.extra = target_predicate_extra(target_not_has_fate_counter);
		  td_unfated.special = TARGET_SPECIAL_EXTRA_FUNCTION | TARGET_SPECIAL_NOT_ME;
		  td_unfated.allow_cancel = 0;

		  ai_priority_fate = can_target(&td_unfated) ? 5 : -1;
		  ai_priority_blink = can_target(&td_destroyed) ? 10 : 3;	// specific value checked for below
		  ai_priority_exile = hand_count[player] < 1 ? 7 : 1;
		}
	  else
		ai_priority_fate = ai_priority_blink = ai_priority_exile = 0;

	  enum
	  {
		CHOICE_FATE = 1,
		CHOICE_BLINK = 2,
		CHOICE_EXILE = 3
	  } choice = DIALOG(player, card, event,
						"Add a fate counter", can_fate, ai_priority_fate,
						"Blink a creature", can_blink, ai_priority_blink,
						"Exile and draw", can_exile, ai_priority_exile);

	  if (event == EVENT_CAN_ACTIVATE)
		return (!choice ? 0
				: ai_priority_blink == 10 && (land_can_be_played & LCBP_REGENERATION) ? 99
				: 1);
	  else if (event == EVENT_ACTIVATE)
		{
		  instance->number_of_targets = 0;

		  if (player == AI && current_turn == 1-player && current_phase == PHASE_DISCARD && can_target(&td_unfated))
			ai_modifier += 48;

		  switch (choice)
			{
			  case CHOICE_FATE:
				if (charge_mana_for_activated_ability(player, card, MANACOST_X(1)))
				  {
					if ((player == AI || ai_is_speculating == 1) && can_target(&td_unfated) && pick_target(&td_unfated, "TARGET_CREATURE"))
					  tap_card(player, card);
					else if (pick_target(&td_notme, "TARGET_CREATURE"))
					  {
						ai_modifier -= 48;
						tap_card(player, card);
					  }
				  }
				break;
			  case CHOICE_BLINK:
				if (charge_mana_for_activated_ability(player, card, MANACOST_W(1)))
				  {
					if ((player == AI || ai_is_speculating == 1) && can_target(&td_destroyed) && pick_target(&td_destroyed, "TARGET_CREATURE"))
					  {
						ai_modifier += 64;
						tap_card(player, card);
					  }
					else if (pick_target(&td_fated, "TARGET_CREATURE"))
					  tap_card(player, card);
				  }
				break;
			  case CHOICE_EXILE:
				if (charge_mana_for_activated_ability(player, card, MANACOST_B(1))
					&& pick_target(&td_fated, "TARGET_CREATURE"))
				  tap_card(player, card);
				break;
			}
		}
	  else	// event == EVENT_RESOLVE_ACTIVATION
		{
		  switch (choice)
			{
			  case CHOICE_FATE:
				if (valid_target(&td_notme))
				  add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_FATE, 1);
				break;
			  case CHOICE_BLINK:
				if (valid_target(&td_fated))
				  blink_effect(instance->targets[0].player, instance->targets[0].card, 0);
				break;
			  case CHOICE_EXILE:
				if (valid_target(&td_fated))
				  {
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
					draw_cards(instance->targets[0].player, 2);
				  }
				break;
			}
		}
	}

  return 0;
}

int card_tymaret_the_murder_king(int player, int card, event_t event){
	/* Tymaret, the Murder King	|B|R	0x200b0e7
	 * Legendary Creature - Zombie Warrior 2/2
	 * |1|R, Sacrifice another creature: ~ deals 2 damage to target player or planeswalker.
	 * |1|B, Sacrifice a creature: Return ~ from your graveyard to your hand. */
	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	check_legend_rule(player, card, event);

	if(event == EVENT_GRAVEYARD_ABILITY){
		if( has_mana_multi( player, MANACOST_WB(1, 1)) && can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE) ){
			return GA_RETURN_TO_HAND;
		}
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_multi( player, MANACOST_WB(1, 1) );
		if( spell_fizzled != 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			if( ! new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
				cancel = 1;
				return 0;
			}
			return GAPAID_REMOVE;
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE | GAA_NOT_ME_AS_TARGET, MANACOST_XR(1, 1),
									0, &td, "TARGET_PLAYER_OR_PLANESWALKER");
}

int card_underworld_cerberus(int player, int card, event_t event)
{
  /* Underworld Cerberus	|3|B|R
   * Creature - Hound 6/6
   * ~ can't be blocked except by three or more creatures.
   * Cards in graveyards can't be the targets of spells or abilities.
   * When ~ dies, exile it and each player returns all creature cards from his or her graveyard to his or her hand. */

  int num_returned[2];
  test_definition_t test;
  minblockers_3(player, card, event);
  if (!this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
    return 0;

  exile_from_owners_graveyard(player, card);
  new_default_test_definition(&test, TYPE_CREATURE, "");
  int first_player = current_turn;
  int returned = from_grave_to_hand_multiple(first_player, &test);
  num_returned[first_player] = returned;
  int second_player = 1 - current_turn;
  returned = from_grave_to_hand_multiple(second_player, &test);
  num_returned[second_player] = returned;
  ai_modifier = ai_modifier + (num_returned[1] - num_returned[0]) * 8;
  return 0;
}

int card_xenagos_the_reveler(int player, int card, event_t event){

	/* Xenagos, the Reveler	|2|R|G
	 * Planeswalker - Xenagos (3)
	 * +1: Add X mana in any combination of |R and/or |G to your mana pool, where X is the number of creatures you control.
	 * 0: Put a 2/2 |Sred and |Sgreen Satyr creature token with haste onto the battlefield.
	 * -6: Exile the top seven cards of your library. You may put any number of creature and/or land cards from among them onto the battlefield. */
	check_legend_rule(player, card, event);

	if (IS_ACTIVATING(event)){

		int priorities[3] = {0, 0, 0} ;
		if( event == EVENT_ACTIVATE ){
			priorities[0] = count_subtype(player, TYPE_CREATURE, -1)*3;
			priorities[1] = 15-(count_subtype(player, TYPE_CREATURE, -1)*5);
			priorities[2] = (internal_rand(5)*5)+((count_counters(player, card, COUNTER_LOYALTY)*5)-30);
		}

		enum{
			CHOICE_MANA = 1,
			CHOICE_SATYR,
			CHOICE_EXILE7
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Produce mana", 1, priorities[0], 1,
						"Generate a Satyr", 1, priorities[1], 0,
						"Exile top 7", 1, priorities[2], -6);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;
		}
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		{
			switch (choice)
			{
				case CHOICE_MANA:
				{
					// Stupidly enough, the ruling is that this isn't a mana ability and it's supposed to happen at resolution.
					int amount = count_subtype(player, TYPE_CREATURE, -1);
					if (amount > 0){
						do {
							spell_fizzled = 0;
							produce_mana_any_combination_of_colors(player, COLOR_TEST_RED|COLOR_TEST_GREEN, amount, NULL);
							tapped_for_mana_color = -2;
						} while (spell_fizzled == 1);
					}
				}
				break;

				case CHOICE_SATYR:
				{
					token_generation_t token;
					default_token_definition(player, card, CARD_ID_SATYR, &token);
					token.pow = 2;
					token.tou = 2;
					token.color_forced = COLOR_TEST_RED|COLOR_TEST_GREEN;
					token.s_key_plus = SP_KEYWORD_HASTE;
					generate_token(&token);
				}
				break;

				case CHOICE_EXILE7:
				{
					int amount = MIN(7, count_deck(player));
					if( amount > 0 ){
						int *deck = deck_ptr[player];
						show_deck( 1-player, deck, amount, "Card exiled by Xenagos", 0, gs_done_008b40e0 );

						test_definition_t this_test;
						new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_LAND, "Select a creature or land card.");
						this_test.create_minideck = amount;
						this_test.no_shuffle = 1;
						while( amount > 0 && new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test) >= 0 ){
								amount--;
								this_test.create_minideck--;
						}
						rfg_top_n_cards_of_deck(player, amount);
					}
				}
				break;
			}
		}
	}

	return planeswalker(player, card, event, 3);
}

// Artifacts

int card_akroan_horse(int player, int card, event_t event){
/* CARD_ID_AKROAN_HORSE	13371
Akroan Horse	|4
Artifact Creature - Horse 0/4
Defender
When ~ enters the battlefield, an opponent gains control of it.
At the beginning of your upkeep, each opponent creates a 1/1 |Swhite Soldier creature token. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		give_control(player, card, player, card);
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_SOLDIER, &token);
		token.t_player = 1-player;
		generate_token(&token);
	}

	return 0;
}

// Anvilwrought Raptor --> vanilla

// bronze sable --> vanilla

int card_burnished_hart(int player, int card, event_t event){

	/* Burnished Hart	|3
	 * Artifact Creature - Elk 2/2
	 * |3, Sacrifice ~: Search your library for up to two basic land cards, put them onto the battlefield tapped, then shuffle your library. */

	if(event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 2);
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(3), 0, NULL, NULL);
}

int card_colossus_of_akros(int player, int card, event_t event){

	/* Colossus of Akros	|8
	 * Artifact Creature - Golem 10/10
	 * Defender, indestructible
	 * |10: Monstrosity 10.
	 * As long as ~ is monstrous, it has trample and can attack as though it didn't have defender. */

	indestructible(player, card, event);

	if (event == EVENT_ABILITIES && affect_me(player, card) && is_monstrous(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result |= KEYWORD_TRAMPLE;
		card_instance_t* instance = get_card_instance(player, card);
		instance->token_status |= STATUS_WALL_CAN_ATTACK;
	}

	return monstrosity(player, card, event, MANACOST_X(10), 10);
}

int card_flamecast_wheel(int player, int card, event_t event){
/* CARD_ID_FLAMECAST_WHEEL	13443
Flamecast Wheel	|1
Artifact
|5, |T, Sacrifice ~: ~ deals 3 damage to target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME|GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(5), 0,
									&td, "TARGET_CREATURE");
}

int card_fleetfeather_sandals(int player, int card, event_t event){
/* CARD_ID_FLEETFEATHER_SANDALS	13446
Fleetfeather Sandals	|2
Artifact - Equipment
Equipped creature has flying and haste.
Equip |2 (|2: Attach to target creature you control. Equip only as a sorcery.) */

	return vanilla_equipment(player, card, event, 2, 0, 0, KEYWORD_FLYING, SP_KEYWORD_HASTE);
}

/* CARD_ID_OPALINE_UNICORN	13504
Opaline Unicorn	|3
Artifact Creature - Unicorn 1/2
|T: Add one mana of any color to your mana pool. */

// opaline unicorn --> card_generic_noncombat_1_mana_producing_creature

int card_prowlers_helm(int player, int card, event_t event){
/* CARD_ID_PROWLERS_HELM	13522
Prowler's Helm	|2
Artifact - Equipment
Equipped creature can't be blocked except by Walls.
Equip |2 */

	if( is_equipping(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_BLOCK_LEGALITY ){
			card_instance_t *instance= get_card_instance(player, card);
			if( attacking_card_controller == instance->damage_target_player && attacking_card == instance->damage_target_card ){
				if( ! has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL) ){
					event_result = 1;
				}
			}
		}
	}

	return basic_equipment(player, card, event, 2);
}

int card_pyxis_of_pandemonium(int player, int card, event_t event)
{
/* CARD_ID_PYXIS_OF_PANDEMONIUM	13528
Pyxis of Pandemonium	|1
Artifact
|T: Each player exiles the top card of his or her library face down.
|7, |T, Sacrifice ~: Each player turns face up all cards he or she owns exiled with ~, then puts all permanent cards among them onto the battlefield. */

  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  int can_exile = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED|GAA_NONSICK, MANACOST_X(0), 0, 0, 0);
	  int can_return = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED|GAA_NONSICK|GAA_SACRIFICE_ME, MANACOST_X(7), 0, 0, 0);
	  int ai_priority_return = exiledby_count(player, card, player);

	  enum
	  {
		CHOICE_EXILE = 1,
		CHOICE_RETURN = 2
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Exile top cards", can_exile, 5,
						"Return permanents from exile", can_return, ai_priority_return);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_EXILE:
			  if (player == AI && current_turn == 1-player && current_phase == PHASE_DISCARD)
				ai_modifier += 48;
			  if (charge_mana_for_activated_ability(player, card, MANACOST_X(0)))
				tap_card(player, card);
			  break;

			case CHOICE_RETURN:
			  if (charge_mana_for_activated_ability(player, card, MANACOST_X(7)))
				{
				  tap_card(player, card);
				  instance->targets[1].player = exiledby_detach(player, card);
				  kill_card(player, card, KILL_SACRIFICE);
				}
			  break;
#if 0
			case CHOICE_DEBUG:
			  exiledby_choose(player, card, CARD_ID_PYXIS_OF_PANDEMONIUM, EXBY_CHOOSE, 0, NULL, 0);
			  cancel=1;
			  break;
#endif
		  }
	  else	// event == EVENT_RESOLVE_ACTIVATION
		{
		  int leg = 0, idx = 0, *loc;

		  switch (choice)
			{
			  case CHOICE_EXILE:
				if (deck_ptr[player][0] != -1)
				  {
					exiledby_remember(player, instance->parent_card, player, deck_ptr[player][0], &leg, &idx);
					play_sound_effect(WAV_DESTROY);
					obliterate_top_card_of_deck(player);
				  }
				if (deck_ptr[1-player][0] != -1)
				  {
					exiledby_remember(player, instance->parent_card, 1-player, deck_ptr[1-player][0], &leg, &idx);
					play_sound_effect(WAV_DESTROY);
					obliterate_top_card_of_deck(1-player);
				  }
				break;

			  case CHOICE_RETURN:
				while ((loc = exiledby_find_any(player-2, instance->targets[1].player, &leg, &idx)))
				  {
					int owner = (*loc & 0x80000000) ? 1 : 0;
					int iid = *loc & ~0x80000000;
					if (is_what(-1, iid, TYPE_PERMANENT))
					  put_into_play(owner, add_card_to_hand(owner, iid));
					else
					  add_card_to_rfg(owner, iid);
					*loc = -1;
				  }

				exiledby_destroy_detached(player, instance->targets[1].player);
				break;
			}
		}
	}

  return 0;
}

int card_witches_eye(int player, int card, event_t event){
/* CARD_ID_WITCHES_EYE	13601
Witches' Eye	|1
Artifact - Equipment
Equipped creature has "|1, |T: Scry 1." (To scry 1, look at the top card of your library, then you may put that card on the bottom of your library.)
Equip |1 */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_EQUIP = 1,
		CHOICE_SCRY
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL) ){
				return 1;
			}
		}
		return basic_equipment(player, card, event, 1);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = { 0, can_activate_basic_equipment(player, card, event, 1), 0 };
		if( is_equipping(player, card) ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( granted_generic_activated_ability(player, card, p, c, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL) ){
				abils[2] = 1;
			}
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Equip", abils[1], 1,
							"Scry", abils[2], 5);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 1);
		}
		if( choice == CHOICE_SCRY ){
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			return granted_generic_activated_ability(player, card, p, c, event, GAA_UNTAPPED, MANACOST_X(1), 0, NULL, NULL);
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_EQUIP ){
			return basic_equipment(player, card, event, 1);
		}
		if( instance->info_slot == CHOICE_SCRY ){
			scry(player, 1);
		}
	}
	return 0;
}

// Lands

int card_nykthos_shrine_to_nix(int player, int card, event_t event){
	/* Nykthos, Shrine to Nyx	""
	 * Legendary Land
	 * |T: Add |C to your mana pool.
	 * |2, |T: Choose a color. Add to your mana pool an amount of mana of that color equal to your devotion to that color. */

	check_legend_rule(player, card, event);

	if( (! IS_GAA_EVENT(event) && ! (event == EVENT_COUNT_MANA && affect_me(player, card))) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		int ai_choice = 0;
		if( has_mana(player, COLOR_ANY, 3) ){
			if( devotion(player, -1, get_deck_color(player, player), 0) >= 3 ){
				ai_choice = 1;
			}
			choice = do_dialog(player, player, card, -1, -1, " Produce 1\n Produce Devotion mana\n Cancel", ai_choice);
		}

		if( choice == 0){
			return mana_producer(player, card, event);
		}
		else if( choice == 1 ){
				instance->state |= STATE_TAPPED;
				charge_mana(player, COLOR_ANY, 2);
				if( spell_fizzled != 1 ){
					int clr = choose_a_color(player, get_deck_color(player, player));
					produce_mana_tapped_impl(player, card, clr, devotion(player, -1, clr, 0), 0, 1);
				}
				else{
					instance->state &= ~STATE_TAPPED;
				}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			instance->info_slot = COLOR_TEST_COLORLESS;

			/* This doesn't really work - has_mana() isn't accurate during EVENT_COUNT_MANA.
			 * Proper solution here for mana sources that require mana to activate is to create a second pass after all the free mana sources have been
			 * counted.  Will want to move 0x472400, count_mana(), into C. */
			if( has_mana(player, COLOR_ANY, 2) ){
				/* No good solution here - declare_mana_available_hex() forces the amount of mana produceable to always be the same color.
				 * Instead we have a choice of e.g. {1}, {-2}{W}, {-2}{U}{U}{U}, {-2}{G}{G}, etc. */
				int clr = get_deck_color(player, player);
				declare_mana_available(player, clr, devotion(player, -1, clr, 0));

				// Set info_slot for get_color_of_mana_produced_by_id() and get_colors_of_mana_land_could_produce_ignoring_costs()
				for (clr = COLOR_BLACK; clr <= COLOR_WHITE; ++clr){
					if (devotion(player, -1, clr, 0) > 0){
						instance->info_slot |= 1 << clr;
					}
				}
			}
			else{
				declare_mana_available(player, COLOR_COLORLESS, 1);
			}
		}
	}

	return 0;
}

// double lands --> new benalia

// "Face the Hydra" challenge deck

// Functions

static void damage_opponent_on_eot(int player, int card, event_t event, int dmg){
	if(trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player){
		if( current_turn == player && ! is_tapped(player, card) ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					if( has_subtype(player, card, SUBTYPE_ELITE) ){
						dmg++;
					}
					damage_player(1-player, dmg, player, card);
			}
		}
	}
}

static int generic_hydra_head(int player, int card, event_t event)
{
  card_instance_t *instance = get_card_instance(player, card);
  if (instance->kill_code > 0 && instance->kill_code > KILL_SACRIFICE)
  {
    instance->kill_code = KILL_BURY;
  }
  leaves_play(player, card, event, 0, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {
    int *deck = deck_ptr[player];
    int i = 0;
    while (deck[0] != -1 && i < 2)
    {
      if (is_what(-1, deck[0], TYPE_PERMANENT))
      {
        put_into_play_a_card_from_deck(player, player, 0);
      }
      else
      {
        mill(player, 1);
      }
      i++;
    }
  }
  return 0;
}

// Cards

// creature (heads)

int card_hydra_head(int player, int card, event_t event){
  /* Hydra Head	""
   * Creature - Head 0/3
   * Hero's Reward - When ~ leaves the battlefield, each player gains 2 life. */

	if( graveyard_from_play(player, card, event) ){
		gain_life(1-player, 2);
	}
	damage_opponent_on_eot(player, card, event, 1);
	return generic_hydra_head(player, card, event);
}

int card_ravenous_brute_head(int player, int card, event_t event){
  /* Ravenous Brute Head	""
   * Elite Creature - Head 0/6
   * Hero's Reward - When ~ leaves the battlefield, each player gains 2 life and draws a card. */

	if( this_dies_trigger(player, card, event, 2) ){
		draw_cards(1-player, 1);
		gain_life(1-player, 2);
	}
	if( graveyard_from_play(player, card, event) ){
		gain_life(1-player, 2);
	}
	damage_opponent_on_eot(player, card, event, 1);
	return generic_hydra_head(player, card, event);
}

int card_savage_vigor_head(int player, int card, event_t event){
  /* Savage Vigor Head	""
   * Elite Creature - Head 0/8
   * At the beginning of the Hydra's end step, reveal the top card of the Hydra's library and the Hydra casts that card.
   * Hero's Reward - When ~ leaves the battlefield, each player gains 4 life and draws a card. */

	if( this_dies_trigger(player, card, event, 2) ){
		draw_cards(1-player, 1);
		gain_life(1-player, 4);
	}
	if( current_turn == player && eot_trigger(player, card, event) ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			int iid = deck[0];
			remove_card_from_deck(player, 0);
			play_card_in_hand_for_free(player, add_card_to_hand(player, iid));	// Inaccurate - won't run draw or replace-draw triggers
		}
		if( ! is_tapped(player, card) ){
			damage_player(1-player, 2, player, card);
		}
	}
	return generic_hydra_head(player, card, event);
}

int card_shrieking_titan_head(int player, int card, event_t event){
  /* Shrieking Titan Head	""
   * Elite Creature - Head 0/8
   * At the beginning of the Hydra's end step, each player discards a card.
   * Hero's Reward - When ~ leaves the battlefield, each player gains 4 life and draws a card. */

	if( this_dies_trigger(player, card, event, 2) ){
		draw_cards(1-player, 1);
		gain_life(1-player, 4);
	}
	if( current_turn == player && eot_trigger(player, card, event) ){
		discard(1-player, 0, player);
		if( ! is_tapped(player, card) ){
			int dmg = 1;
			if( has_subtype(player, card, SUBTYPE_ELITE) ){
				dmg++;
			}
			damage_player(1-player, dmg, player, card);
		}
	}
	return generic_hydra_head(player, card, event);
}

int card_snapping_fang_head(int player, int card, event_t event){
  /* Snapping Fang Head	""
   * Elite Creature - Head 0/8
   * At the beginning of the Hydra's end step, ~ deals 1 damage to each player.
   * Hero's Reward - When ~ leaves the battlefield, each player gains 4 life and draws a card. */

	if( this_dies_trigger(player, card, event, 2) ){
		draw_cards(1-player, 1);
		gain_life(1-player, 4);
	}
	damage_opponent_on_eot(player, card, event, 2);
	return generic_hydra_head(player, card, event);
}

// spells

int card_disorienting_glower(int player, int card, event_t event){
  /* Disorienting Glower	""
   * Sorcery
   * Players can't cast spells until the Hydra's next turn. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_player_cant_cast_type(player, card, 1-player, TYPE_CREATURE|TYPE_ENCHANTMENT|TYPE_ARTIFACT|TARGET_TYPE_PLANESWALKER|TYPE_SPELL|TYPE_EFFECT);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_distract_the_hydra(int player, int card, event_t event){
  /* Distract the Hydra	""
   * Sorcery
   * Each player may sacrifice a creature. Each player who sacrificed a creature this way chooses a Head and taps it. Each player who didn't sacrifice a creature loses 3 life. */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( sacrifice(player, card, 1-player, 0, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.required_subtype = SUBTYPE_HEAD;
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.who_chooses = 1-player;

			if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
		else{
			lose_life(1-player, 3);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_grown_from_the_stump(int player, int card, event_t event)
{
  /* Grown from the Stump	""
   * Sorcery
   * Place exactly two cards named Hydra Head onto the battlefield from the Hydra's graveyard. If you can't, reveal cards from the top of the Hydra's library until you reveal a Head card. Put that card onto the battlefield and the rest into the Hydra's graveyard. */

  int *deck;
  int result;
  char msg[100];
  test_definition_t this_test;
  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  memset(msg, 0, sizeof(msg));
  memcpy(msg, "Select an Hydra Head to reanimate.", 34);
  new_default_test_definition(&this_test, 0xff, msg);
  result = new_special_count_grave(player, &this_test);
  if (result < 2)
  {
    deck = deck_ptr[player];
    result = *deck;
    if (result != -1)
    {
      do
      {
        result = has_subtype_by_id(player, (unsigned int)cards_data[result].id, SUBTYPE_HEAD);
        if (result != 0)
          break;
        result = *deck;
      } while (result != -1);
      show_deck(0, deck, 1, "Cards revealed by Grown From the Stump", 0, gs_done_008b40e0);
      put_into_play_a_card_from_deck(player, player, 0);
    }
  }
  else
  {
    result = new_global_tutor(player, player, 2, 4, 1, 5, &this_test);
    if (-1 < result)
    {
      new_global_tutor(player, player, 2, 4, 1, 5, &this_test);
    }
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

static int indestructible_until_next_turn(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (affect_me(instance->damage_target_player, instance->damage_target_card))
		indestructible(instance->damage_target_player, instance->damage_target_card, event);
	}

  if (event == EVENT_BEGIN_TURN && current_turn == player)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_hydra_impenetrable_hide(int player, int card, event_t event)
{
  /* Hydra's Impenetrable Hide	""
   * Sorcery
   * Each Head gains indestructible until the end of the Hydra's next turn. */

  // Heads are indestructible until the hydra's next turn.

  // (Heads can't be controlled, can they?  Even if so, I'm assuming that only the Hydra can cast this.)

  if (event == EVENT_CAN_CAST)
	return 1;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int p, c;
	  for (p = 0; p <= 1; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c) && has_subtype(p, c, SUBTYPE_HEAD))
			create_targetted_legacy_effect(player, card, indestructible_until_next_turn, p, c);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_neck_tangle(int player, int card, event_t event){
  /* Neck Tangle	""
   * Sorcery
   * If there are five or more Heads on the battlefield, tap two of them and they don't untap during the Hydra's next untap step. Otherwise, reveal the top card of the Hydra's library and the Hydra casts that card. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( count_subtype(player, TYPE_CREATURE, SUBTYPE_HEAD) > 4 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.required_subtype = SUBTYPE_HEAD;
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.allow_cancel = 0;

			card_instance_t *instance = get_card_instance(player, card);

			int count = 0;
			while( count < 2 ){
					if( pick_target(&td, "TARGET_CREATURE") ){
						instance->number_of_targets = 1;
						effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
					}
					count++;
			}
		}
		else{
			int *deck = deck_ptr[player];
			if( deck[0] != -1 ){
				int iid = deck[0];
				remove_card_from_deck(player, 0);
				play_card_in_hand_for_free(player, add_card_to_hand(player, iid));	// Inaccurate - won't run draw or replace-draw triggers
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_noxious_hydra_breath(int player, int card, event_t event){
  /* Noxious Hydra Breath	""
   * Sorcery
   * Choose one - ~ deals 5 damage to each player; or destroy each tapped non-Head creature. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int ai_choice = 0;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_TAPPED;
		this_test.subtype = SUBTYPE_HEAD;
		this_test.subtype_flag = 1;

		if( check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &this_test) > 1 && life[1-player] > 6 ){
			ai_choice = 1;
		}
		do_dialog(player, player, card, -1, -1, " 5 damage to opponents\n Destroy non-Head tapped creatures", ai_choice);
		if( ai_choice == 0 ){
			damage_player(1-player, 5, player, card);
		}
		else{
			new_manipulate_all(player, card, 2, &this_test, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_strike_the_weak_spot(int player, int card, event_t event){
  /* Strike the Weak Spot	""
   * Sorcery
   * Destroy target Head. If that Head was elite, the Hydra takes an extra turn after this one. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_subtype = SUBTYPE_HEAD;
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td)){
		return 1;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( has_subtype(instance->targets[0].player, instance->targets[0].card, SUBTYPE_ELITE) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				return card_time_walk(player, card, event);
			}
			else{
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int swallow_the_hero_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( trigger_condition == TRIGGER_LEAVE_PLAY ){
		if( affect_me( player, card) && has_subtype(trigger_cause_controller, trigger_cause, SUBTYPE_HEAD) && reason_for_trigger_controller == player ){
			if(event == EVENT_TRIGGER){
				event_result |= 2;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					int card_added = add_card_to_hand(instance->targets[0].player, get_internal_card_id_from_csv_id(instance->targets[0].card));
					put_into_play(instance->targets[0].player, card_added);
					kill_card(player, card, KILL_EXILE);
			}
		}
	}
	return 0;
}

int card_swallow_the_hero_whole(int player, int card, event_t event){
  /* Swallow the Hero Whole	""
   * Sorcery
   * Each player exiles a creature they control. Until the Hydra's next turn, when a Head leaves the battlefield, return the exiled cards to the battlefield under their owners' control. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.who_chooses = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( pick_target(&td, "TARGET_CREATURE") ){
			int result = rfg_target_permanent(instance->targets[0].player, instance->targets[0].card);
			int legacy = create_legacy_effect(player, card, &swallow_the_hero_legacy);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[0].player = 1-player;
			if( result > available_slots ){
				result-=available_slots;
				leg->targets[0].player = player;
			}
			leg->targets[0].card = result;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_torn_between_the_heads(int player, int card, event_t event){
  /* Torn Between Heads	""
   * Sorcery
   * Tap up to two Heads. They don't untap during the Hydra's next untap step. ~ deals 5 damage to each player. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.required_subtype = SUBTYPE_HEAD;
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		int count = 0;
		while( count < 2 && can_target(&td) ){
				if( pick_target(&td, "TARGET_CREATURE") ){
					instance->number_of_targets = 1;
					effect_frost_titan(player, card, instance->targets[0].player, instance->targets[0].card);
				}
				count++;
		}
		damage_player(1-player, 5, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_unified_lunge(int player, int card, event_t event){
  /* Unified Lunge	""
   * Sorcery
   * ~ deals X damage to each player, where X is the number of Heads on the battlefield. */

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(1-player, count_subtype(player, TYPE_PERMANENT, SUBTYPE_HEAD), player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

// Hero's paths

// the protector --> samite healer

int card_the_philosopher(int player, int card, event_t event){
  /* The Philosopher	""
   * Hero
   * |2, |T: Tap target creature. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_CAN_TARGET, 2, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}

int card_the_avenger(int player, int card, event_t event){
  /* The Avenger	""
   * Hero
   * |3, |T: Target creature you control gains deathtouch until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_DEATHTOUCH);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_CAN_TARGET, 3, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}

int card_the_warrior(int player, int card, event_t event){
  /* The Warrior	""
   * Hero
   * |T: Target creature you control gains haste until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_CAN_TARGET, 3, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}

int card_the_hunter(int player, int card, event_t event){
  /* The Hunter	""
   * Hero
   * |T: Target creature you control gets +1/+1 until end of turn. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED+GAA_CAN_TARGET, 3, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
}

// the harvester --> merfolk looter

int card_the_slayer(int player, int card, event_t event){
  /* The Slayer	""
   * Hero
   * You start the game with an additional 7 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 7);
	}
	return 0;
}
