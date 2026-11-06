#include "manalink.h"

/* Canonical recovered helper declarations. */
int level_up(int player, int card, event_t event, int x, int b, int u, int g, int r, int w,
                  int l1power, int l1toughness, int l2, int l2power, int l2toughness,
                  keyword_t l2key, sp_keyword_t l2sp, int l3, int l3power, int l3toughness,
                  keyword_t l3key, sp_keyword_t l3sp);
static void level_up_effects(int player, int card, event_t event, int l1power, int l1toughness,
                             int l2, int l2power, int l2toughness, keyword_t l2key, sp_keyword_t l2sp,
                             int l3, int l3power, int l3toughness, keyword_t l3key, sp_keyword_t l3sp);

// Global functions

#ifndef SHANDALAR

int card_aura_finesse(int player, int card, event_t event);

#endif

int when_you_cast(int player, int card, event_t event){
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! is_token(player, card) && ! check_special_flags(player, card, SF_NOT_CAST) ){
			return 1;
		}
	}
	return 0;
}

int annihilator(int player, int card, event_t event, int number)
{
  dat_ability(player, card, event,
              DAT_DONT_REPORT_VALUE | DAT_STORE_IN_INFO_SLOT | DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    impose_sacrifice(player, card, 1 - player, number, TYPE_PERMANENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x24ac850; original cards/rise_of_the_eldrazi.c:25. */
static void level_up_effects(int player, int card, event_t event, int l1power, int l1toughness,
                             int l2, int l2power, int l2toughness, keyword_t l2key, sp_keyword_t l2sp,
                             int l3, int l3power, int l3toughness, keyword_t l3key, sp_keyword_t l3sp)
{
  unsigned int characteristic_event_offset;
  card_instance_t *instance;
  int level_count;
  keyword_t sleighted_protection;
  int ai_score;
  int level_two_score;
  keyword_t level_three_keywords;
  keyword_t level_two_keywords;
  characteristic_event_offset = (uint32_t)(event - EVENT_POWER);
  level_two_keywords = l2key;
  level_three_keywords = l3key;
  if (characteristic_event_offset < 3)
  {
    if (((affected_card == card) && (affected_card_controller == player)) &&
        (instance = in_play(player, card), instance != NULL))
    {
      level_count = count_counters(player, card, COUNTER_LEVEL);
      if (level_count < l3)
      {
        if (l2 <= level_count)
        {
          if (characteristic_event_offset < 2)
          {
            if (event != EVENT_POWER)
            {
              l2power = l2toughness;
            }
            set_base_pt(l2power);
            return;
          }
          if ((l2key & KEYWORD_BASIC_LANDWALK) != KEYWORD_0)
          {
            level_two_keywords = get_hacked_walk(player, card, l2key);
            level_two_keywords = level_two_keywords | l2key & ~KEYWORD_BASIC_LANDWALK;
          }
          if ((level_two_keywords & KEYWORD_PROT_COLORED) != KEYWORD_0)
          {
            sleighted_protection = get_sleighted_protection(player, card, level_two_keywords);
            level_two_keywords = sleighted_protection | level_two_keywords & ~KEYWORD_PROT_COLORED;
          }
          if (l2sp != SP_KEYWORD_0)
          {
            special_abilities(player, card, EVENT_ABILITIES, l2sp, player, card);
          }
          event_result = event_result | level_two_keywords;
        }
      }
      else
      {
        if (characteristic_event_offset < 2)
        {
          l2power = l3power;
          if (event != EVENT_POWER)
          {
            l2power = l3toughness;
          }
          set_base_pt(l2power);
          return;
        }
        if ((l3key & KEYWORD_BASIC_LANDWALK) != KEYWORD_0)
        {
          level_three_keywords = get_hacked_walk(player, card, l3key);
          level_three_keywords = level_three_keywords | l3key & ~KEYWORD_BASIC_LANDWALK;
        }
        if ((level_three_keywords & KEYWORD_PROT_COLORED) != KEYWORD_0)
        {
          sleighted_protection = get_sleighted_protection(player, card, level_three_keywords);
          level_three_keywords =
              sleighted_protection | level_three_keywords & ~KEYWORD_PROT_COLORED;
        }
        if ((l2key & KEYWORD_BASIC_LANDWALK) != KEYWORD_0)
        {
          level_two_keywords = get_hacked_walk(player, card, l2key);
          level_two_keywords = level_two_keywords | l2key & ~KEYWORD_BASIC_LANDWALK;
        }
        if ((level_two_keywords & KEYWORD_PROT_COLORED) != KEYWORD_0)
        {
          sleighted_protection = get_sleighted_protection(player, card, level_two_keywords);
          level_two_keywords = sleighted_protection | level_two_keywords & ~KEYWORD_PROT_COLORED;
        }
        if ((l2sp | l3sp) != SP_KEYWORD_0)
        {
          special_abilities(player, card, EVENT_ABILITIES, l2sp | l3sp, player, card);
        }
        event_result = level_three_keywords | event_result | level_two_keywords;
      }
    }
  }
  else
  {
    if ((event == EVENT_SHOULD_AI_PLAY) &&
        (level_count = count_counters(player, card, COUNTER_LEVEL), level_count != 0))
    {
      if (level_count < l2)
      {
        ai_score = (level_count + 1) * (level_count + 1);
      }
      else
      {
        level_two_score = (l2 + 1) * (l2 + 1);
        ai_score = level_two_score + 0x18;
        if (l2 < level_count)
        {
          if (l3 <= level_count)
          {
            ai_score = level_two_score + 0x30;
            level_count = l3;
          }
          level_count = (level_count - l2) + 1;
          ai_score = ai_score + level_count * level_count;
        }
      }
      if (player == 1)
      {
        ai_modifier = ai_modifier + ai_score;
      }
      else
      {
        ai_modifier = ai_modifier - ai_score;
      }
    }
  }
  return;
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x24ace40; original cards/rise_of_the_eldrazi.c:176. */
int level_up(int player, int card, event_t event, int x, int b, int u, int g, int r, int w,
                  int l1power, int l1toughness, int l2, int l2power, int l2toughness,
                  keyword_t l2key, sp_keyword_t l2sp, int l3, int l3power, int l3toughness,
                  keyword_t l3key, sp_keyword_t l3sp)
{
  int result;
  level_up_effects(player, card, event, l1power, l1toughness, l2, l2power, l2toughness, l2key, l2sp,
      l3, l3power, l3toughness, l3key, l3sp);
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_use_activated_abilities(player, card);
    if ((result != 0) &&
        (result = has_mana_for_activated_ability(player, card, x, b, u, g, r, w), result != 0))
    {
      result = can_sorcery_be_played(player, EVENT_CAN_ACTIVATE);
      return (unsigned int)(result != 0);
    }
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      charge_mana_for_activated_ability(player, card, x, b, u, g, r, w);
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      add_counters(player, card, player, card, COUNTER_LEVEL, 1);
    }
  }
  return 0;
}
#endif

/* Just the effects of leveling up, not activation.  l2sp/l3sp only work for special abilities that can just be set during EVENT_ABILITIES.  Currently only used
 * for deathtouch, indestructibility, lifelink, and vigilance, all of which are ok. */

// Level up, both effects and activation.

int totem_armor(int player, int card, event_t event, int pow, int tou, int key, int s_key)
{

  card_instance_t *instance = get_card_instance(player, card);
  int t_player = instance->damage_target_player;
  int t_card = instance->damage_target_card;

  if (in_play(player, card) && t_player != -1)
  {
    if (instance->targets[7].player == 1)
    {
      return 0;
    }
    instance->targets[7].player = 1;
    if (is_this_dying(t_player, t_card))
    {
      card_instance_t *enchanted = get_card_instance(t_player, t_card);
      if (enchanted->kill_code == KILL_DESTROY || enchanted->kill_code == KILL_BURY)
      {
        int is_tap = is_tapped(t_player, t_card);
        regenerate_target_exe(t_player, t_card);
        if (!is_tap)
        {
          untap_card_no_event(t_player, t_card);
        }
        kill_card(player, card, KILL_SACRIFICE);
      }
    }
    instance->targets[7].player = 0;
  }

  return generic_aura(player, card, event, player, pow, tou, key, s_key, 0, 0, 0);
}

static int rebound_legacy(int player, int card, event_t event)
{
  if (trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) && current_turn == player && reason_for_trigger_controller == player
       && count_upkeeps(player) > 0)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  if (event == EVENT_TRIGGER)
		{
		  if (instance->targets[0].player == 1)
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		  else
			{
			  instance->targets[0].player = 3;
			  event_result |= RESOLVE_TRIGGER_AI(player);
			}
		}

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  if (instance->targets[0].player == 1)
			instance->targets[0].player = 0;	// delay trigger to next upkeep
		  else
			{
			  if (check_rfg(player, instance->targets[0].card) && can_legally_play_csvid(player, instance->targets[0].card))
				play_card_in_exile_for_free(player, player, instance->targets[0].card);

			  kill_card(player, card, KILL_EXILE);
			}
		}

	  if (event == EVENT_END_TRIGGER && instance->targets[0].player == 3)	// trigger declined
		kill_card(player, card, KILL_EXILE);
	}

  return 0;
}

int rebound(int player, int card){

	if( !not_played_from_hand(player, card) ){
		int id = get_id(player, card);
		int legacy = create_legacy_effect(player, card, &rebound_legacy );
		card_instance_t *rebounded = get_card_instance(player, legacy);
		rebounded->targets[0].card = id;
		rebounded->targets[0].player = -1;
		if( current_phase == PHASE_UPKEEP ){
		   rebounded->targets[0].player = 1;
		}
		kill_card(player, card, KILL_EXILE);
		return legacy;
	}
	else{
		 kill_card(player, card, KILL_DESTROY);
		 return -1;
	}
}

/************
* Colorless *
************/
int card_all_is_dust(int player, int card, event_t event){
/* CARD_ID_ALL_IS_DUST	10866
All Is Dust	|7
Tribal Sorcery - Eldrazi
Each player sacrifices all colored permanents he or she controls. */

	if (event == EVENT_RESOLVE_SPELL){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.color = COLOR_TEST_ANY_COLORED;
		new_manipulate_all(player, card, ANYBODY, &test, ACT_KILL_SACRIFICE);
		kill_card(player, card, KILL_DESTROY);
	}
	return basic_spell(player, card, event);
}

int card_artisan_of_kozilek(int player, int card, event_t event){
/* CARD_ID_ARTISAN_OF_KOZILEK	10870
Artisan of Kozilek	|9
Creature - Eldrazi 10/9
When you cast ~, you may return target creature card from your graveyard to the battlefield.
Annihilator 2 (Whenever this creature attacks, defending player sacrifices two permanents.) */

	annihilator(player, card, event, 2);
	if( when_you_cast(player, card, event) ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) > 0 && ! graveyard_has_shroud(player) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 1, AI_MAX_CMC, &test);
		}
	}

	return 0;
}

int card_eldrazi_conscription(int player, int card, event_t event){
/* CARD_ID_ELDRAZI_CONSCRIPTION	10916
Eldrazi Conscription	|8
Tribal Enchantment - Eldrazi Aura
Enchant creature
Enchanted creature gets +10/+10 and has trample and annihilator 2. (Whenever it attacks, defending player sacrifices two permanents.) */

	if( get_card_instance(player, card)->damage_target_player > -1 && ! is_humiliated(player, card, AL_STATIC) )
	{
		card_instance_t *instance = get_card_instance(player, card);
		annihilator(instance->damage_target_player, instance->damage_target_card, event, 2);
	}
	return generic_aura(player, card, event, player, 10, 10, KEYWORD_TRAMPLE, 0, 0, 0, 0);
}

int card_emrakul_the_aeons_torn(int player, int card, event_t event){

	/* Emrakul, the Aeons Torn	|15
	 * Legendary Creature - Eldrazi 15/15
	 * ~ can't be countered.
	 * When you cast ~, take an extra turn after this one.
	 * Flying, protection from colored spells, annihilator 6
	 * When ~ is put into a graveyard from anywhere, its owner shuffles his or her graveyard into his or her library. */

	// Last ability handled in special_mill() and raw_put_iid_on_top_of_graveyard_with_triggers().

	annihilator(player, card, event, 6);

	check_legend_rule(player, card, event);

	cannot_be_countered(player, card, event);

	if( when_you_cast(player, card, event) ){
		alternate_legacy_text(1, player, time_walk_effect(player, card));
	}

	protection_from_colored_spells(player, card, event);
	return 0;
}

int card_hand_of_emrakul(int player, int card, event_t event){
  /* Hand of Emrakul |9
   * Creature � Eldrazi 7/7
   * You may sacrifice four Eldrazi Spawn rather than pay Hand of Emrakul's mana cost.
   * Annihilator 1 (Whenever this creature attacks, defending player sacrifices a permanent.) */

	annihilator(player, card, event, 1);

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select an Eldrazi Spawn to sacrifice.");
		test.id = CARD_ID_ELDRAZI_SPAWN;
		test.qty = 4;
		if( new_can_sacrifice_as_cost(player, card, &test) ){
			get_card_instance( player, card )->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			get_card_instance( player, card )->info_slot = 0;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Sac 4 Eldrazi Spawn\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_PERMANENT, "Select an Eldrazi Spawn to sacrifice.");
				test.id = CARD_ID_ELDRAZI_SPAWN;
				int ts = 0;
				int sacced[4];
				while( ts < 4 ){
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
					if( ts == 4 ){
						kill_card(BYTE2(sacced[i]), BYTE3(sacced[i]), KILL_SACRIFICE);
					}
				}
			}
			 else if( choice == 1 ){
					    charge_mana_from_id(player, card, event, get_id(player, card));
			    }
	             else{
				      spell_fizzled = 1;
			     }
		}
	}

	return 0;
}

static int it_that_betrays_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_IT_THAT_BETRAYS)
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
        if (affected_card_controller == 1 - player &&
            is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
            !is_token(affected_card_controller, affected_card) && kill_code == KILL_SACRIFICE &&
            !check_special_flags(affected_card_controller, affected_card,
                                 SF_KILL_STATE_BASED_ACTION))
        {
          if (instance->targets[11].player < 0)
          {
            instance->targets[11].player = 0;
          }
          int pos = BYTE0(instance->targets[11].player);
          if (pos < 10)
          {
            instance->targets[pos].player = get_owner(affected_card_controller, affected_card);
            instance->targets[pos].card = get_id(affected_card_controller, affected_card);
            SET_BYTE0(instance->targets[11].player)++;
          }
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    card_instance_t *instance = get_card_instance(player, card);
    for (int i = 0; i < BYTE0(result); ++i)
      seek_grave_for_id_to_reanimate(player, card, instance->targets[i].player,
                                     instance->targets[i].card, REANIMATE_DEFAULT);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_it_that_betrays(int player, int card, event_t event){

	/* It That Betrays	|12
	 * Creature - Eldrazi 11/11
	 * Annihilator 2
	 * Whenever an opponent sacrifices a nontoken permanent, put that card onto the battlefield under your control. */

	annihilator(player, card, event, 2);

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)it_that_betrays_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &it_that_betrays_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t* affected = in_play(affected_card_controller, affected_card);
		if( affected && affected_card_controller == 1-player &&
			is_what(affected_card_controller, affected_card, TYPE_PERMANENT) && !is_token(affected_card_controller, affected_card) &&
			affected->kill_code == KILL_SACRIFICE &&
			! check_special_flags(affected_card_controller, affected_card, SF_KILL_STATE_BASED_ACTION) )
		{
			card_instance_t *instance = get_card_instance(player, card);
			if( instance->targets[11].player < 0 ){
				instance->targets[11].player = 0;
			}
			int pos = BYTE0(instance->targets[11].player);
			if( pos < 10 ){
				instance->targets[pos].player = get_owner(affected_card_controller, affected_card);
				instance->targets[pos].card = get_id(affected_card_controller, affected_card);
				SET_BYTE0(instance->targets[11].player)++;
			}
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int i;
			for(i=0; i<amount; i++){
				seek_grave_for_id_to_reanimate(player, card, instance->targets[i].player, instance->targets[i].card, REANIMATE_DEFAULT);
			}
		}
	}

	return 0;
}

int card_kozilek_butcher_of_truth(int player, int card, event_t event){

	/* Kozilek, Butcher of Truth	|10
	 * Legendary Creature - Eldrazi 12/12
	 * When you cast ~, draw four cards.
	 * Annihilator 4
	 * When ~ is put into a graveyard from anywhere, its owner shuffles his or her graveyard into his or her library. */

	// Last ability handled in special_mill() and raw_put_iid_on_top_of_graveyard_with_triggers().

	check_legend_rule(player, card, event);
	annihilator(player, card, event, 4);
	if( when_you_cast(player, card, event) ){
		draw_cards(player, 4 );
	}
	return 0;
}

static const char* targets_a_permanent_you_control(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  int i;
  card_instance_t* instance = get_card_instance(player, card);

  for (i = 0; i < instance->number_of_targets; ++i)
	if (instance->targets[0].player == who_chooses
		&& is_what(instance->targets[0].player, instance->targets[0].card, TYPE_PERMANENT))
	  return NULL;

  return "targets permanent you control";
}

static int targets_a_creature_you_control_with_power_ge_7(int who_chooses, int player, int card)
{
  int i;
  card_instance_t* instance = get_card_instance(player, card);

  for (i = 0; i < instance->number_of_targets; ++i)
	if (instance->targets[0].player == who_chooses
		&& is_what(instance->targets[0].player, instance->targets[0].card, TYPE_PERMANENT))
	  return 1;

  return 0;
}

