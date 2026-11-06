#include "manalink.h"

// Any special_infos that's a characteristic-defining ability needs to be special-cased in get_copyable_special_infos() in token_generation.c.

int card_dragon_token(int player, int card, event_t event){
	if( player < 0 || card < 0 ){
		return 0;
	}
	if( get_special_infos(player, card) == 66 ){
		devour(player, card, event, 2);
	}
	if( get_special_infos(player, card) == 67 && IS_GAA_EVENT(event) ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1, 0);
	}
	return generic_token(player, card, event);
}

int card_beast_token(int player, int card, event_t event){
	return generic_token(player, card, event);
}

int card_insect(int player, int card, event_t event){

	if (event == EVENT_ABILITIES && affect_me(player, card) && get_special_infos(player, card) == 67){
		event_result |= KEYWORD_INFECT;
	}

	return generic_token(player, card, event);
}

int card_gold(int player, int card, event_t event)
{
  /* Gold	""
   * Token Artifact - Gold
   * Sacrifice this token: Add one mana of any color. */

  // Used externally
  if (event == EVENT_CAN_ACTIVATE)
	return can_sacrifice_this_as_cost(player, card) && can_produce_mana(player, card);

  if (event == EVENT_ACTIVATE)
	{
	  ai_modifier -= 36;
	  if (produce_mana_all_one_color(player, get_card_instance(player, card)->mana_color, 1))
		{
		  tapped_for_mana_color = -2;
		  kill_card(player, card, KILL_SACRIFICE);
		}
	}

  if (event == EVENT_COUNT_MANA && affect_me(player, card) && can_produce_mana(player, card))
	declare_mana_available_hex(player, get_card_instance(player, card)->card_color & (COLOR_TEST_ANY | COLOR_TEST_ARTIFACT), 1);

  return 0;
}

int card_golem_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_saproling(int player, int card, event_t event)
{
  // Tokens from Saproling Burst
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && get_special_infos(player, card) == 66)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->damage_source_player >= 0 && instance->damage_source_card >= 0 && in_play(instance->damage_source_player, instance->damage_source_card))
		{
		  set_characteristic_pt(count_counters(instance->damage_source_player, instance->damage_source_card, COUNTER_FADE));
		}
	  else
		kill_card(player, card, KILL_STATE_BASED_ACTION);

	  return 0;
	}

  return generic_token(player, card, event);
}

int card_plant_token(int player, int card, event_t event){

  return generic_token(player, card, event);
}

int card_spirit2(int player, int card, event_t event){

	if( get_special_infos(player, card) == 67 ){
		if( eot_trigger(player, card, event) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	// spirit token from Daxos the Returned
	//"This creature's power and toughness are each equal to the number of experience counters you have."
	if( get_special_infos(player, card) == 66 ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			 set_characteristic_pt(EXPERIENCE_COUNTERS(player));
		}
	}

   return generic_token(player, card, event);
}

int card_thopter2(int player, int card, event_t event){

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE );
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE && get_special_infos(player, card) == 66 && has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0)){
		return can_target(&td);
	}

	else if( event == EVENT_ACTIVATE ){
			if( charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) && pick_target(&td, "TARGET_CREATURE_OR_PLAYER") ){
				kill_card(player, card, KILL_SACRIFICE);
			}
	}

	else if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		  damage_target0(player, card, 1);
	}

	return generic_token(player, card, event);
}

int card_triskelavite(int player, int card, event_t event){
  /* Triskelavite	""(token)	0x200cd11
   * Artifact Creature - Triskelavite 1/1
   * Flying
   * Sacrifice this creature: This creature deals 1 damage to any target. */

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE );
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_ACTIVATE && has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) ){
		return can_target(&td);
	}

	else if( event == EVENT_ACTIVATE ){
			if( charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) && pick_target(&td, "TARGET_CREATURE_OR_PLAYER") ){
				kill_card(player, card, KILL_SACRIFICE);
			}
	}

	else if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
			damage_target0(player, card, 1);
	}

	return generic_token(player, card, event);
}