int card_not_of_this_world(int player, int card, event_t event){
  /* Not of This World	|7
   * Kindred Instant - Eldrazi
   * Counter target spell or ability that targets a permanent you control.
   * This spell costs |7 less to cast if it targets a spell or ability that targets a creature you control with power 7 or greater. */

  /*
	Not of This World |7

	Tribal Instant � Eldrazi

	Counter target spell or ability that targets a permanent you control.

	Not of This World costs {7} less to cast if it targets a spell or ability that targets a creature you control with power 7 or greater.
  */
  target_definition_t td_effect;
  counter_activated_target_definition(player, card, &td_effect, 0);
  td_effect.extra = target_predicate_extra(targets_a_permanent_you_control);
  td_effect.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, 0);
  td_spell.extra = target_predicate_extra(targets_a_permanent_you_control);
  td_spell.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_MODIFY_COST){
		if (instance->info_slot & 4){
			null_casting_cost(player, card);
		}
	}

	if (event == EVENT_CAN_CAST){
		if (counterspell(player, card, event, &td_spell, 0)){
			instance->info_slot = 1;
			if (targets_a_creature_you_control_with_power_ge_7(player, card_on_stack_controller, card_on_stack)){
				instance->info_slot |= 4;
			}
			else{
				instance->info_slot &= ~4;
			}
			return 99;
		}

		if (can_counter_activated_ability(player, card, event, &td_effect)){
			instance->info_slot = 2;
			if (targets_a_creature_you_control_with_power_ge_7(player, stack_cards[stack_size - 1].player, stack_cards[stack_size - 1].card)){
				instance->info_slot |= 4;
			}
			else{
				instance->info_slot &= ~4;
			}
			return 99;
		}
		instance->info_slot = 0;
		return 0;
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		instance->number_of_targets = 0;
		if (instance->info_slot & 1){
			counterspell(player, card, event, &td_spell, 0);
		}
		else if (instance->info_slot & 2){
				cast_counter_activated_ability(player, card, 0);
		}
	}

	if (event == EVENT_RESOLVE_SPELL){
		if (instance->info_slot & 1){
			counterspell(player, card, event, &td_spell, 0);	// destroys card
		}
		else if (instance->info_slot & 2 ){
				resolve_counter_activated_ability(player, card, &td_effect, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_pathrazer_of_ulamog(int player, int card, event_t event)
{
  /* Pathrazer of Ulamog	|11
   * Creature - Eldrazi 9/9
   * Annihilator 3
   * ~ can't be blocked except by three or more creatures. */

  annihilator(player, card, event, 3);
  minblockers_3(player, card, event);
  return 0;
}

int card_skittering_invasion(int player, int card, event_t event){
	/* Skittering Invasion	|7
	 * Tribal Sorcery - Eldrazi
	 * Put five 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to your mana pool." */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 24;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_ELDRAZI_SPAWN, 5);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_spawnsire_of_ulamog(int player, int card, event_t event){
/* Spawnsire of Ulamog	|10
 * Creature - Eldrazi 7/11
 * Annihilator 1
 * |4: Put two 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to your mana pool."
 * |2|0: Cast any number of Eldrazi cards you own from outside the game without paying their mana costs. */

	annihilator(player, card, event, 1);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_SPAWN = 1,
		CHOICE_ELDRAZI_TIME
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(4), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = { 0, 1, has_mana_for_activated_ability(player, card, MANACOST_X(20)) };
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Create 2 Eldrazi Spawn", 	abils[1], 5,
							"It's Eldrazi Time !",		 abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SPAWN ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(4), 0, NULL, NULL);
		}
		if( choice == CHOICE_ELDRAZI_TIME ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(20), 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SPAWN ){
			generate_tokens_by_id(player, card, CARD_ID_ELDRAZI_SPAWN, 2);
		}
		if( instance->info_slot == CHOICE_ELDRAZI_TIME ){
#define RULES_ENGINE_AND_PLAY(player, csvid)	do {																							\
													update_rules_engine(check_card_for_rules_engine(get_internal_card_id_from_csv_id(csvid)));	\
													add_csvid_to_rfg(player, csvid);															\
													play_card_in_exile_for_free(player, player, csvid);											\
												} while (0)
			if( IS_AI(player) ){
				RULES_ENGINE_AND_PLAY(player, CARD_ID_ALL_IS_DUST);
				RULES_ENGINE_AND_PLAY(player, CARD_ID_KOZILEK_BUTCHER_OF_TRUTH);
				RULES_ENGINE_AND_PLAY(player, CARD_ID_KOZILEK_THE_GREAT_DISTORTION);
				RULES_ENGINE_AND_PLAY(player, CARD_ID_ULAMOG_THE_INFINITE_GYRE);
				RULES_ENGINE_AND_PLAY(player, CARD_ID_ULAMOG_THE_CEASELESS_HUNGER);
				RULES_ENGINE_AND_PLAY(player, CARD_ID_EMRAKUL_THE_AEONS_TORN);
				RULES_ENGINE_AND_PLAY(player, CARD_ID_EMRAKUL_THE_PROMISED_END);
			}
			else{
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Choose an Eldrazi spell name.");
				test.subtype = SUBTYPE_ELDRAZI;
				int result = new_card_from_list(player, 2, &test);
				while( result != -1 ){
						RULES_ENGINE_AND_PLAY(player, result);
						result = new_card_from_list(player, 2, &test);
				}
			}
#undef RULES_ENGINE_AND_PLAY
		}
	}

	return 0;
}

int card_ulamog_the_infinite_gyre(int player, int card, event_t event){

	/* Ulamog, the Infinite Gyre	|11
	 * Legendary Creature - Eldrazi 10/10
	 * When you cast ~, destroy target permanent.
	 * Annihilator 4
	 * ~ is indestructible.
	 * When ~ is put into a graveyard from anywhere, its owner shuffles his or her graveyard into his or her library. */

	// Last ability handled in special_mill() and raw_put_iid_on_top_of_graveyard_with_triggers().

	check_legend_rule(player, card, event);
	annihilator(player, card, event, 4);
	indestructible(player, card, event);

	if( when_you_cast(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t* instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_ulamogs_crusher(int player, int card, event_t event){
/* CARD_ID_ULAMOGS_CRUSHER	11069
Ulamog's Crusher	|8
Creature - Eldrazi 8/8
Annihilator 2 (Whenever this creature attacks, defending player sacrifices two permanents.)
~ attacks each turn if able. */

	annihilator(player, card, event, 2);
	return attack_if_able(player, card, event);
}

/* ManalinkEh 2022: 0x24ae4d0; original cards/rise_of_the_eldrazi.c:842. */
int card_affa_guard_hound(int player, int card, event_t event)
{
  /* Affa Guard Hound	|2|W
   * Creature - Hound 2/2
   * Flash
   * When ~ enters the battlefield, target creature gets +0/+3 until end of turn. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 0;
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "TARGET_CREATURE", 0, 0);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 3);
      }
    }
  }
  result = flash(player, card, event);
  return result;
}

/********
* White *
********/
int card_caravan_escort(int player, int card, event_t event)
{
  /* Caravan Escort	|W
   * Creature - Human Knight 1/1
   * Level up |2
   * LEVEL 1-4	2/2
   * LEVEL 5+	5/5
   * First strike */

  return level_up(player, card, event, MANACOST_X(2),
				     1,1,
				  1, 2,2, 0,0,
				  5, 5,5, KEYWORD_FIRST_STRIKE,0);
}

int card_dawnglare_invoker(int player, int card, event_t event){
/* CARD_ID_DAWNGLARE_INVOKER	10899
Dawnglare Invoker	|2|W
Creature - Kor Wizard 2/1
Flying
|8: Tap all creatures target player controls. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, instance->targets[0].player, &this_test, ACT_TAP);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(8), 0, &td, "TARGET_PLAYER");
}

int card_deathless_angel(int player, int card, event_t event)
{
  /* Deathless Angel	|4|W|W
   * Creature - Angel 5/7
   * Flying
   * |W|W: Target creature gains indestructible until end of turn. */

	if (!IS_GAA_EVENT(event))
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
								0, 0, 0, SP_KEYWORD_INDESTRUCTIBLE);
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_W(2), 0, &td, "TARGET_CREATURE");
}

/* Demystify	|W => 0x408b70 (Onslaught)
 * Instant
 * Destroy target enchantment. */

/* Eland Umbra	|1|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature gets +0/+4.
 * Totem armor */

int card_emerge_unschated(int player, int card, event_t event){
/* CARD_ID_EMERGE_UNSCATHED	10918
Emerge Unscathed	|W
Instant
Target creature you control gains protection from the color of your choice until end of turn.
Rebound (If you cast this spell from your hand, exile it as it resolves.
At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int kw = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, kw, 0);
			rebound(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int gideon_jura_force_attack(int player, int card, event_t event){
	// This effect's address is referenced in planeswalker.c to autochoose the attacked planewalker

	event_flags |= EA_FORCE_ATTACK;

	if (event == EVENT_MUST_ATTACK && current_turn == 1-player && !forbid_attack){
		card_instance_t* inst = get_card_instance(player, card);
		if (in_play(inst->damage_source_player, inst->damage_source_card)
			&& is_planeswalker(inst->damage_source_player, inst->damage_source_card)
			&& inst->damage_source_player != current_turn
		   ){
			all_must_attack_if_able(current_turn, event, -1);
		}
	}
	if( current_turn == 1-player && event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_gideon_jura(int player, int card, event_t event)
{
  /* Gideon Jura	|3|W|W
   * Planeswalker - Gideon (6)
   * +2: During target opponent's next turn, creatures that player controls attack ~ if able.
   * -2: Destroy target tapped creature.
   * 0: Until end of turn, ~ becomes a 6/6 Human Soldier creature that's still a planeswalker.
   * Prevent all damage that would be dealt to him this turn. */

  check_legend_rule(player, card, event);



  double_faced_card(player, card, event);

  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
  {
    card_instance_t *instance = get_card_instance(player, card);

    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.allow_cancel = 0;
    td.required_state = TARGET_STATE_TAPPED;

    enum
    {
      CHOICE_FORCE_ATTACK = 1,
      CHOICE_KILL,
      CHOICE_ANIMATE,
    } choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Make creatures attack", 1,
                      count_counters(player, card, COUNTER_LOYALTY) < 6 ? 20 : 5, 2,
                      "Kill tapped creature", can_target(&td), 10, -2, "Become creature", 1,
                      count_counters(player, card, COUNTER_LOYALTY) >= 6 ? 15 : 5, 0);

    if (event == EVENT_CAN_ACTIVATE)
    {
      if (!choice)
        return 0; // else fall through to planeswalker()
    }
    else if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      switch (choice)
      {
      case CHOICE_FORCE_ATTACK:
        break;

      case CHOICE_ANIMATE:
        break;

      case CHOICE_KILL:
        pick_target(&td, "ROYAL_ASSASSIN");
        break;
      }
    }
    else // event == EVENT_RESOLVE_ACTIVATION
      switch (choice)
      {
      case CHOICE_FORCE_ATTACK:;
        int p = instance->parent_controller, c = instance->parent_card;
        if (in_play(p, c))
        {
          create_legacy_effect(p, c, &gideon_jura_force_attack);
        }
        break;

      case CHOICE_ANIMATE:
        true_transform(player, instance->parent_card);
        break;

      case CHOICE_KILL:
        if (valid_target(&td))
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
        break;
      }
  }

  return planeswalker(player, card, event, 6);
}

int card_gideon_jura_animated(int player, int card, event_t event){

	// prevent any damage that would be dealt to me
	if( event == EVENT_DEAL_DAMAGE && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card ){
			if( damage->damage_target_card == card && damage->damage_target_player == player && damage->info_slot > 0 ){
				prevent_damage(affected_card_controller, affected_card, -1);
			}
		}
	}

	// turn back into gideon EOT
	if( eot_trigger(player, card, event ) ){
		true_transform(player, card);
	}

	return planeswalker(player, card, event, 6);
}

/* Glory Seeker	|1|W => Vanilla
 * Creature - Human Soldier 2/2 */

/* Guard Duty	|W
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature has defender. */

/* Harmless Assault	|2|W|W
 * Instant
 * Prevent all combat damage that would be dealt this turn by attacking creatures. */

int card_hedron_field_purists(int player, int card, event_t event)
{
  /* Hedron-Field Purists	|2|W
   * Creature - Human Cleric 0/3
   * Level up |2|W
   * LEVEL 1-4	1/4
   * If a source would deal damage to you or a creature you control, prevent 1 of that damage.
   * LEVEL 5+	2/5
   * If a source would deal damage to you or a creature you control, prevent 2 of that damage. */

  card_instance_t* damage = damage_being_prevented(event);
  if (damage
	  && damage->damage_target_player == player && !damage_is_to_planeswalker(damage))
	{
	  int counters = count_counters(player, card, COUNTER_LEVEL);
	  if (counters >= 5)
		damage->info_slot = MAX(0, damage->info_slot - 2);
	  else if (counters >= 1)
		damage->info_slot--;
	}

  return level_up(player, card, event, MANACOST_XW(2,1),
				     0,3,
				  1, 1,4, 0,0,
				  5, 2,5, 0,0);
}

int card_hyena_umbra(int player, int card, event_t event){
/* CARD_ID_HYENA_UMBRA	10955
Hyena Umbra	|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +1/+1 and has first strike.
Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

	return totem_armor(player, card, event, 1, 1, KEYWORD_FIRST_STRIKE, 0);
}

int card_ikiral_outrider(int player, int card, event_t event)
{
  /* Ikiral Outrider	|1|W
   * Creature - Human Soldier 1/2
   * Level up |4
   * LEVEL 1-3	2/6
   * Vigilance
   * LEVEL 4+	3/10
   * Vigilance */

  // Need to value counters slightly more than usual, so the AI will be willing to pay |4 for the second one.
  if (event == EVENT_SHOULD_AI_PLAY && player == AI)
	{
	  int counters = count_counters(player, card, COUNTER_LEVEL);
	  counters = MIN(4, counters);
	  ai_modifier += counters;
	}

  return level_up(player, card, event, MANACOST_X(4),
				     1,2,
				  1, 2,6,  0,SP_KEYWORD_VIGILANCE,
				  4, 3,10, 0,SP_KEYWORD_VIGILANCE);
}

int card_kabira_vindicator(int player, int card, event_t event)
{
  /* Kabira Vindicator	|3|W
   * Creature - Human Knight 2/4
   * Level up |2|W
   * LEVEL 2-4	3/6
   * Other creatures you control get +1/+1.
   * LEVEL 5+	4/8
   * Other creatures you control get +2/+2. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player)
	{
	  int counters = count_counters(player, card, COUNTER_LEVEL);
	  if (counters >= 5)
		boost_subtype(player, card, event, -1, 2,2, 0,0, BCT_CONTROLLER_ONLY);
	  else if (counters >= 2)
		boost_subtype(player, card, event, -1, 1,1, 0,0, BCT_CONTROLLER_ONLY);
	}

  return level_up(player, card, event, MANACOST_XW(2,1),
				     2,4,
				  2, 3,6, 0,0,
				  5, 4,8, 0,0);
}

int card_knight_of_cliffhaven(int player, int card, event_t event)
{
  /* Knight of Cliffhaven	|1|W
   * Creature - Kor Knight 2/2
   * Level up |3
   * LEVEL 1-3	2/3
   * Flying
   * LEVEL 4+	4/4
   * Flying, vigilance */

  return level_up(player, card, event, MANACOST_X(3),
				     2,2,
				  1, 2,3, KEYWORD_FLYING,0,
				  4, 4,4, KEYWORD_FLYING,SP_KEYWORD_VIGILANCE);
}

/* Kor Line-Slinger	|1|W	=>theros.c:card_epharas_warden 0x200b22c
 * Creature - Kor Scout 0/1
 * |T: Tap target creature with power 3 or less. */

int card_kor_spiritdancer(int player, int card, event_t event)
{
  /* Kor Spiritdancer	|1|W
   * Creature - Kor Wizard 0/2
   * ~ gets +2/+2 for each Aura attached to it.
   * Whenever you cast an Aura spell, you may draw a card. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) )
		event_result += count_auras_enchanting_me(player, card) * 2;

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		test.subtype = SUBTYPE_AURA;
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
	}

	if (event == EVENT_SPA){ draw_a_card(player); }
	return 0;
}

int card_lightmine_field(int player, int card, event_t event){
  /* Lightmine Field |2|W|W
   * Enchantment
   * Whenever one or more creatures attack, Lightmine Field deals damage to each of those creatures equal to the number of attacking creatures. */

	store_attackers(player, card, event, DAT_TRACK, ANYBODY, -1, NULL);
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY)
	{
		card_instance_t* instance = get_card_instance(player, card);
		int amt = instance->info_slot;
		unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
		int total = amt;
		for (--amt; amt >= 0; --amt){
			if (in_play(current_turn, attackers[amt]) )
			{
				damage_creature(current_turn, attackers[amt], total, player, card);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_linvala_keeper_of_silence(int player, int card, event_t event)
{
  /* CARD_ID_LINVALA_KEEPER_OF_SILENCE	10982
   * Linvala, Keeper of Silence	|2|W|W
   * Legendary Creature - Angel 3/4
   * Flying
   * Activated abilities of creatures your opponents control can't be activated. */

  check_legend_rule(player, card, event);
  if (in_play(player, card) && !is_humiliated(player, card, AL_STATIC))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    remove_abilities_by_test(1 - player, event, &test, AL_ALL_ACT);
  }
  return 0;
}

int card_lone_missionary(int player, int card, event_t event){
/* CARD_ID_LONE_MISSIONARY	10984
Lone Missionary	|1|W
Creature - Kor Monk 2/1
When ~ enters the battlefield, you gain 4 life. */

	// Also code for Inspiring Cleric, Shu Soldier-Farmers, Spiritual Guardian, Staunch Defenders, Teroh's Faithful
	return cip_lifegain(player, card, event, 4);
}

/* Luminous Wake	|2|W
 * Enchantment - Aura
 * Enchant creature
 * Whenever enchanted creature attacks or blocks, you gain 4 life. */

/* Makindi Griffin	|3|W => Vanilla
 * Creature - Griffin 2/4
 * Flying */

int card_mammoth_umbra(int player, int card, event_t event){
/* CARD_ID_MAMMOTH_UMBRA	10990
Mammoth Umbra	|4|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/+3 and has vigilance.
Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

	return totem_armor(player, card, event, 3, 3, 0, SP_KEYWORD_VIGILANCE);
}

int card_near_death_experience(int player, int card, event_t event){

	/* Near-Death Experience	|2|W|W|W
	 * Enchantment
	 * At the beginning of your upkeep, if you have exactly 1 life, you win the game. */

	if( life[player] == 1 &&
		(event == EVENT_SHOULD_AI_PLAY ||
		 (current_turn == player && upkeep_trigger(player, card, event)))
	  ){
		lose_the_game(1-player);
	}

	return global_enchantment(player, card, event);
}

int card_nomads_assembly(int player, int card, event_t event){
	/* Nomads' Assembly	|4|W|W
	 * Sorcery
	 * Put a 1/1 |Swhite Kor Soldier creature token onto the battlefield for each creature you control.
	 * Rebound */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) * 8);
	}

	if(event == EVENT_RESOLVE_SPELL){
		int total = count_subtype(player, TYPE_CREATURE, -1);
		generate_tokens_by_id(player, card, CARD_ID_KOR_SOLDIER, total);
		rebound(player, card);
	}

	return basic_spell(player, card, event);
}

int card_oust(int player, int card, event_t event){
	/* Oust	|W
	 * Sorcery
	 * Put target creature into its owner's library second from the top. Its controller gains 3 life. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allow_cancel = 0;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td)  ){
			put_permanent_under_the_first_x_card_of_its_owners_library(instance->targets[0].player, instance->targets[0].card, 1);
			gain_life(instance->targets[0].player, 3);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_puncturing_light(int player, int card, event_t event){
/* CARD_ID_PUNCTURING_LIGHT	11022
Puncturing Light	|1|W
Instant
Destroy target attacking or blocking creature with power 3 or less. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.power_requirement = 3 | TARGET_PT_LESSER_OR_EQUAL;
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						"Select target attacking or blocking creature with Power 3 or less.", 1, NULL);
}

/* Repel the Darkness	|2|W
 * Instant
 * Tap up to two target creatures.
 * Draw a card. */

/* Smite	|W (Stronghold)
 * Instant
 * Destroy target blocked creature. */

int card_souls_attendant(int player, int card, event_t event)
{
/* CARD_ID_SOULS_ATTENDANT	11043
Soul's Attendant	|W
Creature - Human Cleric 1/1
Whenever another creature enters the battlefield, you may gain 1 life. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.not_me = 1;

		etb_ability_global(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 1);
	}

	return 0;
}

/* Soulbound Guardians	|4|W (vanilla)
 * Creature - Kor Spirit 4/5
 * Defender, flying */

/* Stalwart Shield-Bearers	|1|W
 * Creature - Human Soldier 0/3
 * Defender
 * Other creatures you control with defender get +0/+2. */

int card_student_of_warfare(int player, int card, event_t event)
{
  /* Student of Warfare	|W
   * Creature - Human Knight 1/1
   * Level up |W
   * LEVEL 2-6	3/3
   * First strike
   * LEVEL 7+	4/4
   * Double strike */

  return level_up(player, card, event, MANACOST_W(1),
				     1,1,
				  2, 3,3, KEYWORD_FIRST_STRIKE,0,
				  7, 4,4, KEYWORD_DOUBLE_STRIKE,0);
}

int card_survival_cache(int player, int card, event_t event){
/* CARD_ID_SURVIVAL_CACHE	11060
Survival Cache	|2|W
Sorcery
You gain 2 life. Then if you have more life than an opponent, draw a card.
Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep,
you may cast this card from exile without paying its mana cost.) */

// 0x200f55c
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (life[player] < 6 ? 48 : 12);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		gain_life(player, 2);
		if( life[player] > life[1-player] ){
			draw_cards(player, 1);
		}
		rebound(player, card);
	}

	return basic_spell(player, card, event);
}

int card_time_of_heroes(int player, int card, event_t event)
{
  /* Time of Heroes	|1|W
   * Enchantment
   * Each creature you control with a level counter on it gets +2/+2. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card, AL_STATIC)
	  && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && count_counters(affected_card_controller, affected_card, COUNTER_LEVEL) > 0)
	event_result += 2;

  return global_enchantment(player, card, event);
}

int card_totem_guide_hartebeest(int player, int card, event_t event)
{
  /* Totem-Guide Hartebeest	|4|W
   * Creature - Antelope 2/5
   * When ~ enters the battlefield, you may search your library for an Aura card, reveal it, put it into your hand,
   *	then shuffle your library. */

 	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an Aura card.");
		test.subtype = SUBTYPE_AURA;
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

int card_transcendent_master(int player, int card, event_t event)
{
  /* Transcendent Master	|1|W|W
   * Creature - Human Cleric Avatar 3/3
   * Level up |1
   * LEVEL 6-11	6/6
   * Lifelink
   * LEVEL 12+	9/9
   * Lifelink
   * ~ is indestructible. */

  return level_up(player, card, event, MANACOST_X(1),
				     3,3,
				  6, 6,6, 0,SP_KEYWORD_LIFELINK,
				  12,9,9, 0,SP_KEYWORD_LIFELINK|SP_KEYWORD_INDESTRUCTIBLE);
}

/* Umbra Mystic	|2|W
 * Creature - Human Wizard 2/2
 * Auras attached to permanents you control have totem armor. */

/* Wall of Omens	|1|W => ravnica.c:Carven Caryatid
 * Creature - Wall 0/4
 * Defender
 * When ~ enters the battlefield, draw a card. */

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x24aff70; original cards/rise_of_the_eldrazi.c:1538. */
int card_aura_finesse(int player, int card, event_t event)
{
  /* Aura Finesse	|U
   * Instant
   * Attach target Aura you control to target creature.
   * Draw a card. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 4);
  td.required_subtype = 0x4000;
  td.allowed_controller = player;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result == 0)
    {
      return 0;
    }
    set_special_flags(instance->targets[0].player, instance->targets[0].card,
                           SF_MOVE_AURA_LEGAL_TARGET);
    result = call_card_function(instance->targets[0].player, instance->targets[0].card,
                                EVENT_CAN_MOVE_AURA);
    if (result != 0)
    {
      call_card_function(instance->targets[0].player, instance->targets[0].card,
                         EVENT_MOVE_AURA);
      call_card_function(instance->targets[0].player, instance->targets[0].card,
                         EVENT_RESOLVE_MOVING_AURA);
    }
    remove_special_flags(instance->targets[0].player, instance->targets[0].card,
                              SF_MOVE_AURA_LEGAL_TARGET);
    draw_a_card(player);
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td, "Select target Aura you control", 1, NULL);
}
#endif

/* Cast Through Time	|4|U|U|U
 * Enchantment
 * Instant and sorcery spells you control have rebound. */

/*******
* Blue *
*******/

int card_champions_drake(int player, int card, event_t event)
{
  /* Champion's Drake	|1|U
   * Creature - Drake 1/1
   * Flying
   * ~ gets +3/+3 as long as you control a creature with three or more level counters on it. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		int c;
		for (c = 0; c < active_cards_count[player]; ++c){
			if (in_play(player, c) && is_what(player, c, TYPE_CREATURE) && count_counters(player, c, COUNTER_LEVEL) >= 3)
			{
				event_result += 3;
				break;
			}
		}
	}

	return 0;
}

int card_coralhelm_commander(int player, int card, event_t event)
{
  /* Coralhelm Commander	|U|U
   * Creature - Merfolk Soldier 2/2
   * Level up |1
   * LEVEL 2-3	3/3
   * Flying
   * LEVEL 4+	4/4
   * Flying
   * Other Merfolk creatures you control get +1/+1. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) &&
		affected_card_controller == player &&
		! is_humiliated(player, card, AL_STATIC) &&
		count_counters(player, card, COUNTER_LEVEL) >= 4 )
		boost_subtype(player, card, event, SUBTYPE_MERFOLK, 1,1, 0,0, BCT_CONTROLLER_ONLY);

	return level_up(player, card, event, MANACOST_X(1),
				    2, 2,
					2, 3, 3, KEYWORD_FLYING,0,
					4, 4, 4, KEYWORD_FLYING,0);
}

/* Crab Umbra	|U
 * Enchantment - Aura
 * Enchant creature
 * |2|U: Untap enchanted creature.
 * Totem armor */

int card_deprive(int player, int card, event_t event){
/* CARD_ID_DEPRIVE	10903
Deprive	|U|U
Instant
As an additional cost to cast ~, return a land you control to its owner's hand.
Counter target spell. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);


	if( event == EVENT_CAN_CAST ){
		if( can_target(&td) ){
			return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
		if( spell_fizzled != 1 && new_pick_target(&td, "Select a land you control, return it to its owner's hand.", 1, 1 | GS_LITERAL_PROMPT) ){
			bounce_permanent(instance->targets[1].player, instance->targets[1].card);
			instance->number_of_targets = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, NULL, 0);
	}

	return 0;
}

int card_distortion_strike(int player, int card, event_t event){
/* CARD_ID_DISTORTION_STRIKE	10906
Distortion Strike	|U
Sorcery
Target creature gets +1/+0 until end of turn and can't be blocked this turn.
Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep,
you may cast this card from exile without paying its mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
									1, 0, 0, SP_KEYWORD_UNBLOCKABLE);
			rebound(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_domestication(int player, int card, event_t event){

	/* Domestication	|2|U|U
	 * Enchantment - Aura
	 * Enchant creature
	 * You control enchanted creature.
	 * At the beginning of your end step, if enchanted creature's power is 4 or greater, sacrifice ~. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( current_turn == player && eot_trigger(player, card, event) ){
			card_instance_t *instance = get_card_instance(player, card);
			int p = instance->damage_target_player;
			int c = instance->damage_target_card;
			if( p > -1 && get_power(p, c) >= 4 ){
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	return card_control_magic(player, card, event);
}

int card_dormant_gomazoa(int player, int card, event_t event){
/* CARD_ID_DORMANT_GOMAZOA	10908
Dormant Gomazoa	|1|U|U
Creature - Jellyfish 5/5
Flying
~ enters the battlefield tapped.
~ doesn't untap during your untap step.
Whenever you become the target of a spell, you may untap ~. */

	comes_into_play_tapped(player, card, event);

	does_not_untap(player, card, event);

	if( ! is_humiliated(player, card, AL_TRIG) &&
		any_becomes_target(player, card, event, player, 1, TYPE_NONE, -1, TYPE_NONEFFECT, ANYBODY, RESOLVE_TRIGGER_DUH) )
	{
		untap_card(player, card);
	}

	return 0;
}

int card_drake_umbra(int player, int card, event_t event){
/* CARD_ID_DRAKE_UMBRA	10909
Drake Umbra	|4|U
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/+3 and has flying.
Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

	return totem_armor(player, card, event, 3, 3, KEYWORD_FLYING, 0);
}

int card_echo_mage(int player, int card, event_t event){

	/* Echo Mage	|1|U|U
	 * Creature - Human Wizard 2/3
	 * Level up |1|U
	 * LEVEL 2-3	2/4
	 * |U|U, |T: Copy target instant or sorcery spell. You may choose new targets for the copy.
	 * LEVEL 4+	2/5
	 * |U|U, |T: Copy target instant or sorcery spell twice. You may choose new targets for the copies. */

	level_up_effects(player, card, event, 2, 3, 2, 2, 4, 0, 0, 4, 2, 5, 0, 0);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_SPELL);
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_LEVEL_UP = 1,
		CHOICE_TWINCAST
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_UNTAPPED, MANACOST_U(2), 0, &td, NULL) &&
			count_counters(player, card, COUNTER_LEVEL) >= 2 )
		{
			return 99;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XU(1, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
            0,
            has_mana_multi(player, MANACOST_XU(1, 1)) && can_sorcery_be_played(player, event),
            count_counters(player, card, COUNTER_LEVEL) >= 2 &&
            generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SPELL_ON_STACK | GAA_UNTAPPED, MANACOST_U(2), 0, &td, NULL) ? 1 : 0
        };
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Level up", abils[1], 5,
							"Copy spell", abils[2], 15);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_LEVEL_UP ){
			int mod = count_counters(player, card, COUNTER_LEVEL) < 2 ? 24 : 12;
			ai_modifier += (current_phase > PHASE_DECLARE_BLOCKERS ? (mod * 2) : mod);
			return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XU(1, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_TWINCAST ){
			instance->targets[1].player = count_counters_no_parent(player, card, COUNTER_LEVEL);
			return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_UNTAPPED, MANACOST_U(2), 0, &td, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LEVEL_UP ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_LEVEL, 1);
		}
		if( instance->info_slot == CHOICE_TWINCAST ){
			activate_twincast(player, card, event, NULL, NULL);
			if( instance->targets[1].player >= 4 ){
				activate_twincast(player, card, event, NULL, NULL);
			}
		}
	}

	return 0;
}

int card_eel_umbra(int player, int card, event_t event){
/* Eel Umbra	|1|U	0x200f561
 * Enchantment - Aura
 * Flash
 * Enchant creature
 * Enchanted creature gets +1/+1.
 * Totem armor */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == 1-player ? 24 : -24);
	}

	return totem_armor(player, card, event, 1, 1, 0, 0);
}

int card_enclave_cryptologist(int player, int card, event_t event){

	/* Enclave Cryptologist	|U
	 * Creature - Merfolk Wizard 0/1
	 * Level up |1|U
	 * LEVEL 1-2	0/1
	 * |T: Draw a card, then discard a card.
	 * LEVEL 3+	0/1
	 * |T: Draw a card. */

	level_up_effects(player, card, event, 0, 1, 1, 0, 1, 0, 0, 3, 0, 1, 0, 0);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_LEVEL_UP = 1,
		CHOICE_DRAW
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) &&
			count_counters(player, card, COUNTER_LEVEL) >= 1 )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XU(1, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int abils[3] = {
						0,
						has_mana_multi(player, MANACOST_XU(1, 1)) && can_sorcery_be_played(player, event),
						count_counters(player, card, COUNTER_LEVEL) >= 1 &&
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ? 1 : 0
		};
		char msg[100];
		if( count_counters(player, card, COUNTER_LEVEL) < 3 ){
			scnprintf(msg, 100, "Draw & discard");
		}
		else{
			scnprintf(msg, 100, "Draw a card");
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Level up", abils[1], 5,
							msg, abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_LEVEL_UP ){
			int mod = count_counters(player, card, COUNTER_LEVEL) < 1 ? 24 : 12;
			ai_modifier += (current_phase > PHASE_DECLARE_BLOCKERS ? (mod * 2) : mod);
			return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XU(1, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_DRAW ){
			instance->targets[1].player = count_counters_no_parent(player, card, COUNTER_LEVEL);
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LEVEL_UP ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_LEVEL, 1);
		}
		if( instance->info_slot == CHOICE_DRAW ){
			draw_cards(player, 1);
			if( instance->targets[1].player < 3 ){
				discard(player, 0, player);
			}
		}
	}

	return 0;
}

/* Fleeting Distraction	|U => avacyn_restored.c
 * Instant
 * Target creature gets -1/-0 until end of turn.
 * Draw a card. */

int card_frostwing_invoker(int player, int card, event_t event){
/* Frostwind Invoker	|4|U	0x200f566
 * Creature - Merfolk Wizard 3/3
 * Flying
 * |8: Creatures you control gain flying until end of turn. */

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (current_phase < PHASE_DECLARE_BLOCKERS ? 24 : -24);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_subtype_until_eot(instance->parent_controller, instance->parent_card, player, -1, 0, 0, KEYWORD_FLYING, 0);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_X(8), 0, NULL, NULL);
}

int card_gravitational_shift(int player, int card, event_t event){
/* CARD_ID_GRAVITATIONAL_SHIFT	10939
Gravitational Shift	|3|U|U
Enchantment
Creatures with flying get +2/+0.
Creatures without flying get -2/-0. */

	if( event == EVENT_POWER && is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_humiliated(player, card, AL_STATIC) ){
		if( check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) ){
			event_result+=2;
		}
		else{
			event_result-=2;
		}
	}

	return global_enchantment(player, card, event);
}

int card_guard_gomazoa(int player, int card, event_t event)
{
  /* CARD_ID_GUARD_GOMAZOA	10944
   * Guard Gomazoa	|2|U
   * Creature - Jellyfish 1/3
   * Defender, flying
   * Prevent all combat damage that would be dealt to ~. */

  card_instance_t *instance;

  if ((((event == EVENT_CHECK_ABILITIES) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (current_phase < 0x18))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      check_abilities_toughness = check_abilities_toughness + 0x3fff;
    }
  }
  instance = combat_damage_being_prevented(event);
  if (((instance != NULL) && (instance->damage_target_card == card)) &&
      (instance->damage_target_player == player))
  {
    if (!is_humiliated(player, card, AL_STATIC))
    {
      prevent_damage(affected_card_controller, affected_card, -1);
    }
  }
  return 0;
}

int card_hada_spy_patrol(int player, int card, event_t event)
{
  /* Hada Spy Patrol	|1|U
   * Creature - Human Rogue 1/1
   * Level up |2|U
   * LEVEL 1-2	2/2
   * ~ is unblockable.
   * LEVEL 3+	3/3
   * Shroud
   * ~ is unblockable. */

  return level_up(player, card, event, MANACOST_XU(2,1),
				     1,1,
				  1, 2,2, 0,             SP_KEYWORD_UNBLOCKABLE,
				  3, 3,3, KEYWORD_SHROUD,SP_KEYWORD_UNBLOCKABLE);
}

int card_halimar_wavewatch(int player, int card, event_t event)
{
  /* Halimar Wavewatch	|1|U
   * Creature - Merfolk Soldier 0/3
   * Level up |2
   * LEVEL 1-4	0/6
   * LEVEL 5+	6/6
   * |H2Islandwalk */

  return level_up(player, card, event, MANACOST_X(2),
				     0,3,
				  1, 0,6, 0,0,
				  5, 6,6, KEYWORD_ISLANDWALK,0);
}

/* Jwari Scuttler	|2|U => Vanilla
 * Creature - Crab 2/3 */

int card_lay_bare(int player, int card, event_t event){
/* CARD_ID_LAY_BARE	10978
Lay Bare	|2|U|U
Instant
Counter target spell. Look at its controller's hand. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			reveal_target_player_hand(instance->targets[0].player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_COUNTERSPELL, NULL, NULL, 1, NULL);
}

int card_lighthouse_chronologist(int player, int card, event_t event)
{
  /* Lighthouse Chronologist	|1|U
   * Creature - Human Wizard 1/3
   * Level up |U
   * LEVEL 4-6	2/4
   * LEVEL 7+	3/5
   * At the beginning of each end step, if it's not your turn, take an extra turn after this one. */

	if( trigger_condition == TRIGGER_EOT && current_turn != player && count_counters(player, card, COUNTER_LEVEL) >= 7 &&
		eot_trigger(player, card, event) )
		time_walk_effect(player, card);

	return level_up(player, card, event, MANACOST_U(1),
				     1,3,
				  4, 2,4, 0,0,
				  7, 3,5, 0,0);
}