int card_minion(int player, int card, event_t event){
  /* Minion	""
   * Token Creature - Minion * / * */

	if (card == -1){
		return 0;
	}
	return generic_token(player, card, event);
}

int card_zombie(int player, int card, event_t event){
	return generic_token(player, card, event);
}

int card_illusion_token(int player, int card, event_t event){

	if( get_special_infos(player, card) == 66 ){
		if( event == EVENT_ABILITIES && affect_me(player, card) ){
			event_result &= ~KEYWORD_FLYING;
		}
	}

	return generic_token(player, card, event);
}

int card_wolf_token(int player, int card, event_t event)
{
  if (get_special_infos(player, card) == 66 && (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card))
    event_result += count_graveyard_by_id(ANYBODY, CARD_ID_SOUND_THE_CALL);
  if (get_special_infos(player, card) == 67 && this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
    for (int c = 0; c < active_cards_count[player]; ++c)
      if (is_what(player, c, TARGET_TYPE_PLANESWALKER) && has_subtype(player, c, SUBTYPE_GARRUK))
        add_counters(player, card, player, c, COUNTER_LOYALTY, 1);
  return generic_token(player, card, event);
}

// token made by kalitas

int card_vampire(int player, int card, event_t event){
	if (card == -1){
		return 0;
	}
	return generic_token(player, card, event);
}

int card_ooze(int player, int card, event_t event)
{
  if (card == -1)
	return 0;

  // Tokens from Gutter Grime
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && get_special_infos(player, card) == 66)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int pt;
	  if (instance->damage_source_player >= 0 && instance->damage_source_card >= 0 && in_play(instance->damage_source_player, instance->damage_source_card))
		pt = count_counters(instance->damage_source_player, instance->damage_source_card, COUNTER_SLIME);
	  else
		pt = 0;
	  set_characteristic_pt(pt);
	}

  // "Splitting" Tokens from Mitotic Slime
  if (get_special_infos(player, card) == 67 && graveyard_from_play(player, card, event))
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_OOZE, &token);
	  token.qty = 2;
	  token.pow = 1;
	  token.tou = 1;
	  generate_token(&token);
	}

  return generic_token(player, card, event);
}