/* Merfolk Observer	|1|U
 * Creature - Merfolk Rogue 2/1
 * When ~ enters the battlefield, look at the top card of target player's library. */

/* Merfolk Skyscout	|2|U|U
 * Creature - Merfolk Scout 2/3
 * Flying
 * Whenever ~ attacks or blocks, untap target permanent. */

/* Mnemonic Wall	|4|U => m13.c:Archaeomancer
 * Creature - Wall 0/4
 * Defender
 * When ~ enters the battlefield, you may return target instant or sorcery card from your graveyard to your hand. */

int card_narcolepsy(int player, int card, event_t event){
/* CARD_ID_NARCOLEPSY	10998
Narcolepsy	|1|U
Enchantment - Aura
Enchant creature
At the beginning of each upkeep, if enchanted creature is untapped, tap it. */

	if( ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_tapped(instance->damage_target_player, instance->damage_target_card) && upkeep_trigger(player, card, event) ){
			tap_card( instance->damage_target_player, instance->damage_target_card );
		}
	}
	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return disabling_aura(player, card, event);
}

/* Phantasmal Abomination	|1|U|U	=>mirage.c:card_skulking_ghost 0x2003167
 * Creature - Illusion 5/5
 * Defender
 * When ~ becomes the target of a spell or ability, sacrifice it. */

/* Reality Spasm	|X|U|U
 * Instant
 * Choose one - Tap X target permanents; or untap X target permanents. */

int card_recurring_insight(int player, int card, event_t event){
/* CARD_ID_RECURRING_INSIGHT	11028
Recurring Insight	|4|U|U
Sorcery
Draw cards equal to the number of cards in target opponent's hand.
Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep,
you may cast this card from exile without paying its mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (hand_count[1-player] * 8);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(player, hand_count[instance->targets[0].player]);
			rebound(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

/* Regress	|2|U => 0x42c6d0:Boomerang
 * Instant
 * Return target permanent to its owner's hand. */

int card_renegade_doppelganger(int player, int card, event_t event)
{
  /* Renegade Doppelganger	|1|U
   * Creature - Shapeshifter 0/1
   * Whenever another creature enters the battlefield under your control, you may have ~ become a copy of that creature until end of turn. */

  if (trigger_condition == TRIGGER_COMES_INTO_PLAY)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.not_me = 1;

	  if (new_specific_cip(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  shapeshift_target(player, card, player, card, instance->targets[1].player, instance->targets[1].card, SHAPESHIFT_UNTIL_EOT);
		}
	}

  return 0;

}

int card_sea_gate_oracle(int player, int card, event_t event)
{
  /* Sea Gate Oracle	|2|U
   * Creature - Human Wizard 1/3
   * When ~ enters the battlefield, look at the top two cards of your library. Put one of them into your hand and the other on the bottom of your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int amount = MIN(2, count_deck(player));
		if( amount ){
			reveal_top_cards_of_library_and_choose_type(player, card, player, amount, 1, TUTOR_HAND, 0, TUTOR_BOTTOM_OF_DECK, 0, TYPE_ANY);
		}
	}

	return 0;
}

int card_see_beyond(int player, int card, event_t event){

	/* See Beyond	|1|U
	 * Sorcery
	 * Draw two cards, then shuffle a card from your hand into your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		ai_modifier += (total <= 3 || total >= 6 ? 48 : 24);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		if( hand_count[player] ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to reshuffle into your deck.");
			if( IS_AI(player) ){
				int total = get_total_available_lands(player);
				if( total <= 3 ){
					test.type = TYPE_LAND;
				}
				if( total >= 6 ){
					test.type = TYPE_LAND;
					test.type_flag = DOESNT_MATCH;
				}
			}
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
			if( selected == -1 ){
				default_test_definition(&test, TYPE_ANY);
				selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
			}
			put_on_top_of_deck(player, selected);
			shuffle(player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_shared_discovery(int player, int card, event_t event)
{
  /* Shared Discovery	|U	0x200f56b
   * Sorcery
   * As an additional cost to cast ~, tap four untapped creatures you control.
   * Draw three cards. */

  int p;
  bool untap_synergy;
  card_instance_t *instance;
  card_instance_t *candidate;
  int candidate_id_or_result;
  int i;
  int other_card_or_selected_count;
  target_t *selected_target;
  int untapped_creatures;
  target_definition_t td;
  char msg[100];

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.illegal_abilities = 0;
    td.illegal_state = TARGET_STATE_TAPPED;
    get_card_instance(player, card);
    if (basic_spell(player, card, EVENT_CAN_CAST) != 0)
    {
      other_card_or_selected_count = target_available(player, card, &td);
      return (unsigned int)(3 < other_card_or_selected_count);
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.allowed_controller = player;
      td.illegal_abilities = 0;
      td.illegal_state = TARGET_STATE_TAPPED;
      instance = get_card_instance(player, card);
      p = current_turn;
      if ((card == affected_card) && (player == affected_card_controller))
      {
        if (0x1d < current_phase)
        {
          i = active_cards_count[current_turn] - 1;
          untapped_creatures = 0;
          untap_synergy = false;
          if (i >= 0)
          {
            do
            {
              candidate = in_play(p, i);
              if (candidate != NULL &&
                  (candidate_id_or_result = is_what(p, i, 0x47), candidate_id_or_result != 0) &&
                  (candidate_id_or_result = is_humiliated(p, i, AL_TRIG),
                   candidate_id_or_result == 0))
              {
                candidate_id_or_result = get_id(p, i);
                if (candidate_id_or_result == 0x1921 || candidate_id_or_result == 0x34d1)
                {
                  if (player == p)
                    untap_synergy = true;
                }
                else if (candidate_id_or_result == 0xd0b)
                  untap_synergy = true;
                if (player == p)
                {
                  candidate_id_or_result = is_what(p, i, 2);
                  if (candidate_id_or_result != 0)
                  {
                    candidate_id_or_result = is_tapped(p, i);
                    untapped_creatures += (unsigned int)(candidate_id_or_result == 0);
                  }
                }
              }
              --i;
            } while (i != -1);
            i = 1 - current_turn;
            other_card_or_selected_count = active_cards_count[i] - 1;
          }
          else
          {
            i = 1 - current_turn;
            other_card_or_selected_count = active_cards_count[i] - 1;
          }
          while (other_card_or_selected_count >= 0)
          {
            candidate = in_play(i, other_card_or_selected_count);
            if (candidate != NULL &&
                (candidate_id_or_result = is_what(i, other_card_or_selected_count, 0x47),
                 candidate_id_or_result != 0) &&
                (candidate_id_or_result = is_humiliated(i, other_card_or_selected_count, AL_TRIG),
                 candidate_id_or_result == 0))
            {
              candidate_id_or_result = get_id(i, other_card_or_selected_count);
              if (candidate_id_or_result == 0x1921 || candidate_id_or_result == 0x34d1)
              {
                if (player == i)
                  untap_synergy = true;
              }
              else if (candidate_id_or_result == 0xd0b)
                untap_synergy = true;
              if (player == i)
              {
                candidate_id_or_result = is_what(i, other_card_or_selected_count, 2);
                if (candidate_id_or_result != 0)
                {
                  candidate_id_or_result = is_tapped(i, other_card_or_selected_count);
                  untapped_creatures += (unsigned int)(candidate_id_or_result == 0);
                }
              }
            }
            --other_card_or_selected_count;
          }
          if (untap_synergy)
            ai_modifier += 0x30;
          other_card_or_selected_count = count_subtype(1 - player, 2, -1);
          if (other_card_or_selected_count < untapped_creatures + -3)
          {
            ai_modifier = ai_modifier + 0x18;
          }
        }
        other_card_or_selected_count = 0;
        do
        {
          i = other_card_or_selected_count + 1;
          scnprintf(msg, 100, "Select a creature you control to tap (%d of 4)", i);
          if (new_pick_target(&td, msg, other_card_or_selected_count, 0x81) == 0)
          {
            i = other_card_or_selected_count;
            break;
          }
          state_untargettable(instance->targets[other_card_or_selected_count].player,
                              instance->targets[other_card_or_selected_count].card, 1);
          other_card_or_selected_count = i;
        } while (i != 4);
        if (i > 0)
        {
          selected_target = instance->targets;
          do
          {
            state_untargettable(selected_target->player, selected_target->card, 0);
            if (i == 4)
            {
              tap_card(selected_target->player, selected_target->card);
            }
            selected_target = selected_target + 1;
          } while (instance->targets + i != selected_target);
        }
        instance->number_of_targets = '\0';
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.illegal_abilities = 0;
    td.illegal_state = TARGET_STATE_TAPPED;
    get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      draw_cards(player, 3);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

int card_skywatcher_adept(int player, int card, event_t event)
{
  /* Skywatcher Adept	|U
   * Creature - Merfolk Wizard 1/1
   * Level up |3
   * LEVEL 1-2	2/2
   * Flying
   * LEVEL 3+	4/2
   * Flying */

  return level_up(player, card, event, MANACOST_X(3),
				     1,1,
				  1, 2,2, KEYWORD_FLYING,0,
				  3, 4,2, KEYWORD_FLYING,0);
}

int card_sphinx_of_magosi(int player, int card, event_t event)
{
  /* CARD_ID_SPHINX_OF_MAGOSI	11048
   * Sphinx of Magosi	|3|U|U|U
   * Creature - Sphinx 6/6
   * Flying
   * |2|U: Draw a card, then put a +1/+1 counter on ~. */

  int result;
  card_instance_t *instance;
  if (1 < (uint32_t)(event - EVENT_POW_BOOST))
  {
    if (event == EVENT_ACTIVATE)
    {
      if (0x1d < current_phase)
      {
        ai_modifier = ai_modifier + (unsigned int)((1 - player == current_turn) ? 0 : -36) + 0x18;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      draw_cards(player, 1);
      add_counters(player, card, instance->parent_controller, instance->parent_card,
                               COUNTER_P1_P1, 1);
    }
    result = generic_activated_ability(player, card, event, 0, 2, 0, 1, 0, 0, 0, 0, NULL, NULL);
    return result;
  }
  return generic_shade(player, card, event, 0, 2, 0, 1, 0, 0, 0, 1, 1, 0, 0);
}

int card_surrakar_spellblade(int player, int card, event_t event){

	/* Surrakar Spellblade	|1|U|U
	 * Creature - Surrakar 2/1
	 * Whenever you cast an instant or sorcery spell, you may put a charge counter on ~.
	 * Whenever ~ deals combat damage to a player, you may draw X cards, where X is the number of charge counters on it. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		add_counters(player, card, player, card, COUNTER_CHARGE, 1);
	}

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		draw_cards(player, count_counters(player, card, COUNTER_CHARGE) * amount);
	}

	return 0;
}

int card_training_grounds(int player, int card, event_t event)
{
  /* CARD_ID_TRAINING_GROUNDS	11065
   * Training Grounds	|U
   * Enchantment
   * Activated abilities of creatures you control cost up to |2 less to activate.
   * This effect can't reduce the amount of mana an ability costs to activate to less than one mana. */

  int result;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 2);
    set_cost_mod_for_activated_abilities(player, -1, 2, &this_test);
    leaves_play(player, card, EVENT_RESOLVE_SPELL, 0, 0);
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if (affected_card_controller == player)
    {
      result = is_what(player, affected_card, 2);
      if (result != 0)
      {
        set_cost_mod_for_activated_abilities(affected_card_controller, affected_card, 2, NULL);
      }
    }
    leaves_play(player, card, EVENT_CAST_SPELL, 0, 0);
  }
  else
  {
    leaves_play(player, card, event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      default_test_definition(&this_test, 2);
      remove_cost_mod_for_activated_abilities(player, -1, 2, &this_test);
    }
  }
  return global_enchantment(player, card, event);
}

int card_unified_will(int player, int card, event_t event){
  /* Unified Will |1|U
   * Instant
   * Counter target spell if you control more creatures than that spell's controller. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( count_permanents_by_type(player, TYPE_CREATURE) <= count_permanents_by_type(1-player, TYPE_CREATURE) ){
			ai_modifier -= 96;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( count_permanents_by_type(player, TYPE_CREATURE) > count_permanents_by_type(1-player, TYPE_CREATURE) ){
			return card_counterspell(player, card, event);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return card_counterspell(player, card, event);

}

int card_venerated_teacher(int player, int card, event_t event)
{
  /* Venerated Teacher	|2|U
   * Creature - Human Wizard 2/2
   * When ~ enters the battlefield, put two level counters on each creature you control with level up. */

  card_instance_t *instance;
  int c;
  int leveler_priority;
  if (event != EVENT_CAST_SPELL)
  {
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event != EVENT_ETB)
    {
      return 0;
    }
    if (active_cards_count[player] < 1)
    {
      return 0;
    }
    c = 0;
    do
    {
      instance = in_play(player, c);
      if (((instance != NULL) && (is_what(player, c, 2) != 0)) &&
          (get_counter_type_by_id((unsigned int)cards_data[instance->internal_card_id].id) == 0x40))
      {
        add_counters(player, card, player, c, COUNTER_LEVEL, 2);
      }
      c = c + 1;
    } while (c < active_cards_count[player]);
    return 0;
  }
  if ((card == affected_card) && (player == affected_card_controller))
  {
    c = 0;
    leveler_priority = 0;
    if (0 < active_cards_count[player])
    {
      do
      {
        instance = in_play(player, c);
        if (((instance != NULL) && (is_what(player, c, 2) != 0)) &&
            (get_counter_type_by_id((unsigned int)cards_data[instance->internal_card_id].id) ==
             0x40))
        {
          leveler_priority = leveler_priority + 0xc;
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
      if (leveler_priority != 0)
      {
        ai_modifier = leveler_priority + ai_modifier;
        etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
        return 0;
      }
    }
    ai_modifier = ai_modifier + -0x60;
  }
  etb_ability(player, card, EVENT_CAST_SPELL, RESOLVE_TRIGGER_MANDATORY);
  return 0;
}

/********
* Black *
********/
static void destroy_self_at_end_of_combat_if_vs_le_1_power(intptr_t player, intptr_t card, int t_player, int t_card)
{
  if (get_power(t_player, t_card) <= 1)
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, player, card);
}

int card_arrogant_bloodlord(int player, int card, event_t event)
{
  /* Arrogant Bloodlord	|1|B|B
   * Creature - Vampire Knight 4/4
   * Whenever this creature blocks or becomes blocked by a creature with power 1 or less, destroy this creature at end of combat. */

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
	instance->destroys_if_blocked |= DIFB_ASK_CARD;

  if (event == EVENT_CHECK_DESTROY_IF_BLOCKED && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)
	  && get_power(attacking_card_controller, attacking_card) <= 1)
	event_result |= 2;

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG))
	{
	  if (player == current_turn && (instance->state & STATE_ATTACKING))
		for_each_creature_blocking_me(player, card, destroy_self_at_end_of_combat_if_vs_le_1_power, player, card);

	  if (player == 1-current_turn && (instance->state & STATE_BLOCKING))
		for_each_creature_blocked_by_me(player, card, destroy_self_at_end_of_combat_if_vs_le_1_power, player, card);
	}

  return 0;
}

/* Bala Ged Scorpion	|3|B
 * Creature - Scorpion 2/3
 * When ~ enters the battlefield, you may destroy target creature with power 1 or less. */

int card_baneful_omen(int player, int card, event_t event){
/* CARD_ID_BANEFUL_OMEN	10875
Baneful Omen	|4|B|B|B
Enchantment
At the beginning of your end step, you may reveal the top card of your library. If you do, each opponent loses life equal to that card's converted mana cost.*/

	if( eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) ){
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			show_deck(0, deck, 1, "Baneful Omen reveals...", 0, gs_done_008b40e0 );
			show_deck(1, deck, 1, "Baneful Omen reveals...", 0, gs_done_008b40e0 );
			lose_life(1-player, get_cmc_by_internal_id(deck[0]));
		}
	}

	return global_enchantment(player, card, event);
}

/* Bloodrite Invoker	|2|B
 * Creature - Vampire Shaman 3/1
 * |8: Target player loses 3 life and you gain 3 life. */

/* Bloodthrone Vampire	|1|B => onslaught.c:Nantuko Husk
 * Creature - Vampire 1/1
 * Sacrifice a creature: ~ gets +2/+2 until end of turn. */

/* Cadaver Imp	|1|B|B => portal_1_2_3k.c:Gravedigger
 * Creature - Imp 1/1
 * Flying
 * When ~ enters the battlefield, you may return target creature card from your graveyard to your hand. */

int card_consume_the_meek(int player, int card, event_t event){
/* CARD_ID_CONSUME_THE_MEEK	10891
Consume the Meek	|3|B|B
Instant
Destroy each creature with converted mana cost 3 or less. They can't be regenerated. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.cmc = 4;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_BURY);
		kill_card(player, card, KILL_BURY);
	}

	return basic_spell(player, card, event);
}

int card_consuming_vapors(int player, int card, event_t event){
/* CARD_ID_CONSUMING_VAPORS	10892
Consuming Vapors	|3|B
Sorcery
Target player sacrifices a creature. You gain life equal to that creature's toughness.
Rebound (If you cast this spell from your hand, exile it as it resolves.
At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = ANYBODY;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			if( ! can_cause_sacrifice(player, instance->targets[0].player) ){
				ai_modifier -= 96;
			}
			if( ! count_subtype(instance->targets[0].player, TYPE_CREATURE, -1) ){
				ai_modifier -= 96;
			}
			if( life[player] < 6 ){
				ai_modifier += (instance->targets[0].player == player ? (count_subtype(player, TYPE_CREATURE, -1) * 12) : -48);
			}
			else{
				ai_modifier += (instance->targets[0].player == 1-player ? 48 : -96);
			}
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, instance->targets[0].player,
									SAC_CAUSED | SAC_NO_CANCEL | SAC_JUST_MARK | SAC_RETURN_CHOICE, &test);
			if( sac ){
				int amount = get_toughness(BYTE2(sac), BYTE3(sac));
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				gain_life(player, amount);
			}
			rebound(player, card);
			return 0;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

/* Contaminated Ground	|1|B => gatecrash.c
 * Enchantment - Aura
 * Enchant land
 * Enchanted land is |Ha Swamp.
 * Whenever enchanted land becomes tapped, its controller loses 2 life. */

/* Corpsehatch	|3|B|B
 * Sorcery
 * Destroy target non|Sblack creature. Put two 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to
 * your mana pool." */

int card_curse_of_wizardry(int player, int card, event_t event){
/* CARD_ID_CURSE_OF_WIZARDRY	10897
Curse of Wizardry	|2|B|B
Enchantment
As ~ enters the battlefield, choose a color.
Whenever a player casts a spell of the chosen color, that player loses 1 life. */

	if (event == EVENT_RESOLVE_SPELL){
		int color = choose_a_color_and_show_legacy(player, card, 1-player, -1);
		get_card_instance(player, card)->info_slot = 1u << (color & 31);
	}
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_card_instance(player, card)->info_slot;
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){ lose_life(trigger_cause_controller, 1); }
	return global_enchantment(player, card, event);
}

/* Death Cultist	|B
 * Creature - Human Wizard 1/1
 * Sacrifice ~: Target player loses 1 life and you gain 1 life. */

/* Demonic Appetite	|B
 * Enchantment - Aura
 * Enchant creature you control
 * Enchanted creature gets +3/+3.
 * At the beginning of your upkeep, sacrifice a creature. */

/* ManalinkEh 2022: 0x24b35e0; original cards/rise_of_the_eldrazi.c:2678. */
int card_drana_kalastria_bloodchief(int player, int card, event_t event)
{
  /* Drana, Kalastria Bloodchief	|3|B|B
   * Legendary Creature - Vampire Shaman 4/4
   * Flying
   * |X|B|B: Target creature gets -0/-X until end of turn and Drana gets +X/+0 until end of turn. */

  card_instance_t *instance;
  int chosen_x;
  int target_valid;
  target_definition_t td;

  check_legend_rule(player, card, event);
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    instance->info_slot = x_value;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    instance->info_slot = x_value;

    if (((event == EVENT_RESOLVE_ACTIVATION) &&
         (target_valid = valid_target(&td), chosen_x = x_value, target_valid != 0)) &&
        (0 < x_value))
    {
      pump_until_eot_merge_previous_alt(player, card, instance->parent_controller,
                                        instance->parent_card, x_value, 0, 2);
      pump_until_eot_merge_previous_alt(player, card, instance->targets[0].player,
                                        instance->targets[0].card, 0, -chosen_x, 1);
    }
  }
  return generic_activated_ability(player, card, event, 4, -1, 2, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}

/* Dread Drone	|4|B
 * Creature - Eldrazi Drone 4/1
 * When ~ enters the battlefield, put two 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to your
 * mana pool." */

/* Escaped Null	|3|B
 * Creature - Zombie Berserker 1/2
 * Lifelink
 * Whenever ~ blocks or becomes blocked, it gets +5/+0 until end of turn. */

/* Essence Feed	|5|B
 * Sorcery
 * Target player loses 3 life. You gain 3 life and put three 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this
 * creature: Add |1 to your mana pool." */

/* Gloomhunter	|2|B (vanilla)
 * Creature - Bat 2/1
 * Flying */

int card_guul_draz_assassin(int player, int card, event_t event){

	/* Guul Draz Assassin	|B
	 * Creature - Vampire Assassin 1/1
	 * Level up |1|B
	 * LEVEL 2-3	2/2
	 * |B, |T: Target creature gets -2/-2 until end of turn.
	 * LEVEL 4+	4/4
	 * |B, |T: Target creature gets -4/-4 until end of turn. */

	level_up_effects(player, card, event, 1, 1, 2, 2, 2, 0, 0, 4, 4, 4, 0, 0);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_LEVEL_UP = 1,
		CHOICE_WEAKEN
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_B(1), 0, &td, NULL) &&
			count_counters(player, card, COUNTER_LEVEL) >= 2 )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XB(1, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
            0,
            has_mana_multi(player, MANACOST_XB(1, 1)) && can_sorcery_be_played(player, event),
            count_counters(player, card, COUNTER_LEVEL) >= 2 &&
            generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_B(1), 0, &td, NULL) ? 1 : 0
        };
		char msg[100];
		if( count_counters(player, card, COUNTER_LEVEL) < 4 ){
			scnprintf(msg, 100, "Give -2/-2");
		}
		else{
			scnprintf(msg, 100, "Give -4/-4");
		}
		
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Level up", abils[1], 5,
							msg, abils[2], 15);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_LEVEL_UP ){
			int mod = count_counters(player, card, COUNTER_LEVEL) < 2 ? 24 : 12;
			ai_modifier += (current_phase > PHASE_DECLARE_BLOCKERS ? (mod * 2) : mod);
			return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XB(1, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_WEAKEN ){
			instance->targets[1].player = count_counters_no_parent(player, card, COUNTER_LEVEL);
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_B(1), 0, &td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LEVEL_UP ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_LEVEL, 1);
		}
		if( instance->info_slot == CHOICE_WEAKEN ){
			if( valid_target(&td) ){
				int pump = instance->targets[1].player >= 4 ? -4 : -2;
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, pump, pump);
			}
		}
	}

	return 0;
}

int card_hellcarver_demon(int player, int card, event_t event){
  /* Hellcarver Demon |3|B|B|B
   * Creature � Demon 6/6
   * Flying
   * Whenever Hellcarver Demon deals combat damage to a player, sacrifice all other permanents you control and discard your hand.
   * Exile the top six cards of your library. You may cast any number of nonland cards exiled this way without paying their mana costs. */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE);
	if( result ){
		int qty = BYTE0(result) + BYTE1(result);
		while( qty ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_PERMANENT);
				this_test.not_me = 1;
				new_manipulate_all(player, card, player, &this_test, KILL_SACRIFICE);
				discard_all(player);
				int amount = MIN(6, count_deck(player));
				if( amount > 0 ){
					int *deck = deck_ptr[player];
					show_deck( HUMAN, deck, amount, "Cards exiled by Hellcarver Demon", 0, gs_done_008b40e0 );
					int count = amount-1;
					int demon[amount];
					int demon_count = 0;
					while( count > -1 ){
							if( ! is_what(-1, deck[count], TYPE_LAND) && can_legally_play_iid(player, deck[count]) ){
								demon[demon_count] = deck[count];
								demon_count++;
							}
							rfg_card_in_deck(player, count);
							count--;
					}
					while( demon_count > 0 ){
							test_definition_t this_test2;
							new_default_test_definition(&this_test2, TYPE_ANY, "Select a card to play for free");
							int selected = select_card_from_zone(player, player, demon, demon_count, 0, AI_MAX_CMC, -1, &this_test2);
							if( selected != -1 ){
								int csvid = cards_data[demon[selected]].id;
								int k;
								for(k=selected; k<demon_count; k++){
									demon[k] = demon[k+1];
								}
								demon_count--;
								play_card_in_exile_for_free(player, player, csvid);
							}
							else{
								break;
							}
					}
				}
				qty--;
		}
	}

	return 0;
}

/* Induce Despair	|2|B
 * Instant
 * As an additional cost to cast ~, reveal a creature card from your hand.
 * Target creature gets -X/-X until end of turn, where X is the revealed card's converted mana cost. */

int card_inquisition_of_kozilek(int player, int card, event_t event){
/* CARD_ID_INQUISITION_OF_KOZILEK	10958
Inquisition of Kozilek	|B
Sorcery
Target player reveals his or her hand. You choose a nonland card from it with converted mana cost 3 or less. That player discards that card. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
		if( spell_fizzled != 1 ){
			ai_modifier += (hand_count[instance->targets[0].player]*8);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			ec_definition_t ec;
			default_ec_definition(instance->targets[0].player, player, &ec);

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a nonland card with CMC 3 or less.");
			this_test.type_flag = DOESNT_MATCH;
			this_test.cmc = 4;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			new_effect_coercion(&ec, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

/* Last Kiss	|2|B (vicious hunger)
 * Instant
 * ~ deals 2 damage to target creature and you gain 2 life. */

int card_mortician_beetle(int player, int card, event_t event){
	/* Mortician Beetle	|B
	 * Creature - Insect 1/1
	 * Whenever a player sacrifices a creature, you may put a +1/+1 counter on ~. */

	if (whenever_a_player_sacrifices_a_permanent(player, card, event, ANYBODY, TYPE_CREATURE, RESOLVE_TRIGGER_AI(player))){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	return 0;
}

/* Nighthaze	|B
 * Sorcery
 * Target creature gains |H2swampwalk until end of turn.
 * Draw a card. */

int card_nirkana_cutthroat(int player, int card, event_t event)
{
  /* Nirkana Cutthroat	|2|B
   * Creature - Vampire Warrior 3/2
   * Level up |2|B
   * LEVEL 1-2	4/3
   * Deathtouch
   * LEVEL 3+	5/4
   * First strike, deathtouch */

  return level_up(player, card, event, MANACOST_XB(2,1),
				     3,2,
				  1, 4,3, 0,                   SP_KEYWORD_DEATHTOUCH,
				  3, 5,4, KEYWORD_FIRST_STRIKE,SP_KEYWORD_DEATHTOUCH);
}

int card_nirkana_revenant(int player, int card, event_t event){
/* CARD_ID_NIRKANA_REVENANT	11004
Nirkana Revenant	|4|B|B
Creature - Vampire Shade 4/4
Whenever you tap |Ha Swamp for mana, add |B to your mana pool (in addition to the mana the land produces).
|B: ~ gets +1/+1 until end of turn. */

	produce_mana_when_subtype_is_tapped(player, event, TYPE_PERMANENT, SUBTYPE_SWAMP, COLOR_BLACK, 1);

	return generic_shade_merge_pt(player, card, event, 0, MANACOST_B(1), 1, 1);
}

int card_null_champion(int player, int card, event_t event)
{
  /* Null Champion	|1|B
   * Creature - Zombie Warrior 1/1
   * Level up |3
   * LEVEL 1-3	4/2
   * LEVEL 4+	7/3
   * |B: Regenerate ~. */

  if (IS_ACTIVATING(event) && (land_can_be_played & LCBP_REGENERATION) && count_counters_no_parent(player, card, COUNTER_LEVEL) >= 4)
	return regeneration(player, card, event, MANACOST_B(1));

  return level_up(player, card, event, MANACOST_X(3),
				     1,1,
				  1, 4,2, 0,0,
				  4, 7,3, KEYWORD_REGENERATION,0);
}

static int pawn_of_ulamog_effect(int player, int card, event_t event)
{

  // In case something is copying Pawn of Ulamog and then reverts to its original form.
  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_PAWN_OF_ULAMOG)
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
            // Will trigger also if Pawn of Ulamog is not a creature when it goes into the
            // graveyard;
            if (!is_what(p, c, TYPE_CREATURE))
            {
              count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
            }
            instance->damage_target_player = instance->damage_target_card = -1;
            remove_status(player, card, STATUS_INVISIBLE_FX);
            if (instance->targets[11].player < 0)
              instance->targets[11].player = 0;
            SET_BYTE1(instance->targets[11].player) = 1;
          }
        }
        if (!is_token(affected_card_controller, affected_card) || affect_me(player, card))
        {
          count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_ELDRAZI_SPAWN, &token);
    token.qty = BYTE0(result);
    generate_token(&token);
    kill_card(player, card, KILL_EXILE);
  }

  // If you decide to not get the Eldrazi Spawns...
  if (event == EVENT_CLEANUP && get_card_instance(player, card)->damage_target_player == -1)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_pawn_of_ulamog(int player, int card, event_t event){
  /* Pawn of Ulamog	|1|B|B
   * Creature - Vampire Shaman 2/2
   * Whenever this creature or another nontoken creature you control dies, you may create a 0/1 colorless Eldrazi Spawn creature token. It has "Sacrifice this token: Add |C." */

	/*
	  Pawn of Ulamog English
	  Creature � Vampire Shaman 2/2, 1BB (3)
	  Whenever Pawn of Ulamog or another nontoken creature you control dies, you may put a 0/1 colorless Eldrazi Spawn creature token
	  onto the battlefield. It has "Sacrifice this creature: Add {C} to your mana pool."
	*/

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (intptr_t)pawn_of_ulamog_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &pawn_of_ulamog_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY && in_play(affected_card_controller, affected_card) ){
		if( ! is_token(affected_card_controller, affected_card) ){
			count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( amount ){

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELDRAZI_SPAWN, &token);
			token.qty = amount;
			generate_token(&token);

		}
	}

	return 0;
}

/* Perish the Thought	|2|B
 * Sorcery
 * Target opponent reveals his or her hand. You choose a card from it. That player shuffles that card into his or her library. */

int card_pestilence_demon(int player, int card, event_t event){
  /* Pestilence Demon	|5|B|B|B
   * Creature - Demon 7/6
   * Flying
   * |B: This creature deals 1 damage to each creature and each player. */

	if(event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		new_damage_all(instance->parent_controller, instance->parent_card, ANYBODY, 1, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL);
}

int card_repay_in_kind(int player, int card, event_t event){
/* Repay in Kind	|5|B|B	0x200f570
 * Sorcery
 * Each player's life total becomes the lowest life total among all players. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < life[1-player] ){
			ai_modifier += ((life[1-player] - life[player]) * 4);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int amount = life[player] < life[1-player] ? life[player] : life[1-player];
		APNAP(p, { set_life_total(p, amount); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* Shrivel	|1|B => 0x40ff70:Nausea
 * Sorcery
 * All creatures get -1/-1 until end of turn. */

/* Skeletal Wurm	|7|B (drudge skeletons)
 * Creature - Skeleton Wurm 7/6
 * |B: Regenerate ~. */

/* Suffer the Past	|X|B
 * Instant
 * Exile X target cards from target player's graveyard. For each card exiled this way, that player loses 1 life and you gain 1 life. */

int card_thought_gorger(int player, int card, event_t event)
{
  /* Thought Gorger	|2|B|B
   * Creature - Horror 2/2
   * Trample
   * When this creature enters, put a +1/+1 counter on it for each card in your hand. If you do, discard your hand.
   * When this creature leaves the battlefield, draw a card for each +1/+1 counter on it. */

  int amount;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    add_counters(player, card, player, card, COUNTER_P1_P1, hand_count[player]);
    discard_all(player);
    leaves_play(player, card, EVENT_ETB, 1, 1);
    return 0;
  }
  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    return 0;
  }
  amount = count_counters(player, card, COUNTER_P1_P1);
  draw_cards(player, amount);
  return 0;
}

/* Vendetta	|B => mercadian_masques.c
 * Instant
 * Destroy target non|Sblack creature. It can't be regenerated. You lose life equal to that creature's toughness. */

int card_virulent_swipe(int player, int card, event_t event){
  /* Virulent Swipe	|B
   * Instant
   * Target creature gets +2/+0 and gains deathtouch until end of turn.
   * Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		if( spell_fizzled != 1 ){
			if( current_turn == player ){
				ai_modifier += (is_attacking(instance->targets[0].player, instance->targets[0].card) &&
								! is_unblocked(instance->targets[0].player, instance->targets[0].card) ? 48 : -48);
			}
			if( current_turn == player ){
				ai_modifier += (check_state(instance->targets[0].player, instance->targets[0].card, STATE_BLOCKING) ? 48 : -48);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0, 0, SP_KEYWORD_DEATHTOUCH);
			rebound(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

/* Zof Shade	|3|B (generic_shade)
 * Creature - Shade 2/2
 * |2|B: ~ gets +2/+2 until end of turn. */

int card_zulaport_enforcer(int player, int card, event_t event)
{
  /* Zulaport Enforcer	|B
   * Creature - Human Warrior 1/1
   * Level up |4
   * LEVEL 1-2	3/3
   * LEVEL 3+	5/5
   * ~ can't be blocked except by |Sblack creatures. */

  // Need to value counters slightly more than usual, so the AI will be willing to pay |4 for the second one.
  if (event == EVENT_SHOULD_AI_PLAY && player == AI)
	{
	  int counters = count_counters(player, card, COUNTER_LEVEL);
	  counters = MIN(4, counters);
	  ai_modifier += counters;
	}

  if (event == EVENT_BLOCK_LEGALITY
	  && attacking_card_controller == player && attacking_card == card
	  && count_counters(player, card, COUNTER_LEVEL) >= 3
	  && !(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK)))
	event_result = 1;

  return level_up(player, card, event, MANACOST_X(4),
				     1,1,
				  1, 3,3, 0,0,
				  3, 5,5, 0,0);
}

/* Akoum Boulderfoot	|4|R|R (sparkmage apprentice)
 * Creature - Giant Warrior 4/5
 * When ~ enters the battlefield, it deals 1 damage to target creature or player. */

/* Battle Rampart	|2|R
 * Creature - Wall 1/3
 * Defender
 * |T: Target creature gains haste until end of turn. */

/* Battle-Rattle Shaman	|3|R
 * Creature - Goblin Shaman 2/2
 * At the beginning of combat on your turn, you may have target creature get +2/+0 until end of turn. */

/******
* Red *
******/