int card_graveborn_token(int player, int card, event_t event){

	haste(player, card, event);

	if( get_special_infos(player, card) == 66 && eot_trigger(player, card, event) ){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_token(player, card, event);
}

int card_wurm_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_snake_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_elemental_token(int player, int card, event_t event){

	if (card == -1){
		return 0;
	}

	if( get_special_infos(player, card) == 66 ){
		if( eot_trigger(player, card, event) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( get_special_infos(player, card) == 67 ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			set_characteristic_pt(count_subtype(player, TYPE_CREATURE, -1));
		}
	}

	if (get_special_infos(player, card) == 68){
		upkeep_trigger_ability(player, card, event, player);
		if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
			if (!graveyard_has_shroud(player)){
				seek_grave_for_id_to_reanimate(player, card, player, CARD_ID_REKINDLING_PHOENIX, REANIMATE_HASTE_UNTIL_EOT);
			}
			kill_card(player, card, KILL_SACRIFICE);
		}
	}
	return generic_token(player, card, event);
}

int card_demon_token(int player, int card, event_t event){
	if( get_special_infos(player, card) == 66 ){
		upkeep_trigger_ability(player, card, event, player);
		if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select another creature to sacrifice.");
			test.not_me = 1;
			int sac = new_sacrifice(player, card, player, SAC_AS_COST | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
			if( sac ){
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
			else{
				damage_player(player, 6, player, card);
			}
		}
	}
	return generic_token(player, card, event);
}

int card_twin_token(int player, int card, event_t event){
	if (card == -1){
		return 0;
	}
	return generic_token(player, card, event);
}

int card_human_token(int player, int card, event_t event){

	if( get_special_infos(player, card) == 66 ){
		if( eot_trigger(player, card, event) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}
	return generic_token(player, card, event);
}

int card_dragon_spirit(int player, int card, event_t event){

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		int owner = get_owner(player, card);
		int csvid = CARD_ID_TATSUMASA_THE_DRAGONS_FANG;
		if( check_rfg(owner, csvid) ){
			int fake = get_internal_card_id_from_csv_id(csvid);
			remove_card_from_rfg(owner, csvid);
			int card_added = add_card_to_hand(owner, fake);
			put_into_play(owner, card_added);
		}
	}
	return generic_token(player, card, event);
}

int card_centaur_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_hellion_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_horror_token(int player, int card, event_t event){

	if (card == -1){
		return 0;
	}

	return generic_token(player, card, event);
}

int card_bird_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_goblin_token(int player, int card, event_t event){

	if( get_special_infos(player, card) == 66 ){
		if(trigger_condition == TRIGGER_EOT &&  affect_me(player, card ) && reason_for_trigger_controller == player ){
			if( check_special_flags3(player, card, SF3_WONT_TRIGGER_AT_THIS_EOT) ){
				remove_special_flags3(player, card, SF3_WONT_TRIGGER_AT_THIS_EOT);
			}
			else{
				if( eot_trigger(player, card, event) ){
					kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}
	if( get_special_infos(player, card) == 67 ){
		haste(player, card, event);
	}
	if( get_special_infos(player, card) == 68 ){
		// "Creatures you control attack each combat if able."
		all_must_attack_if_able(player, event, -1);
	}
	return generic_token(player, card, event);
}

int card_spider_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_bat_token(int player, int card, event_t event){

	if( get_special_infos(player, card) == 66 ){
		if( event == EVENT_CAN_ACTIVATE && has_mana_for_activated_ability(player, card, 1, 1, 0, 0, 0, 0) && check_rfg(player, CARD_ID_SENGIR_NOSFERATU)  ){
			return can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
		}
		if( event == EVENT_ACTIVATE ){
			charge_mana_for_activated_ability(player, card, 1, 1, 0, 0, 0, 0);
			if( spell_fizzled != 1 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( check_rfg(player, CARD_ID_SENGIR_NOSFERATU)  ){
				int count = count_rfg(player);
				int *rfg = rfg_ptr[player];
				while( count > -1  ){
						if( cards_data[rfg[count]].id == CARD_ID_SENGIR_NOSFERATU ){
							int card_added = add_card_to_hand(player, rfg[count]);
							remove_card_from_rfg(player, CARD_ID_SENGIR_NOSFERATU);
							put_into_play(player, card_added);
							break;
						}
						count--;
				}
			}
		}
	}
	return generic_token(player, card, event);
}

int card_cat_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_ape_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_knight_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

int card_monk_token(int player, int card, event_t event){
	prowess(player, card, event);
	return generic_token(player, card, event);
}

int card_rhino_token(int player, int card, event_t event){

	if (card == -1){
		return 0;
	}

	if (trigger_condition == TRIGGER_EOT && affect_me(player, card) && player == reason_for_trigger_controller
		&& get_special_infos(player, card) == 66 && eot_trigger(player, card, event)){
		kill_card(player, card, KILL_SACRIFICE);
	}

	return generic_token(player, card, event);
}

int card_survivor_token(int player, int card, event_t event){
	return generic_token(player, card, event);
}

#ifndef SHANDALAR
static int find_any_attached_legacy_effect(CardFunction func_ptr, int target_player_id, int target_card)
{
  for (int card = 0; card < active_cards_count[target_player_id]; ++card)
  {
    card_instance_t *instance = in_play(target_player_id, card);
    if (instance && instance->internal_card_id == LEGACY_EFFECT_CUSTOM &&
        instance->info_slot_fn == func_ptr && instance->damage_target_player == target_player_id &&
        instance->damage_target_card == target_card)
      return card;
  }
  return -1;
}
#endif

int card_assassin_token(int player, int card, event_t event)
{
  if (get_special_infos(player, card) == 66 && damage_dealt_by_me(player, card, event,
      DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_MUST_DAMAGE_OPPONENT))
    lose_the_game(1-player);
  if (get_special_infos(player, card) == 67 && !is_humiliated(player, card, AL_TRIG) && event == EVENT_DEAL_DAMAGE)
  {
    card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
    if (damage->internal_card_id == damage_card && damage->info_slot > 0)
    {
      if (damage->damage_source_player != player || damage->damage_source_card != card)
        return 0;
      if (damage->damage_target_card == -1)
      {
        if ((damage->token_status & (STATUS_COMBAT_DAMAGE | STATUS_FIRST_STRIKE_DAMAGE)) &&
            check_special_flags(player, card, SF_ATTACKING_PWALKER))
          for (int c = 0; c < active_cards_count[player]; ++c)
          {
            card_instance_t* legacy = in_play(player, c);
            if (legacy && legacy->internal_card_id == LEGACY_EFFECT_CUSTOM &&
                legacy->info_slot == (intptr_t)attacking_planeswalker &&
                legacy->damage_target_player == player && legacy->damage_target_card == card)
            {
              legacy = get_card_instance(player, c);
              int target_card = legacy->targets[1].card;
              legacy = get_card_instance(player, c);
              set_special_flags(legacy->targets[1].player, target_card, SF_LETHAL_DAMAGE_DESTROY);
              break;
            }
          }
      }
      else if (is_planeswalker(damage->damage_target_player, damage->damage_target_card))
        set_special_flags(damage->targets[4].player, damage->targets[4].card, SF_LETHAL_DAMAGE_DESTROY);
    }
  }
  return generic_token(player, card, event);
}

int card_elemental_cat(int player, int card, event_t event){

	if( get_special_infos(player, card) == 66 ){
		if( eot_trigger(player, card, event) ){
			kill_card(player, card, KILL_EXILE);
		}
	}
	if( get_special_infos(player, card) == 67 ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) ){
			event_result+=count_subtype(player, TYPE_CREATURE, -1);
		}
	}

	return generic_token(player, card, event);
}

int card_cleric_token(int player, int card, event_t event){

	if( get_special_infos(player, card) == 66 ){
		if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) && ! is_tapped(player, card) && ! is_sick(player, card) ){
			if( has_mana_for_activated_ability(player, card, 3, 2, 0, 0, 0, 1) ){
				return can_sacrifice_as_cost(player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
			}
		}

		if(event == EVENT_ACTIVATE ){
			charge_mana_for_activated_ability(player, card, 3, 2, 0, 0, 0, 1) ;
			if( spell_fizzled != 1 ){
				tap_card(player, card);
				kill_card(player, card, KILL_EXILE);
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			seek_grave_for_id_to_reanimate(player, card, player, CARD_ID_DEATHPACT_ANGEL, REANIMATE_DEFAULT);
		}
	}

	return generic_token(player, card, event);
}

int card_eldrazi_spawn(int player, int card, event_t event){
  /* Eldrazi Spawn	""
   * Token Creature - Eldrazi Spawn 0/1
   * Sacrifice this creature: Add |C. */

	if( event == EVENT_CAN_ACTIVATE && get_special_infos(player, card) != 66 ){
		return 1;
	}
	if( event == EVENT_ACTIVATE ){
		produce_mana(player, COLOR_COLORLESS, 1);
		tapped_for_mana_color = -2;
		kill_card(player, card, KILL_SACRIFICE);
	}
	if( event == EVENT_COUNT_MANA && affect_me(player, card) && get_special_infos(player, card) != 66 ){
		declare_mana_available(player, COLOR_COLORLESS, 1);
	}
	if( get_special_infos(player, card) == 66 ){
		annihilator(player, card, event, 1);
	}
	return generic_token(player, card, event);
}

int card_angel_token(int player, int card, event_t event){

	return generic_token(player, card, event);
}

/* ManalinkEh 2022: 0x2582e50; original cards/tokens.c:589. */
int card_etherium_cell(int player, int card, event_t event)
{
  /* Etherium Cell	""
   * Token Artifact
   * |T, Sacrifice this artifact: Add one mana of any color. */

  int result;
  result = artifact_mana_all_one_color(player, card, event, 1, 1);
  return result;
}

/* October 2022 COFF entry 0x2582e90: native tail jump to generic_token. */
int card_horse_token(int player, int card, event_t event)
{
  return generic_token(player, card, event);
}

/* October 2022 COFF entry 0x2582ea0: native tail jump to generic_token. */
int card_cat_dragon_token(int player, int card, event_t event)
{
  return generic_token(player, card, event);
}

/* ManalinkEh 2022: 0x2582eb0; original cards/tokens.c:604. */
int card_dinosaur_token(int player, int card, event_t event)
{
  int result;
  if (((event == EVENT_ABILITIES) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = get_special_infos(player, card);
    if (result == 0x42)
    {
      event_result = event_result | 0x80;
    }
  }
  result = generic_token(player, card, event);
  return result;
}

/* October 2022 COFF entry 0x2582f20: native tail jump to generic_token. */
int card_pirate_token(int player, int card, event_t event)
{
  return generic_token(player, card, event);
}

/* October 2022 COFF entry 0x2582f30: native tail jump to card_etherium_cell. */
int card_treasure(int player, int card, event_t event)
{
  return card_etherium_cell(player, card, event);
}

/* ManalinkEh 2022: 0x2582f40; original cards/tokens.c:623. */
int card_construct(int player, int card, event_t event)
{
  int result;

  result = get_special_infos(player, card);
  if (result == 0x42)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((((result == 0) && ((uint32_t)(event - EVENT_POWER) < 2)) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      result = count_subtype(player, 0x40, -1);
      event_result = event_result + result;
    }
  }
  result = generic_token(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2582fd0; original cards/tokens.c:632. */
int card_devil(int player, int card, event_t event)
{
  /* Devil	""	0x200ec1f
   * Creature - Devil 1/1
   * When this creature dies, it deals 1 damage to any target. */

  int result;
  target_definition_t td;
  result = get_special_infos(player, card);
  if (result == 0x42)
  {
    result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result != 0)
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;
      result = can_target(&td);
      if (result != 0)
      {
        result = pick_target(&td, "TARGET_ANY");
        if (result != 0)
        {
          damage_target0(player, card, 1);
        }
      }
    }
  }
  result = generic_token(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x25830a0; original cards/tokens.c:647. */
int card_sculpture_token(int player, int card, event_t event)
{
  int result;

  if ((-1 < player) && (-1 < card))
  {
    if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
        (player == affected_card_controller))
    {
      result = get_special_infos(player, card);
      if (result == 0x42)
      {
        result = count_subtype(player, -1, 0x101);
        set_characteristic_pt(result);
      }
    }
    result = generic_token(player, card, event);
    return result;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2583130; original cards/tokens.c:658. */
int card_food(int player, int card, event_t event)
{
  /* Food	""
   * Token Artifact - Food
   * |2, |T, Sacrifice this token: You gain 3 life. */

  int result;
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = generic_token(player, card, event);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      gain_life(player, 3);
    }
  }
  return generic_activated_ability(player, card, event, 9, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x25831f0; original cards/tokens.c:666. */
int card_satyr(int player, int card, event_t event)
{
  int result;
  result = get_special_infos(player, card);
  if (result == 0x42)
  {
    cannot_block(player, card, event);
  }
  result = generic_token(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2583240; original cards/tokens.c:676. */
int card_nightmare_token(int player, int card, event_t event)
{
  int result;

  if ((player < 0) || (card < 0))
  {
    return 0;
  }
  result = get_special_infos(player, card);
  if (result == 0x42)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
      if (event != EVENT_DAT_ABILITY)
      {
        result = blocking(player, card, event);
        if (result == 0)
          return generic_token(player, card, event);
      }
      rfg_top_card_of_deck(1 - player);
      rfg_top_card_of_deck(1 - player);
    }
  }
  return generic_token(player, card, event);
}

/* ManalinkEh 2022: 0x2583310; original cards/tokens.c:691. */
int card_pest_token(int player, int card, event_t event)
{
  int result;
  result = get_special_infos(player, card);
  if (result == 0x42)
  {
    result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result != 0)
    {
      gain_life(player, 1);
    }
  }
  return 0;
}