int card_brimstone_mage(int player, int card, event_t event)
{
  /* Brimstone Mage	|2|R	0x200bfd8
   * Creature - Human Shaman 2/2
   * Level up |3|R
   * LEVEL 1-2
   * 2/3
   * |T: ~ deals 1 damage to any target.
   * LEVEL 3+
   * 2/4
   * |T: ~ deals 3 damage to any target. */

  enum
  {
    CHOICE_LEVEL_UP = 1,
    CHOICE_DAMAGE = 2
  };

  bool level_up_default;
  int level_count_or_result;
  int ability_choice_or_result;
  const char *dialog_mode;
  bool activation_or_manual;
  uint32_t level_up_priority;
  const char *damage_option;
  unsigned int level_up_available;
  target_definition_t td;
  char damage_prompt[100];
  level_up_effects(player, card, event, 2, 2, 1, 2, 3, KEYWORD_0, SP_KEYWORD_0, 3, 2, 4, KEYWORD_0,
      SP_KEYWORD_0);
  level_up_default = (uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2;
  activation_or_manual = level_up_default || event == EVENT_ACTIVATE;
  if (level_up_default || event == EVENT_ACTIVATE)
  {
    level_count_or_result = count_counters_no_parent(player, card, COUNTER_LEVEL);
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (ai_is_speculating == 1)
    {
      damage_prompt[0] = '\0';
    }
    else
    {
      scnprintf(damage_prompt, 100, "Deal %d damage",
                (unsigned int)(2 < level_count_or_result) * 2 + 1);
    }
    damage_option = damage_prompt;
    bool default_level_path = true;
    if (level_count_or_result < 3)
    {
      level_up_default = false;
      level_up_priority = 3;
    }
    else
    {
      level_up_priority = 0xffffffff;
      if (ai_is_speculating != 1 && (player != 1 || ((unsigned char)trace_mode & 2) != 0))
      {
        ability_choice_or_result = duh_mode(player);
        activation_or_manual = ability_choice_or_result == 0;
        level_up_default = !activation_or_manual;
      }
      else
        default_level_path = false;
    }
    ability_choice_or_result = can_sorcery_be_played(player, event);
    if (default_level_path)
    {
      level_up_available =
          (unsigned int)(unsigned char)(ability_choice_or_result != 0 & activation_or_manual);
      if (level_count_or_result != 0 && !level_up_default)
        dialog_mode = DLG_NO_OP;
      else
        dialog_mode = DLG_AUTOCHOOSE_IF_1;
    }
    else
    {
      level_up_available = (unsigned int)(ability_choice_or_result != 0);
      dialog_mode = DLG_NO_OP;
    }
    ability_choice_or_result =
        dialog_impl(player, card, event, DLG_RANDOM, dialog_mode, "Level up", level_up_available,
                    level_up_priority, DLG_MANA6(3, 0, 0, 0, 1, 0), damage_option,
                    (unsigned int)(0 < level_count_or_result), 1, DLG_TAP, DLG_TARGET(&td,
                    "TARGET_ANY"), 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return ability_choice_or_result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (ability_choice_or_result == CHOICE_LEVEL_UP)
      {
        add_counters(player, card, player, card, COUNTER_LEVEL, 1);
      }
      else if (ability_choice_or_result == CHOICE_DAMAGE)
      {
        damage_target0(player, card, (unsigned int)(2 < level_count_or_result) * 2 + 1);
      }
    }
  }
  if (event == EVENT_CHECK_PUMP)
  {
    if (is_tapped(player, card) != 0)
    {
      return 0;
    }
    if (is_animated_and_sick(player, card) != 0)
    {
      return 0;
    }
    if (can_use_activated_abilities(player, card) == 0)
    {
      return 0;
    }
    if (has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) == 0)
    {
      return 0;
    }
    level_count_or_result = count_counters(player, card, COUNTER_LEVEL);
    if (2 < level_count_or_result)
    {
      pumpable_toughness[1 - player] = pumpable_toughness[1 - player] + -3;
      return 0;
    }
    if (level_count_or_result < 1)
    {
      return 0;
    }
    pumpable_toughness[1 - player] = pumpable_toughness[1 - player] + -1;
    return 0;
  }
  if ((event == EVENT_SHOULD_AI_PLAY) && (player == 1))
  {
    ability_choice_or_result = count_counters(1, card, COUNTER_LEVEL);
    level_count_or_result = ability_choice_or_result;
    if (3 < ability_choice_or_result)
    {
      level_count_or_result = 3;
    }
    ai_modifier = ai_modifier + level_count_or_result;
    if (((0 < ability_choice_or_result) && (current_turn == 1)) &&
        ((level_count_or_result = is_tapped(1, card),
          level_count_or_result == 0 &&
              (((level_count_or_result = is_animated_and_sick(1, card),
                 level_count_or_result == 0 &&
                     (level_count_or_result = can_use_activated_abilities(1, card),
                      level_count_or_result != 0)) &&
                (level_count_or_result = has_mana_for_activated_ability(1, card, 0, 0, 0, 0, 0, 0),
                 level_count_or_result != 0))))))
    {
      ai_modifier = ai_modifier + (unsigned int)((2 < ability_choice_or_result) ? 0 : -48) + 0x48;
    }
  }
  else if (((event == EVENT_ATTACK_RATING) &&
            (((card == affected_card && (player == affected_card_controller)) &&
              (level_count_or_result = can_use_activated_abilities(player, card),
               level_count_or_result != 0)))) &&
           ((level_count_or_result = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0),
             level_count_or_result != 0 &&
                 (level_count_or_result = count_counters(player, card, COUNTER_LEVEL),
                  0 < level_count_or_result))))
  {
    ai_defensive_modifier = ai_defensive_modifier + 0x30;
    return 0;
  }
  return 0;
}

/* Brood Birthing	|1|R
 * Sorcery
 * If you control an Eldrazi Spawn, put three 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to
 * your mana pool." Otherwise, put one of those tokens onto the battlefield. */

int card_conquering_manticore(int player, int card, event_t event){
  /* Conquering Manticore	|4|R|R
   * Creature - Manticore 5/5
   * Flying
   * When this creature enters, gain control of target creature an opponent controls until end of turn. Untap that creature. It gains haste until end of turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select target creature your opponent controls.", 0, GS_LITERAL_PROMPT) ){
			ai_modifier_for_act_of_treason(player, card);
			effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_devastating_summons(int player, int card, event_t event){
  /* Devastating Summons |R
   * Sorcery
   * As an additional cost to cast Devastating Summons, sacrifice X lands.
   * Put two X/X red Elemental creature tokens onto the battlefield. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return new_can_sacrifice_as_cost(player, card, &test);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		basic_spell(player, card, event);
		int sacced[151];
		int sc = 0;
		while( new_can_sacrifice_as_cost(player, card, &test) ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if( !sac ){
					break;
				}
				sacced[sc] = BYTE3(sac);
				sc++;
				if( IS_AI(player) && (sc == 4 || sc >= (count_subtype(player, TYPE_LAND, -1) / 2)) ){
					break;
				}
		}
		int i;
		for(i=0; i<sc; i++){
			kill_card(player, sacced[i], KILL_SACRIFICE);
		}
		instance->info_slot = sc;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.qty = 2;
		token.pow = token.tou = instance->info_slot;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_disaster_radius(int player, int card, event_t event){
  /* Disaster Radius	|5|R|R
   * Sorcery
   * As an additional cost to cast this spell, reveal a creature card from your hand.
   * ~ deals X damage to each creature your opponents control, where X is the revealed card's mana value. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
	this_test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && basic_spell(player, card, event) ){
		return check_battlefield_for_special_card(player, card, player, 0, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
		if( selected != -1 ){
			reveal_card(player, card, player, selected);
			instance->info_slot = get_cmc(player, selected);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, 1-player, instance->info_slot, NDA_ALL_CREATURES, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* Emrakul's Hatcher	|4|R
 * Creature - Eldrazi Drone 3/3
 * When ~ enters the battlefield, put three 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to your
 * mana pool." */

int card_explosive_revelation(int player, int card, event_t event){
	/* Explosive Revelation	|3|R|R	0x20085b3
	 * Sorcery
	 * Choose any target. Reveal cards from the top of your library until you reveal a nonland card. ~ deals damage equal to that card's converted mana cost to that permanent or player. Put the nonland card into your hand and the rest on the bottom of your library in any order. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			default_test_definition(&test, TYPE_LAND);
			test.type_flag = DOESNT_MATCH;
			int result = reveal_card_until_you_reveal_test(player, player, &test, RCUYRT_ADD_TEST_TO_HAND |
															RCUYRT_PUT_REST_ON_BOTTOM | RCUYRT_RETURN_ADDED_CARD);
			if( result > -1 ){
				damage_target0(player, card, get_cmc(player, result));
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

/* Fissure Vent	|3|R|R
 * Sorcery
 * Choose one or both - Destroy target artifact; and/or destroy target nonbasic land. */

int card_flame_slash(int player, int card, event_t event){
  /* Flame Slash	|R
   * Sorcery
   * ~ deals 4 damage to target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 4, player, card);
		}
		kill_card(player, card, KILL_DESTROY );
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_forked_bolt(int player, int card, event_t event){
	/* Forked Bolt	|R	0x20085b8
	 * Sorcery
	 * ~ deals 2 damage divided as you choose among one or two targets. */
	/* Twin Bolt	|1|R	0x20085b8
	 * Instant
	 * ~ deals 2 damage divided as you choose among one or two targets. */
	/* Chandra's Pyrohelix	|1|R	0x20085b8
	 * Instant
	 * ~ deals 2 damage divided as you choose among one or two targets. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td, "TARGET_ANY", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_goblin_arsonist(int player, int card, event_t event){
	/* Goblin Arsonist	|R	0x2004099
	 * Creature - Goblin Shaman 1/1
	 * When ~ dies, you may have it deal 1 damage to any target. */
	if( this_dies_trigger(player, card, event, 2) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		if( can_target(&td) && pick_target(&td, "TARGET_ANY") ){
			damage_target0(player, card, 1);
		}
	}

	return 0;
}

/* Goblin Tunneler	|1|R => 0x4c73d0:Dwarven Warriors
 * Creature - Goblin Rogue 1/1
 * |T: Target creature with power 2 or less is unblockable this turn. */

/* Grotag Siege-Runner	|1|R
 * Creature - Goblin Rogue 2/1
 * |R, Sacrifice ~: Destroy target creature with defender. ~ deals 2 damage to that creature's controller. */

/* Heat Ray	|X|R
 * Instant
 * ~ deals X damage to target creature. */

int card_hellion_eruption(int player, int card, event_t event){
  /* Hellion Eruption	|5|R
   * Sorcery
   * Sacrifice all creatures you control, then create that many 4/4 red Hellion creature tokens. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int mod = 0;
		CYCLE_ALL_CARDS(p, c, 	{
									if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
										mod += get_power(p, c) >= 4 ? -8 : 8;
									}
								}
		);
		ai_modifier += mod;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		int amount = new_manipulate_all(player, card, player, &this_test, KILL_SACRIFICE);

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_HELLION, &token);
		token.qty = amount;
		token.pow = token.tou = 4;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_kargan_dragonlord(int player, int card, event_t event)
{
  /* Kargan Dragonlord	|R|R
   * Creature - Human Warrior 2/2
   * Level up |R
   * LEVEL 4-7	4/4
   * Flying
   * LEVEL 8+	8/8
   * Flying, trample
   * |R: ~ gets +1/+0 until end of turn. */

  if (IS_SHADE_EVENT(event) && count_counters_no_parent(player, card, COUNTER_LEVEL) >= 8)
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1,0);

  return level_up(player, card, event, MANACOST_R(1),
				     2,2,
				  4, 4,4, KEYWORD_FLYING,0,
				  8, 8,8, KEYWORD_FLYING|KEYWORD_TRAMPLE,0);
}

int card_kiln_fiend(int player, int card, event_t event){
	/* Kiln Fiend	|1|R
	 * Creature - Elemental Beast 1/2
	 * Whenever you cast an instant or sorcery spell, ~ gets +3/+0 until end of turn. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		pump_until_eot_merge_previous(player, card, player, card, 3, 0);
	}
	return 0;
}

/* Lagac Lizard	|3|R (vanilla)
 * Creature - Lizard 3/3 */

/* Lavafume Invoker	|2|R
 * Creature - Goblin Shaman 2/2
 * |8: Creatures you control get +3/+0 until end of turn. */

int card_lord_of_shatterskull_pass(int player, int card, event_t event)
{
  /* Lord of Shatterskull Pass	|3|R
   * Creature - Minotaur Shaman 3/3
   * Level up |1|R
   * LEVEL 1-5	6/6
   * LEVEL 6+	6/6
   * Whenever ~ attacks, it deals 6 damage to each creature defending player controls. */

  if (count_counters(player, card, COUNTER_LEVEL) >= 6){
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  }
  if (event == EVENT_DAT_ABILITY){
	new_damage_all(player, card, 1-current_turn, 6, NDA_ALL_CREATURES, NULL);
  }

  return level_up(player, card, event, MANACOST_XR(1,1),
				     3,3,
				  1, 6,6, 0,0,
				  6, 6,6, 0,0);
}

/* ManalinkEh 2022: 0x24b6540; original cards/rise_of_the_eldrazi.c:3728. */
int card_lust_for_war(int player, int card, event_t event)
{
  /* Lust for War	|2|R
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature becomes tapped, ~ deals 3 damage to that creature's controller.
   * Enchanted creature attacks each turn if able. */

  card_instance_t *instance;
  int result;
  int argument1;

  instance = get_card_instance(player, card);
  if ((-1 < instance->damage_target_player) &&
      (result = is_humiliated(player, card, AL_STATIC), result == 0))
  {

    instance = get_card_instance(player, card);
    result = instance->damage_target_card;
    argument1 = (int)instance->damage_target_player;
    if ((event == EVENT_TAP_CARD) &&
        ((result == affected_card && (argument1 == affected_card_controller))))
    {
      damage_player(argument1, 3, player, card);
      argument1 = (int)instance->damage_target_player;
      result = instance->damage_target_card;
    }
    attack_if_able(argument1, result, event);
  }
  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return disabling_aura(player, card, event);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return disabling_aura(player, card, event);
    if ((((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
         (((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
           (event != EVENT_CHANGE_TYPE)))) &&
        (((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)) &&
          (event != EVENT_STATIC_EFFECTS))))
    {
      return 0;
    }
  }
  return disabling_aura(player, card, event);
}

int card_magmaw(int player, int card, event_t event){
	/* Magmaw	|3|R|R	0x20085fe
	 * Creature - Elemental 4/4
	 * |1, Sacrifice a nonland permanent: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.type_flag = DOESNT_MATCH;

		if( new_can_sacrifice_as_cost(player, card, &test) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if (charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select a nonland permanent to sacrifice.");
			test.type_flag = DOESNT_MATCH;

			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_RETURN_CHOICE, &test);
			if (sac){
				instance->number_of_targets = 0;
				if( pick_target(&td, "TARGET_ANY") ){
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				}
				else{
					state_untargettable(BYTE2(sac), BYTE3(sac), 0);
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return 0;
}

/* Ogre Sentry	|1|R (vanilla)
 * Creature - Ogre Warrior 3/3
 * Defender */

int card_rage_nimbus(int player, int card, event_t event){
  /* Rage Nimbus	|2|R
   * Creature - Elemental 5/3
   * Defender, flying
   * |1|R: Target creature attacks this turn if able. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		if( current_turn == player || current_phase > PHASE_DECLARE_ATTACKERS ){
			ai_modifier -= 96;
		}
		generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_CREATURE");
		if( spell_fizzled != 1 ){
			if( check_for_special_ability(instance->targets[0].player, instance->targets[0].card, SP_KEYWORD_MUST_ATTACK) ){
				ai_modifier -= 48;
			}
			int pow = get_power(instance->targets[0].player, instance->targets[0].card);
			ai_modifier += ( pow < 3 ? 48 : -48);
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_MUST_ATTACK);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0, &td, "TARGET_CREATURE");
}

int card_raid_bombardment(int player, int card, event_t event)
{
	/* Raid Bombardment	|2|R	0x200863f
	 * Enchantment
	 * Whenever a creature you control with power 2 or less attacks, ~ deals 1 damage to the player or planeswalker that creature is attacking. */

	if (event == EVENT_DECLARE_ATTACKERS){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.power = 3;
		test.power_flag = F5_POWER_LESSER_THAN_VALUE;
		store_attackers(player, card, event, DAT_TRACK, player, -1, &test);
	}
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)&instance->targets[2].player;
		for (int i = instance->info_slot - 1; i >= 0; --i){
			int attacking = planeswalker_its_attacking(player, attackers[i]);
			if (attacking != -2){ damage_creature(1-current_turn, attacking, 1, player, card); }
		}
	}
	return global_enchantment(player, card, event);
}

int card_rapacious_one(int player, int card, event_t event){
/* Rapacious One	|5|R
 * Creature - Eldrazi Drone 5/4
 * Trample
 * Whenever ~ deals combat damage to a player, put that many 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield.
 * 		They have "Sacrifice this creature: Add |1 to your mana pool." */

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT);
	if( result ){
		card_instance_t *instance = get_card_instance(player, card);
		int qty = BYTE1(instance->targets[16].player) + BYTE0(instance->targets[16].player);
		instance->targets[16].player = 0;
		generate_tokens_by_id(player, card, CARD_ID_ELDRAZI_SPAWN, qty);
	}

	return 0;
}

/* Soulsurge Elemental	|3|R
 * Creature - Elemental 100/1
 * First strike
 * ~'s power is equal to the number of creatures you control. */

/* Spawning Breath	|1|R
 * Instant
 * ~ deals 1 damage to target creature or player. Put a 0/1 colorless Eldrazi Spawn creature token onto the battlefield. It has "Sacrifice this creature: Add |1
 * to your mana pool." */

int card_splinter_twin(int player, int card, event_t event){
  /* Splinter Twin	|2|R|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has "|T: Create a token that's a copy of this creature, except it has haste. Exile that token at the beginning of the next end step." */

	if( ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
			return granted_generic_activated_ability(player, card, t_player, t_card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		token_generation_t token;
		copy_token_definition(instance->parent_controller, instance->parent_card, &token, t_player, t_card);
		token.special_code_on_generation = &haste_and_remove_token_at_end_of_next_turn;
		generate_token(&token);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;

	return targeted_aura_custom_prompt(player, card, event, &td, "Select target creature you control.");
}

int card_staggershock(int player, int card, event_t event){
	/* Staggershock	|2|R	0x200543a
	 * Instant
	 * ~ deals 2 damage to any target.
	 * Rebound */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
			rebound(player, card);
		}
		else{
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_surreal_memoir(int player, int card, event_t event){
  /* Surreal Memoir	|3|R
   * Sorcery
   * Return an instant card at random from your graveyard to your hand.
   * Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

	if( event == EVENT_CAN_CAST ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_INSTANT | TYPE_INTERRUPT, "Select a sorcery or instant card.");
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += 48;
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		tutor_random_permanent_from_grave(player, card, player, TUTOR_HAND, TYPE_INSTANT | TYPE_INTERRUPT, 1, REANIMATE_DEFAULT);
		rebound(player, card);
	}

	return 0;
}

int card_traitorous_instinct(int player, int card, event_t event){
  /* Traitorous Instinct	|3|R
   * Sorcery
   * Gain control of target creature until end of turn. Untap that creature. Until end of turn, it gets +2/+0 and gains haste. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( valid_target(&td) ){
			effect_act_of_treason_and_modify_pt_or_abilities(player, card, instance->targets[0].player, instance->targets[0].card,
															2, 0, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
		return 0;
	}

	return aot_like_spell(player, card, event, &td, "TARGET_CREATURE", 0);
}

int card_tuktuk_the_explorer(int player, int card, event_t event){
	/* Tuktuk the Explorer	|2|R
	 * Legendary Creature - Goblin 1/1
	 * Haste
	 * When ~ dies, put a legendary 5/5 colorless Goblin Golem artifact creature token named Tuktuk the Returned onto the battlefield. */

	check_legend_rule(player, card, event);

	haste(player, card, event);

	nice_creature_to_sacrifice(player, card);

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		generate_token_by_id(player, card, CARD_ID_TUKTUK_THE_RETURNED);
	}

	return 0;
}

int card_tuktuk_the_returned(int player, int card, event_t event){
  /* Tuktuk the Returned	""
   * Token Artifact Creature - Goblin Golem 5/5
   * ~ is legendary. */

	check_legend_rule(player, card, event);

	return 0;
}

int card_valakut_fireboar(int player, int card, event_t event)
{
  /* Valakut Fireboar	|4|R
   * Creature - Elemental Boar 1/7
   * Whenever this creature attacks, switch its power and toughness until end of turn. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    switch_power_and_toughness_until_eot(player, card, player, card);
  return 0;
}

/* Vent Sentinel	|3|R
 * Creature - Elemental 2/4
 * Defender
 * |1|R, |T: ~ deals damage to target player equal to the number of creatures with defender you control. */

static int waw_legacy(int player, int card, event_t event){
	if( end_of_combat_trigger(player, card, event, 2) ){
		relentless_assault_effect(player, card);
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_world_at_war(int player, int card, event_t event){
  /* World at War	|3|R|R
   * Sorcery
   * After the second main phase this turn, there's an additional combat phase followed by an additional main phase. At the beginning of that combat, untap all creatures that attacked this turn.
   * Rebound (If you cast this spell from your hand, exile it as it resolves. At the beginning of your next upkeep, you may cast this card from exile without paying its mana cost.) */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_ATTACKING | STATE_ATTACKED;
		extra_combat_effect(player, card, event, 1, &test, 1);
		rebound(player, card);
	}

   return basic_spell(player, card, event);
}

/* Wrap in Flames	|3|R
 * Sorcery
 * ~ deals 1 damage to each of up to three target creatures. Those creatures can't block this turn. */

/********
* Green *
********/
int card_ancient_stirrings(int player, int card, event_t event){
  /* Ancient Stirrings	|G
   * Sorcery
   * Look at the top five cards of your library. You may reveal a colorless card from among them and put it into your hand. Then put the rest on the bottom of your library in any order. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int total = get_total_available_lands(player);
		ai_modifier += (total <= 3 || total >= 6 ? 48 : 24);
	}

	if( event == EVENT_RESOLVE_SPELL){
		int amount = MIN(5, count_deck(player));
		if( amount ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a colorless card.");
			this_test.color = COLOR_TEST_ANY_COLORED;
			this_test.color_flag = DOESNT_MATCH;
			this_test.create_minideck = amount;
			this_test.no_shuffle = 1;
			if( IS_AI(player) ){
				int total = get_total_available_lands(player);
				if( total <= 3 ){
					this_test.type = TYPE_LAND;
				}
				if( total >= 6 ){
					this_test.type = TYPE_LAND;
					this_test.type_flag = DOESNT_MATCH;
				}
			}
			int selected = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			if( IS_AI(player) && selected < 0 ){
				this_test.type = TYPE_ANY;
				selected = new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			}
			if( selected > -1 ){
				reveal_card(player, card, player, selected);
				amount--;
			}
			if( amount > 0 ){
				put_top_x_on_bottom(player, player, amount);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_aura_gnarlid(int player, int card, event_t event)
{
  /* Aura Gnarlid	|2|G
   * Creature - Beast 2/2
   * Creatures with power less than ~'s power can't block it.
   * ~ gets +1/+1 for each Aura on the battlefield. */

	if (event == EVENT_BLOCK_LEGALITY
		&& ! is_humiliated(player, card, AL_STATIC)
		&& player == attacking_card_controller && card == attacking_card
		&& get_power(affected_card_controller, affected_card) < get_power(player, card))
		event_result = 1;

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) )
		event_result += count_subtype(ANYBODY, TYPE_ENCHANTMENT, SUBTYPE_AURA);

	return 0;
}

int card_awakening_zone(int player, int card, event_t event){
	/* Awakening Zone	|2|G
	 * Enchantment
	 * At the beginning of your upkeep, you may put a 0/1 colorless Eldrazi Spawn creature token onto the battlefield. It has "Sacrifice this creature: Add |1 to your mana pool." */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_ELDRAZI_SPAWN);
	}

	return global_enchantment(player, card, event);
}

int card_bear_umbra(int player, int card, event_t event)
{
  /* Bear Umbra	|2|G|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+2 and has "Whenever this creature attacks, untap all lands you control."
   * Umbra armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

  card_instance_t *instance;
  int result;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    store_attackers(player, card, event, 0, (int)instance->damage_target_player,
                    instance->damage_target_card, NULL);
    dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
    if (event == EVENT_DAT_ABILITY)
    {
      manipulate_all(player, card, player, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0, ACT_UNTAP);
    }
  }
  result = totem_armor(player, card, event, 2, 2, 0, 0);
  return result;
}

int card_beastbreaker_of_bala_ged(int player, int card, event_t event)
{
  /* Beastbreaker of Bala Ged	|1|G
   * Creature - Human Warrior 2/2
   * Level up |2|G
   * LEVEL 1-3	4/4
   * LEVEL 4+	6/6
   * Trample */

  return level_up(player, card, event, MANACOST_XG(2,1),
				     2,2,
				  1, 4,4, 0,0,
				  4, 6,6, KEYWORD_TRAMPLE,0);
}

int card_boar_umbra(int player, int card, event_t event){
/* CARD_ID_BOAR_UMBRA	10881
Boar Umbra	|2|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +3/+3.
Totem armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

	return totem_armor(player, card, event, 3, 3, 0, 0);
}

int card_bramblesnap(int player, int card, event_t event){
/* CARD_ID_BRAMBLESNAP	10882
Bramblesnap	|1|G
Creature - Elemental 1/1
Trample
Tap an untapped creature you control: ~ gets +1/+1 until end of turn. */

	if(event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.illegal_state = TARGET_STATE_TAPPED;

		int num_can_tap = target_available(player, card, &td);

		if(num_can_tap <= 0)
			return 0;

		return generic_shade_amt_can_pump(player, card, 1, 0, MANACOST0, num_can_tap);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.illegal_state = TARGET_STATE_TAPPED;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST0, 1, 1);
	}

	return generic_activated_ability_with_tapsubtype_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, &td, 1);
}

int card_broodwarden(int player, int card, event_t event){
/* CARD_ID_BROODWARDEN	10885
Broodwarden	|3|G|G
Creature - Eldrazi Drone 4/4
Eldrazi Spawn creatures you control get +2/+1. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( player == affected_card_controller && has_creature_type(affected_card_controller, affected_card, SUBTYPE_SPAWN) &&
			has_creature_type(affected_card_controller, affected_card, SUBTYPE_ELDRAZI) && in_play(affected_card_controller, affected_card) )
		{
			if( event == EVENT_POWER ){
				event_result += 2;
			}
			else if( event == EVENT_TOUGHNESS ){
				event_result += 1;
			}
		}
	}

	return 0;
}

/* Daggerback Basilisk	|2|G (deadly recluse)
 * Creature - Basilisk 2/2
 * Deathtouch */

int card_gelatinous_genesis(int player, int card, event_t event){
/* CARD_ID_GELATINOUS_GENESIS	10932
Gelatinous Genesis	|X|X|G
Sorcery
Create X X/X |Sgreen Ooze creature tokens. */

	if (event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 1, NULL);
	}
	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		multi_x_spell(player, card, event, 2);
		return basic_spell(player, card, event);
	}
	if (event == EVENT_RESOLVE_SPELL){
		card_instance_t* instance = get_card_instance(player, card);
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_OOZE, &token);
		token.qty = token.pow = token.tou = instance->info_slot;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_gigantomancer(int player, int card, event_t event){
  /* Gigantomancer	|7|G
   * Creature - Human Shaman 1/1
   * |1: Target creature you control has base power and toughness 7/7 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			real_set_pt(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 7, 7, SET_PT_END_AT_EOT);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
}

/* Gravity Well	|1|G|G
 * Enchantment
 * Whenever a creature with flying attacks, it loses flying until end of turn. */

int card_growth_spasm(int player, int card, event_t event){
	/* Growth Spasm	|2|G
	 * Sorcery
	 * Search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. Put a 0/1 colorless Eldrazi Spawn creature token onto the battlefield. It has "Sacrifice this creature: Add |1 to your mana pool." */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_ELDRAZI_SPAWN);
	}
	return card_rampant_growth(player, card, event);
}

int card_haze_frog(int player, int card, event_t event){
/* Haze Frog	|3|G|G	0x200f575
 * Creature - Frog 2/1
 * Flash
 * When ~ enters the battlefield, prevent all combat damage that other creatures would deal this turn. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_phase != PHASE_BEFORE_BLOCKING || current_turn != 1-player ){
			ai_modifier -= 128;
		}
		else{
			ai_modifier += (count_attackers(current_turn) * 12);
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int legacy = fog_effect(player, card);
		card_instance_t *instance = get_card_instance(player, legacy);
		instance->targets[2].player = player;
		instance->targets[2].card = card;
	}

	return flash(player, card, event);
}

/* Irresistible Prey	|G
 * Sorcery
 * Target creature must be blocked this turn if able.
 * Draw a card. */

/* Jaddi Lifestrider	|4|G
 * Creature - Elemental 2/8
 * When ~ enters the battlefield, you may tap any number of untapped creatures you control. You gain 2 life for each creature tapped this way. */

int card_joraga_treespeaker(int player, int card, event_t event){
	/* Joraga Treespeaker	|G
	 * Creature - Elf Druid 1/1
	 * Level up |1|G
	 * LEVEL 1-4	1/2
	 * |T: Add |G|G to your mana pool.
	 * LEVEL 5+	1/4
	 * Elves you control have "|T: Add |G|G to your mana pool." */

	level_up_effects(player, card, event, 1, 1, 1, 1, 2, 0, 0, 5, 1, 4, 0, 0);

	if (!IS_ACTIVATING(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card))){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	int level = count_counters(player, card, COUNTER_LEVEL);

#define TAP_ELF(evnt)	(permanents_you_control_can_tap_for_mana(player, card, (evnt), TYPE_PERMANENT, SUBTYPE_ELF, COLOR_GREEN, 2))

#define CAN_LEVEL_UP	(!paying_mana() && can_use_activated_abilities(player, card) && has_mana_for_activated_ability(player, card, 1, 0, 0, 1, 0, 0) && can_sorcery_be_played(player, event))
#define CAN_GET_MANA	(level > 0 && !is_tapped(player, card) && !is_sick(player, card) && can_produce_mana(player, card))
#define CAN_TAP_ELF		(level > 4 && TAP_ELF(EVENT_CAN_ACTIVATE))
#define CHOICE_LEVEL	(1<<0)
#define CHOICE_MANA		(1<<1)
#define CHOICE_ELF		(1<<2)

	if( event == EVENT_CAN_ACTIVATE ){
		return CAN_LEVEL_UP || CAN_GET_MANA || CAN_TAP_ELF;
	}

	if( event == EVENT_ACTIVATE ){
		instance->info_slot = 0;

		int choice = (CAN_LEVEL_UP ? CHOICE_LEVEL : 0) | (CAN_GET_MANA ? CHOICE_MANA : 0) | (CAN_TAP_ELF ? CHOICE_ELF : 0);

		switch (num_bits_set(choice)){
			case 0:
				return 0;
			case 1:
				break;
			default:{
				char buffer[500];
				int pos = 0;
				int ai_choice = -1;
				buffer[pos++] = ' ';
				if (choice & CHOICE_LEVEL){
					ai_choice = 0;
				} else {
					buffer[pos++] = '_';
				}
				pos += scnprintf(buffer + pos, 500-pos, "Level up\n ", buffer);
				if (choice & CHOICE_MANA){
					ai_choice = 1;
				} else {
					buffer[pos++] = '_';
				}
				pos += scnprintf(buffer + pos, 500-pos, "Tap for GG\n ", buffer);
				if (choice & CHOICE_ELF){
					if (ai_choice != 1){
						ai_choice = 2;
					}
				} else {
					buffer[pos++] = '_';
				}
				pos += scnprintf(buffer + pos, 500-pos, "Tap an Elf for GG\n Cancel", buffer);

				choice = 1 << do_dialog(player, player, card, -1, -1, buffer, ai_choice);
			}
		}

		instance->info_slot = 0;
		if( choice == CHOICE_LEVEL && charge_mana_for_activated_ability(player, card, 1, 0, 0, 1, 0, 0) ){
			instance->info_slot = 66;
		}
		else if( choice == CHOICE_MANA ){
			produce_mana_tapped(player, card, COLOR_GREEN, 2);
		}
		else if( choice == CHOICE_ELF ){
			TAP_ELF(event);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_LEVEL, 1);
		}
	}

	if( level > 0 && event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if (level > 4){
			TAP_ELF(event);
		} else if (CAN_GET_MANA){
			declare_mana_available(player, COLOR_GREEN, 2);
		}
	}

	return 0;
#undef TAP_ELF
#undef CAN_LEVEL_UP
#undef CAN_GET_MANA
#undef CAN_TAP_ELF
#undef CHOICE_LEVEL
#undef CHOICE_MANA
#undef CHOICE_ELF
}

int card_kazandu_tuskcaller(int player, int card, event_t event){

	/* Kazandu Tuskcaller	|1|G
	 * Creature - Human Shaman 1/1
	 * Level up |1|G
	 * LEVEL 2-5	1/1
	 * |T: Put a 3/3 |Sgreen Elephant creature token onto the battlefield.
	 * LEVEL 6+	1/1
	 * |T: Put two 3/3 |Sgreen Elephant creature tokens onto the battlefield. */

	level_up_effects(player, card, event, 1, 1, 2, 1, 1, 0, 0, 6, 1, 1, 0, 0);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_LEVEL_UP = 1,
		CHOICE_ELEPHANT
	};

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) &&
			count_counters(player, card, COUNTER_LEVEL) >= 2 )
		{
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XG(1, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abils[3] = {
						0,
						has_mana_multi(player, MANACOST_XG(1, 1)) && can_sorcery_be_played(player, event),
						count_counters(player, card, COUNTER_LEVEL) >= 2 &&
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ? 1 : 0
		};
		char msg[100];
		if( count_counters(player, card, COUNTER_LEVEL) < 6 ){
			scnprintf(msg, 100, "Create a 3/3 Elephant");
		}
		else{
			scnprintf(msg, 100, "Create 2 3/3 Elephant");
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Level up", abils[1], 5,
							msg, abils[2], 15);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_LEVEL_UP ){
			int mod = count_counters(player, card, COUNTER_LEVEL) < 2 ? 24 : 12;
			ai_modifier += (current_phase > PHASE_DECLARE_BLOCKERS ? (mod * 2) : mod);
			return generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST_XG(1, 1), 0, NULL, NULL);
		}
		if( choice == CHOICE_ELEPHANT ){
			instance->targets[1].player = count_counters_no_parent(player, card, COUNTER_LEVEL);
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_LEVEL_UP ){
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_LEVEL, 1);
		}
		if( instance->info_slot == CHOICE_ELEPHANT ){
			generate_tokens_by_id(instance->parent_controller, instance->parent_card, CARD_ID_ELEPHANT,
									instance->targets[1].player >= 6 ? 2 : 1);
		}
	}

	return 0;
}

int card_khalni_hydra(int player, int card, event_t event){
  /* Khalni Hydra	|G|G|G|G|G|G|G|G
   * Creature - Hydra 8/8
   * This spell costs |G less to cast for each green creature you control.
   * Trample */

	if( event == EVENT_MODIFY_COST ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.required_color = COLOR_TEST_GREEN;
		td.illegal_abilities = 0;

		COST_GREEN -= target_available(player, card, &td);
	}

	return 0;
}

int card_kozileks_predator(int player, int card, event_t event){
	/* Kozilek's Predator	|3|G
	 * Creature - Eldrazi Drone 3/3
	 * When ~ enters the battlefield, put two 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to your mana pool." */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_tokens_by_id(player, card, CARD_ID_ELDRAZI_SPAWN, 2);
	}
	return 0;
}

/* Leaf Arrow	|G
 * Instant
 * ~ deals 3 damage to target creature with flying. */

/* Living Destiny	|3|G
 * Instant
 * As an additional cost to cast ~, reveal a creature card from your hand.
 * You gain life equal to the revealed card's converted mana cost. */

int card_might_of_the_masses(int player, int card, event_t event)
{
  /* Might of the Masses	|G
   * Instant
   * Target creature gets +1/+1 until end of turn for each creature you control. */
  int amt = (event == EVENT_CHECK_PUMP || event == EVENT_RESOLVE_SPELL) ? count_subtype(player, TYPE_CREATURE, -1) : 0;
  return vanilla_instant_pump(player, card, event, ANYBODY, player, amt, amt, 0, 0);
}

int card_momentous_fall(int player, int card, event_t event){
  /* Momentous Fall	|2|G|G
   * Instant
   * As an additional cost to cast this spell, sacrifice a creature.
   * You draw cards equal to the sacrificed creature's power, then you gain life equal to its toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( life[player] < 6 || hand_count[player] <= 1 ){
			ai_modifier += 24;
		}
		if( current_phase == PHASE_AFTER_BLOCKING ){
			ai_modifier += 24;
		}
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
		if (!sac){
			cancel = 1;
			return 0;
		}
		instance->targets[1].player = get_power(BYTE2(sac), BYTE3(sac));
		instance->targets[1].card = get_toughness(BYTE2(sac), BYTE3(sac));
		kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, instance->targets[1].player);
		gain_life(player, instance->targets[1].card);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_mul_daya_channelers(int player, int card, event_t event){
  /* Mul Daya Channelers	|1|G|G
   * Creature - Elf Druid Shaman 2/2
   * Play with the top card of your library revealed.
   * As long as the top card of your library is a creature card, this creature gets +3/+3.
   * As long as the top card of your library is a land card, this creature has "|T: Add two mana of any one color." */

	reveal_top_card(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		int iid = deck_ptr[player][0];
		if( iid > -1 && is_what(-1, iid, TYPE_CREATURE) ){
			event_result+=3;
		}
	}

	if( event == EVENT_CAN_ACTIVATE && mana_producer(player, card, event) ){
		int iid = deck_ptr[player][0];
		if( iid > -1 ){
			return is_what(-1, iid, TYPE_LAND) ? 1 : 0;
		}
	}

	if( event == EVENT_ACTIVATE ){
		produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 2);
	}

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			int iid = deck_ptr[player][0];
			if( iid > -1 && is_what(-1, iid, TYPE_LAND) ){
				declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 2);
			}
		}
	}

	return 0;
}

/* Naturalize	|1|G => 0x4a2f10:Disenchant
 * Instant
 * Destroy target artifact or enchantment. */

/* Nema Siltlurker	|4|G (vnailla)
 * Creature - Lizard 3/5 */

int card_nest_invader(int player, int card, event_t event){
	/* Nest Invader	|1|G
	 * Creature - Eldrazi Drone 2/2
	 * When ~ enters the battlefield, put a 0/1 colorless Eldrazi Spawn creature token onto the battlefield. It has "Sacrifice this creature: Add |1 to your mana pool." */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		generate_token_by_id(player, card, CARD_ID_ELDRAZI_SPAWN);
	}
	return 0;
}

/* Ondu Giant	|3|G (farhaven elf)
 * Creature - Giant Druid 2/4
 * When ~ enters the battlefield, you may search your library for a basic land card, put it onto the battlefield tapped, then shuffle your library. */

int card_overgrown_battlement(int player, int card, event_t event){
  /* Overgrown Battlement	|1|G
   * Creature - Wall 0/4
   * Defender
   * |T: Add |G for each creature you control with defender. */

	int count = 0;
	if (event == EVENT_COUNT_MANA || event == EVENT_ACTIVATE || (event == EVENT_CAN_ACTIVATE && player == AI)){	// Only count if it'll actually be needed
		count = count_defenders(player);
	}
	return mana_producing_creature(player, card, event, 24, COLOR_GREEN, count);
}

int card_pelakka_wurm(int player, int card, event_t event){

	/* Pelakka Wurm	|4|G|G|G
	 * Creature - Wurm 7/7
	 * Trample
	 * When ~ enters the battlefield, you gain 7 life.
	 * When ~ dies, draw a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		gain_life(player, 7);
	}

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_realms_uncharted(int player, int card, event_t event)
{
/*
Realms Uncharted |2|G	0x200f557
Instant
Search your library for up to four land cards with different names and reveal them. An opponent chooses two of those cards.
Put the chosen cards into your graveyard and the rest into your hand. Then shuffle your library.
 */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_LAND);

	return gift_ungiven_impl(player, card, event, &test, 0);
}

int card_snake_umbra(int player, int card, event_t event)
{
  /* Snake Umbra	|2|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 and has "Whenever this creature deals damage to an opponent, you may draw a card."
   * Totem armor */

  if (IS_DDBM_EVENT(event) || event == EVENT_GRAVEYARD_FROM_PLAY || event == EVENT_CHANGE_TYPE)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int packets;
	  if (instance->damage_target_card >= 0 && !is_humiliated(instance->damage_target_player, instance->damage_target_card, AL_TRIG)
		  && (packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_DAMAGE_OPPONENT|DDBM_TRIGGER_OPTIONAL)))
		draw_cards(player, packets);
	}

  return totem_armor(player, card, event, 1, 1, 0, 0);
}

int card_spider_umbra(int player, int card, event_t event){
  /* Spider Umbra	|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 and has reach. (It can block creatures with flying.)
   * Umbra armor (If enchanted creature would be destroyed, instead remove all damage from it and destroy this Aura.) */

	return totem_armor(player, card, event, 1, 1, KEYWORD_REACH, 0);
}

/* Sporecap Spider	|2|G (vanilla)
 * Creature - Spider 1/5
 * Reach */

/* Stomper Cub	|3|G|G (vanilla)
 * Creature - Beast 5/3
 * Trample */

/* Tajuru Preserver	|1|G => Vanilla plus hack in sacrifices.c
 * Creature - Elf Shaman 2/1
 * Spells and abilities your opponents control can't cause you to sacrifice permanents. */

int card_vengevine(int player, int card, event_t event){
  /* Vengevine	|2|G|G
   * Creature - Elemental 4/3
   * Haste
   * Whenever you cast a spell, if it's the second creature spell you cast this turn, you may return this card from your graveyard to the battlefield. */

	haste(player, card, event);
	return 0;
}

/* ManalinkEh 2022: 0x24b9a10; original cards/rise_of_the_eldrazi.c:4874. */
int card_wildheart_invoker(int player, int card, event_t event)
{
  /* Wildheart Invoker	|2|G|G
   * Creature - Elf Shaman 4/3
   * |8: Target creature gets +5/+5 and gains trample until end of turn. */

  card_instance_t *instance;
  target_definition_t td;
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if ((event == EVENT_RESOLVE_ACTIVATION) && (valid_target(&td) != 0))
    {
      instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             5, 5, 0x80, 0);
    }
  }
  return generic_activated_ability(player, card, event, 4, 8, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
}

/*************
* Multicolor *
*************/
int card_sarkhan_the_mad(int player, int card, event_t event)
{
  /* Sarkhan the Mad	|3|B|R	0x20024fb
   * Legendary Planeswalker - Sarkhan (7)
   * 0: Reveal the top card of your library and put it into your hand. ~ deals damage to himself equal to that card's converted mana cost.
   * -2: Target creature's controller sacrifices it, then that player creates a 5/5 |Sred Dragon creature token with flying.
   * -4: Each Dragon creature you control deals damage equal to its power to target player or planeswalker. */

  enum
  {
    CHOICE_REVEAL = 1,
    CHOICE_SAC_TO_DRAGON = 2,
    CHOICE_DRAGON_POWER = 3
  };

  int *deck;
  int choice_or_result;
  card_instance_t *dragon_or_sarkhan;
  int dragon_power_or_result;
  unsigned int sacrifice_available;
  int opponent_targets;
  int own_targets;
  target_definition_t *damage_target_definition;
  card_instance_t *instance;
  target_definition_t *opponent_creature_target;
  test_definition_t *dragon_test;
  int opponent_sacrifice_priority;
  int own_sacrifice_priority;
  token_generation_t token;
  target_definition_t td;
  target_definition_t td2;
  target_definition_t td3;
  target_definition_t td4;
  test_definition_t this_test;
  check_legend_rule(player, card, event);
  if ((event != EVENT_RESOLVE_ACTIVATION) &&
      (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE))
    return planeswalker(player, card, event, 7);
  default_target_definition(player, card, &td, 2);
  default_target_definition(player, card, &td2, 2);
  td2.preferred_controller = player;
  if (player == 1)
  {
    td2.power_requirement = 0x2004;
    td2.toughness_requirement = 0x2004;
    default_target_definition(1, card, &td3, 2);
    td3.power_requirement = 0x1005;
    td3.toughness_requirement = 0x1005;
    default_target_definition(1, card, &td4, 0x100);
    td4.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_test_definition(&this_test, 2);
    this_test.subtype = 0x30;
    instance = get_card_instance(1, card);
    own_targets = target_available(1, card, &td2);
    own_sacrifice_priority = own_targets * 3;
    opponent_targets = target_available(1, card, &td3);
    opponent_sacrifice_priority = opponent_targets * 3;
  }
  else
  {
    default_target_definition(player, card, &td3, 2);
    default_target_definition(player, card, &td4, 0x100);
    td4.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_test_definition(&this_test, 2);
    this_test.subtype = 0x30;
    instance = get_card_instance(player, card);
    own_targets = target_available(player, card, &td2);
    own_sacrifice_priority = own_targets * 3;
    opponent_targets = target_available(player, card, &td3);
    opponent_sacrifice_priority = opponent_targets * 3;
  }
  if (player != 0)
  {
    bool own_sacrifice_available = false;
    if (own_sacrifice_priority != 0)
    {
      choice_or_result = can_sacrifice(player, player, 1, TYPE_CREATURE, SUBTYPE_INVALID);
      sacrifice_available = 1;
      own_sacrifice_available = choice_or_result != 0;
    }
    if (!own_sacrifice_available)
    {
      if (opponent_sacrifice_priority == 0)
      {
        sacrifice_available = 0;
      }
      else
      {
        choice_or_result = can_sacrifice(player, 1 - player, 1, TYPE_CREATURE, SUBTYPE_INVALID);
        sacrifice_available = (unsigned int)(choice_or_result != 0);
      }
    }
  }
  else
    sacrifice_available = can_target(&td2);
  dragon_test = &this_test;
  opponent_creature_target = &td3;
  damage_target_definition = &td4;
  choice_or_result = check_battlefield_for_special_card(player, card, player, 0x40, dragon_test);
  if (own_sacrifice_priority < opponent_sacrifice_priority)
  {
    own_sacrifice_priority = opponent_sacrifice_priority;
  }
  choice_or_result =
      dialog_impl(player, card, event, DLG_RANDOM, DLG_PLANESWALKER, "Reveal & damage Sarkhan", 1, 10, 0,
                  "Impose sacrifice", sacrifice_available, own_sacrifice_priority, 0xfffffffe,
                  "Dragon Power", 1, choice_or_result * 2, 0xfffffffc, 0, 0, 0, 0);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (choice_or_result == 0)
    {
      return 0;
    }
  }
  else if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
  {
    instance->number_of_targets = '\0';
    if (choice_or_result == CHOICE_SAC_TO_DRAGON)
    {
      if (player == 1)
      {
        if (own_targets < opponent_targets)
        {
          pick_target(opponent_creature_target, "TARGET_CREATURE");
        }
        else
        {
          pick_target(&td2, "TARGET_CREATURE");
        }
      }
      else
      {
        pick_target(&td, "TARGET_CREATURE");
      }
    }
    else if (choice_or_result == CHOICE_DRAGON_POWER)
    {
      pick_target(damage_target_definition, "TARGET_PLAYER_OR_PLANESWALKER");
    }
  }
  else if (choice_or_result == CHOICE_REVEAL)
  {
    deck = deck_ptr[player];
    if (*deck != -1)
    {
      show_deck(1 - player, deck, 1, "Sarkhan the Mad revealed...", 0, gs_done_008b40e0);
      choice_or_result = add_card_to_hand(player, *deck);
      remove_card_from_deck(player, 0);
      choice_or_result = get_cmc(player, choice_or_result);
      remove_counters(player, card, instance->parent_controller, instance->parent_card,
                      COUNTER_LOYALTY, choice_or_result);
    }
  }
  else if (choice_or_result == CHOICE_SAC_TO_DRAGON)
  {
    choice_or_result = valid_target(&td);
    if ((choice_or_result != 0) &&
        (choice_or_result =
             can_sacrifice(player, instance->targets[0].player, 1, TYPE_CREATURE, SUBTYPE_INVALID),
         choice_or_result != 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
      default_token_definition(player, card, CARD_ID_DRAGON, &token);
      token.t_player = instance->targets[0].player;
      token.color_forced = '\x10';
      token.pow = 5;
      token.tou = 5;
      generate_token(&token);
    }
  }
  else
  {
    if ((choice_or_result == CHOICE_DRAGON_POWER) &&
        (choice_or_result = active_cards_count[player] + -1, -1 < choice_or_result))
    {
      do
      {
        dragon_or_sarkhan = in_play(player, choice_or_result);
        if ((dragon_or_sarkhan != NULL) &&
            ((dragon_power_or_result =
                  new_make_test_in_play(player, choice_or_result, -1, dragon_test),
              dragon_power_or_result != 0 &&
                  (dragon_power_or_result = valid_target(damage_target_definition),
                   dragon_power_or_result != 0))))
        {
          dragon_power_or_result = get_power(player, choice_or_result);
          damage_target0(player, card, dragon_power_or_result);
          dragon_or_sarkhan = get_card_instance(player, card);
          dragon_or_sarkhan->number_of_targets = '\0';
        }
        choice_or_result = choice_or_result + -1;
      } while (choice_or_result != -1);
    }
  }
  return planeswalker(player, card, event, 7);
}

/* Angelheart Vial	|5
 * Artifact
 * Whenever you're dealt damage, you may put that many charge counters on ~.
 * |2, |T, Remove four charge counters from ~: You gain 2 life and draw a card. */

/***********
* Artifact *
***********/

int card_dreamstone_hedron(int player, int card, event_t event){
  /* Dreamstone Hedron |6
   * Artifact
   * {T}: Add {C}{C}{C} to your mana pool.
   * {3}, {T}, Sacrifice Dreamstone Hedron: Draw three cards. */

	if( event == EVENT_COUNT_MANA && affect_me(player, card) ){
		if( mana_producer(player, card, EVENT_CAN_ACTIVATE) ){
			declare_mana_available(player, COLOR_COLORLESS, 3);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_DRAW
	};

	if( event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	if(event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST_X(6), 0, NULL, NULL) )
		{
			choice = CHOICE_SAC_DRAW;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Sac & draw 3 card", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA ){
			produce_mana_tapped(player, card, COLOR_COLORLESS, 3);
		}
		if( choice == CHOICE_SAC_DRAW ){
			ai_modifier += (hand_count[player] <= 1 ? 24 : 0);
			ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? 24 : 0);
			instance->state |= STATE_TAPPED;
			generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_X(3), 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				instance->state &= ~STATE_TAPPED;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_DRAW ){
			draw_cards(player, 3);
		}
	}

	return 0;
}

/* Enatu Golem	|6
 * Artifact Creature - Golem 3/5
 * When ~ dies, you gain 4 life. */

int card_hedron_matrix(int player, int card, event_t event){
/* CARD_ID_HEDRON_MATRIX	10951
Hedron Matrix	|4
Artifact - Equipment
Equipped creature gets +X/+X, where X is its converted mana cost.
Equip |4 */

	if( in_play(player, card) && is_equipping(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			event_result += get_cmc(p, c);
		}
	}

	return basic_equipment(player, card, event, 4);
}

int card_keening_stone(int player, int card, event_t event){
/* CARD_ID_KEENING_STONE	10967
Keening Stone	|6
Artifact
|5, |T: Target player puts the top X cards of his or her library into his or her graveyard, where X is the number of cards in that player's graveyard.*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_ACTIVATE ){
		generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(5), 0, &td, "TARGET_PLAYER");
		if( spell_fizzled != 1 ){
			int amount = count_graveyard(instance->targets[0].player);
			ai_modifier += (current_turn == 1-player && current_phase >= PHASE_MAIN2 ? (amount *3) : (amount * 2));
		}
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = count_graveyard(instance->targets[0].player);
			mill(instance->targets[0].player, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(5), 0, &td, "TARGET_PLAYER");
}

int card_ogres_cleaver(int player, int card, event_t event){
  /* Ogre's Cleaver	|2
   * Artifact - Equipment
   * Equipped creature gets +5/+0.
   * Equip |5 */

	return vanilla_equipment(player, card, event, 5, 5, 0, 0, 0);
}

int card_pennon_blade(int player, int card, event_t event)
{
  /* Pennon Blade	|3
   * Artifact - Equipment
   * Equipped creature gets +1/+1 for each creature you control.
   * Equip |4 */

	card_instance_t* instance;
	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS)
		&& (instance = in_play(player, card))
		&& ! is_humiliated(player, card, AL_STATIC)
		&& affect_me(instance->damage_target_player, instance->damage_target_card))
		event_result += count_subtype(player, TYPE_CREATURE, -1);

	return basic_equipment(player, card, event, 4);
}

int card_prophetic_prism(int player, int card, event_t event)
{
/* CARD_ID_PROPHETIC_PRISM	11021
Prophetic Prism	|2
Artifact
When ~ enters the battlefield, draw a card.
|1, |T: Add one mana of any color to your mana pool. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		draw_a_card(player);
	}
	return card_viridian_acolyte(player, card, event);
}

/* Reinforced Bulwark	|3
 * Artifact Creature - Wall 0/4
 * Defender
 * |T: Prevent the next 1 damage that would be dealt to you this turn. */

/* Runed Servitor	|2
 * Artifact Creature - Construct 2/2
 * When ~ dies, each player draws a card. */

int card_sphinx_bone_wand(int player, int card, event_t event){
	/* Sphinx-Bone Wand	|7	0x200865d
	 * Artifact
	 * Whenever you cast an instant or sorcery spell, you may put a charge counter on ~. If you do, ~ deals damage equal to the number of charge counters on it to any target. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_SPELL, NULL, player, card);
	if (event == EVENT_SPA){
		add_counters(player, card, player, card, COUNTER_CHARGE, 1);
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE | TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		card_instance_t* instance = get_card_instance(player, card);
		if (can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0)){
			damage_target0(player, card, count_counters(player, card, COUNTER_CHARGE));
		}
		instance->number_of_targets = 0;
	}
	return 0;
}

int card_warmongers_chariot(int player, int card, event_t event)
{
/* CARD_ID_WARMONGERS_CHARIOT	11079
Warmonger's Chariot	|2
Artifact - Equipment
Equipped creature gets +2/+2.
As long as equipped creature has defender, it can attack as though it didn't have defender.
Equip |3 (|3: Attach to target creature you control. Equip only as a sorcery.) */

	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_ABILITIES &&
		is_equipping(player, card) &&
		! is_humiliated(player, card, AL_STATIC) &&
		affect_me(instance->damage_target_player, instance->damage_target_card) )
		get_card_instance(instance->damage_target_player, instance->damage_target_card)->token_status |= STATUS_WALL_CAN_ATTACK;

	return vanilla_equipment(player, card, event, 3, 2, 2, 0, 0);
}

/*******
* Land *
*******/
int card_eldrazi_temple(int player, int card, event_t event){
/* CARD_ID_ELDRAZI_TEMPLE	10917
Eldrazi Temple	""
Land
|T: Add |C to your mana pool.
|T: Add |C|C to your mana pool. Spend this mana only to cast colorless Eldrazi spells or activate abilities of colorless Eldrazi. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_MANA) ){
		if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == player ){
			if(has_subtype(affected_card_controller, affected_card, SUBTYPE_ELDRAZI )
				&& is_colorless(affected_card_controller, affected_card)
				&& ! is_tapped(player, card) )
			{
				COST_COLORLESS--;
			}
		}
	}
	return mana_producer(player, card, event);
}

/* Evolving Wilds	"" => m11.c:Terramorphic Expanse
 * Land
 * |T, Sacrifice ~: Search your library for a basic land card and put it onto the battlefield tapped. Then shuffle your library. */
