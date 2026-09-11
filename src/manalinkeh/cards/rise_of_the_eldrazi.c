#include "manalink.h"

// Global functions
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
  // Whenever this creature attacks, defending player sacrifices [number] permanents.
  if (declare_attackers_trigger(player, card, event, DAT_STORE_IN_INFO_SLOT, player, card))	// info_slot to avoid conflict with It That Betrays
	impose_sacrifice(player, card, 1-player, number, TYPE_PERMANENT, 0, 0, 0, 0, 0, 0, 0, -1, 0);

  return 0;
}

/* Just the effects of leveling up, not activation.  l2sp/l3sp only work for special abilities that can just be set during EVENT_ABILITIES.  Currently only used
 * for deathtouch, indestructibility, lifelink, and vigilance, all of which are ok. */
static void level_up_effects(int player, int card, event_t event,
							 int l1power, int l1toughness,
							 int l2, int l2power, int l2toughness, keyword_t l2key, sp_keyword_t l2sp,
							 int l3, int l3power, int l3toughness, keyword_t l3key, sp_keyword_t l3sp)
{
  if (event == EVENT_POWER && affect_me(player, card) && l3power > l1power)
	{
	  int counters = count_counters(player, card, COUNTER_LEVEL);
	  if (counters >= l3)
		event_result += l3power - l1power;
	  else if (counters >= l2)
		event_result += l2power - l1power;
	}

  if (event == EVENT_TOUGHNESS && affect_me(player, card) && l3toughness > l1toughness)
	{
	  int counters = count_counters(player, card, COUNTER_LEVEL);
	  if (counters >= l3)
		event_result += l3toughness - l1toughness;
	  else if (counters >= l2)
		event_result += l2toughness - l1toughness;
	}

  if (event == EVENT_ABILITIES && affect_me(player, card) && (l3key || l3sp))
	{
	  int counters = count_counters(player, card, COUNTER_LEVEL);
	  if (counters >= l3)
		{
		  if (l3key & KEYWORD_BASIC_LANDWALK)
			l3key = get_hacked_walk(player, card, l3key);

		  if (l3key & KEYWORD_PROT_COLORED)
			l3key = get_sleighted_protection(player, card, l3key);

		  event_result |= l3key;

		  if (l3sp)
			special_abilities(player, card, event, l3sp, player, card);
		}
	  else if (counters >= l2)
		{
		  if (l2key & KEYWORD_BASIC_LANDWALK)
			l2key = get_hacked_walk(player, card, l2key);

		  if (l2key & KEYWORD_PROT_COLORED)
			l2key = get_sleighted_protection(player, card, l2key);

		  event_result |= l2key;

		  if (l2sp)
			special_abilities(player, card, event, l2sp, player, card);
		}
	}

  if (event == EVENT_SHOULD_AI_PLAY)
	{
	  /* Encourage AI to level up, particularly so that it crosses a level threshold, and preferably level up creatures the more counters they have past a level
	   * threshold.
	   *
	   * Each counter up to l2 is worth (1+number_of_counters) squared.  (+1 so that the first is worth 4, so the AI is willing to tap for it; it counts a
	   * tapped land as worth 1 less than an untapped one.)
	   *
	   * The counter putting it at l2 is worth an extra 24.
	   *
	   * Between l2 and l3, each counter is again worth (1+number_of_counters), but number_of_counters is counted from the l2 threshold, not from 0.
	   *
	   * The counter putting it at l3 is again worth an extra 24; counters past l3 aren't worth anything.
	   *
	   * Three examples:
	   * Enclave Cryptologist (l2 = 1, l3 = 3)
	   * Counters	Total modifier
	   * 0			0
	   * 1 (l2)		28 (2^2 + 24)
	   * 2			32 (l2bonus + 2^2)
	   * 3 (l3)		61 (l2bonus + 3^2 + 24)
	   *
	   * Student of Warfare (l2 = 2, l3 = 7)
	   * 0			0
	   * 1			4 (2^2)
	   * 2 (l2)		33 (3^2 + 24)
	   * 3			37 (l2bonus + 2^2)
	   * 4			42 (l2bonus + 3^2)
	   * 5			49 (l2bonus + 4^2)
	   * 6			58 (l2bonus + 5^2)
	   * 7 (l3)		93 (l2bonus + 6^2 + 24)
	   *
	   * Transcendent Master (l2 = 6, l3 = 12)
	   * 0			0
	   * 1			4
	   * 2			9
	   * 3			16
	   * 4			25
	   * 5			36
	   * 6 (l2)		73 (7^2 + 24)
	   * 7			77 (l2bonus + 2^2)
	   * 8			82 (l2bonus + 3^2)
	   * 9			89 (l2bonus + 4^2)
	   * 10			98 (l2bonus + 5^2)
	   * 11			109 (l2bonus + 6^2)
	   * 12			146 (l2bonus + 7^2 + 24)
	   */

	  int counters = count_counters(player, card, COUNTER_LEVEL);

	  if (!counters)
		return;

	  int mod = 0;

	  if (counters < l2)
		mod += (1+counters) * (1+counters);
	  else
		{
		  mod += (1+l2) * (1+l2) + 24;

		  if (counters > l2)
			{
			  if (counters >= l3)
				{
				  counters = l3;
				  mod += 24;
				}

			  int past_l2_plus_1 = counters - l2 + 1;
			  mod += past_l2_plus_1 * past_l2_plus_1;
			}
		}

	  if (player == AI)
		ai_modifier += mod;
	  else
		ai_modifier -= mod;
	}
}

// Level up, both effects and activation.
static int level_up(int player, int card, event_t event,
					int x, int b, int u, int g, int r, int w,
					int l1power, int l1toughness,
					int l2, int l2power, int l2toughness, keyword_t l2key, sp_keyword_t l2sp,
					int l3, int l3power, int l3toughness, keyword_t l3key, sp_keyword_t l3sp)
{
  level_up_effects(player, card, event,
				   l1power, l1toughness,
				   l2, l2power, l2toughness, l2key, l2sp,
				   l3, l3power, l3toughness, l3key, l3sp);

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_ACTIVATE(player, card, MANACOST6(x,b,u,g,r,w)) && can_sorcery_be_played(player, event);

  if (event == EVENT_ACTIVATE)
	charge_mana_for_activated_ability(player, card, x,b,u,g,r,w);

  if (event == EVENT_RESOLVE_ACTIVATION)
	add_counter(player, card, COUNTER_LEVEL);

  return 0;
}

int totem_armor(int player, int card, event_t event, int pow, int tou, int key, int s_key){

	card_instance_t *instance = get_card_instance(player, card);
	int t_player = instance->damage_target_player;
	int t_card = instance->damage_target_card;

	if( in_play(player, card) && t_player != -1 ){
		if( instance->targets[7].player == 1 ){
			return 0;
		}
		instance->targets[7].player = 1;
		if( in_play(player, card) && is_creature_dead() && ( land_can_be_played & LCBP_REGENERATION) ){
			card_instance_t *enchanted = get_card_instance(t_player, t_card);
			if( enchanted->kill_code == KILL_DESTROY || enchanted->kill_code == KILL_BURY ){
				int is_tap = is_tapped(t_player, t_card);
				regenerate_target_exe(t_player, t_card);
				if( ! is_tap ){
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

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {
					int count = active_cards_count[p]-1;
					while(count >= 0 ){
						if( in_play(p, count) && ! is_colorless(p, count) && is_what(p, count, TYPE_PERMANENT) ){
							kill_card(p, count, KILL_SACRIFICE);
						}
						count--;
					}
				}
		);
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

	if( get_card_instance(player, card)->damage_target_player > -1 && ! is_humiliated(player, card) )
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

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		state_untargettable(player, card, 1);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		state_untargettable(player, card, 0);
	}

	if( when_you_cast(player, card, event) ){
		alternate_legacy_text(1, player, time_walk_effect(player, card));
	}

	return 0;
}

int card_hand_of_emrakul(int player, int card, event_t event){
/*
Hand of Emrakul |9
Creature — Eldrazi 7/7
You may sacrifice four Eldrazi Spawn rather than pay Hand of Emrakul's mana cost.
Annihilator 1 (Whenever this creature attacks, defending player sacrifices a permanent.)
*/

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

static int it_that_betrays_effect(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_IT_THAT_BETRAYS ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
						if( instance->targets[11].player < 0 ){
							instance->targets[11].player = 0;
						}
						SET_BYTE1(instance->targets[11].player) = 1;
					}
				}
				if( affected_card_controller == 1-player &&
					is_what(affected_card_controller, affected_card, TYPE_PERMANENT) &&
					!is_token(affected_card_controller, affected_card) &&
					kill_code == KILL_SACRIFICE &&
					! check_special_flags(affected_card_controller, affected_card, SF_KILL_STATE_BASED_ACTION) )
				{
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
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
		if( result ){
			card_instance_t *instance = get_card_instance(player, card);
			int amount = BYTE0(result);
			int i;
			for(i=0; i<amount; i++){
				seek_grave_for_id_to_reanimate(player, card, instance->targets[i].player, instance->targets[i].card, REANIMATE_DEFAULT);
			}
			kill_card(player, card, KILL_EXILE);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	if( event == EVENT_CLEANUP && ! check_status(player, card, STATUS_INVISIBLE_FX) ){
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
				if( inst->info_slot == (int)it_that_betrays_effect ){
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

static const char* targets_a_permanent_you_control(int who_chooses, int player, int card)
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
  /*
	Not of This World |7

	Tribal Instant — Eldrazi

	Counter target spell or ability that targets a permanent you control.

	Not of This World costs {7} less to cast if it targets a spell or ability that targets a creature you control with power 7 or greater.
  */
  target_definition_t td_effect;
  counter_activated_target_definition(player, card, &td_effect, 0);
  td_effect.extra = (int32_t)targets_a_permanent_you_control;
  td_effect.special |= TARGET_SPECIAL_EXTRA_FUNCTION;

  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, 0);
  td_spell.extra = (int32_t)targets_a_permanent_you_control;
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

int card_pathrazer_of_ulamog(int player, int card, event_t event){
	/* Pathrazer of Ulamog	|11
	 * Creature - Eldrazi 9/9
	 * Annihilator 3
	 * ~ can't be blocked except by three or more creatures. */

	annihilator(player, card, event, 3);
	minimum_blockers(player, card, event, 3);
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
				int result = card_from_list(player, 2, TYPE_ANY, 0, SUBTYPE_ELDRAZI, 0, COLOR_COLORLESS, 0, 0, 0, -1, 0);
				while( result != -1 ){
						RULES_ENGINE_AND_PLAY(player, result);
						result = card_from_list(player, 2, TYPE_ANY, 0, SUBTYPE_ELDRAZI, 0, COLOR_COLORLESS, 0, 0, 0, -1, 0);
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

/********
* White *
********/

/* Affa Guard Hound	|2|W
 * Creature - Hound 2/2
 * Flash
 * When ~ enters the battlefield, target creature gets +0/+3 until end of turn. */

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

int card_gideon_jura(int player, int card, event_t event){

	/* Gideon Jura	|3|W|W
	 * Planeswalker - Gideon (6)
	 * +2: During target opponent's next turn, creatures that player controls attack ~ if able.
	 * -2: Destroy target tapped creature.
	 * 0: Until end of turn, ~ becomes a 6/6 Human Soldier creature that's still a planeswalker. Prevent all damage that would be dealt to him this turn. */

	double_faced_card(player, card, event);

	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t *instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;
		td.required_state = TARGET_STATE_TAPPED;


		enum {
			CHOICE_FORCE_ATTACK = 1,
			CHOICE_KILL,
			CHOICE_ANIMATE,
		}
		choice = DIALOG(player, card, event,
						DLG_RANDOM, DLG_PLANESWALKER,
						"Make creatures attack", 1, count_counters(player, card, COUNTER_LOYALTY) < 6 ? 20 : 5, 2,
						"Kill tapped creature", can_target(&td), 10, -2,
						"Become creature", 1, count_counters(player, card, COUNTER_LOYALTY) >= 6 ? 15 : 5, 0);

	  if (event == EVENT_CAN_ACTIVATE)
		{
		  if (!choice)
			return 0;	// else fall through to planeswalker()
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
	  else	// event == EVENT_RESOLVE_ACTIVATION
		switch (choice)
		  {
			case CHOICE_FORCE_ATTACK:
				;int p = instance->parent_controller, c = instance->parent_card;
				if (in_play(p, c)){
					create_legacy_effect(p, c, &gideon_jura_force_attack);
				}
				break;

			case CHOICE_ANIMATE:
				true_transform(player, instance->parent_card);
				break;

			case CHOICE_KILL:
				if (valid_target(&td)){
					kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
				}
				break;
		  }
	}

	return planeswalker(player, card, event, 6);
}

int card_gideon_jura_animated(int player, int card, event_t event){

	// prevent any damage that would be dealt to me
	if( event == EVENT_DEAL_DAMAGE && ! is_humiliated(player, card) ){
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

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) )
		event_result += count_auras_enchanting_me(player, card) * 2;

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ENCHANTMENT);
		test.subtype = SUBTYPE_AURA;
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			draw_a_card(player);
		}
	}

	return 0;
}

int card_lightmine_field(int player, int card, event_t event){
/*
Lightmine Field |2|W|W
Enchantment
Whenever one or more creatures attack, Lightmine Field deals damage to each of those creatures equal to the number of attacking creatures.
*/
	int amt;
	if ((amt = declare_attackers_trigger(player, card, event, DAT_TRACK, ANYBODY, -1)))
	{
		card_instance_t* instance = get_card_instance(player, card);
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

int card_linvala_keeper_of_silence(int player, int card, event_t event){
/* CARD_ID_LINVALA_KEEPER_OF_SILENCE	10982
Linvala, Keeper of Silence	|2|W|W
Legendary Creature - Angel 3/4
Flying
Activated abilities of creatures your opponents control can't be activated. */

	check_legend_rule(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card) )
		player_bits[1-player] |= PB_ACTIVATE_ABILITIES_OF_CREATURES_DISABLED;

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
			int *deck = deck_ptr[instance->targets[0].player];
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
			int transit = deck[1];
			deck[1] = deck[0];
			deck[0] = transit;
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

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card)
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

/*******
* Blue *
*******/

/* Aura Finesse	|U
 * Instant
 * Attach target Aura you control to target creature.
 * Draw a card. */

/* Cast Through Time	|4|U|U|U
 * Enchantment
 * Instant and sorcery spells you control have rebound. */

int card_champions_drake(int player, int card, event_t event)
{
  /* Champion's Drake	|1|U
   * Creature - Drake 1/1
   * Flying
   * ~ gets +3/+3 as long as you control a creature with three or more level counters on it. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
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
		! is_humiliated(player, card) &&
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

	if( in_play(player, card) && ! is_humiliated(player, card) ){
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

	if( ! is_humiliated(player, card) &&
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

	level_up_effects(player, card, event,
					2, 3,
					2, 2, 4, 0,0,
					4, 2, 5, 0,0);

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
						generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0,
												    NULL, NULL),
						count_counters(player, card, COUNTER_LEVEL) >= 2 &&
						generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_UNTAPPED, MANACOST0, 0,
													&td, NULL) ? 1 : 0
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
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
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_LEVEL);
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

	level_up_effects(player, card, event,
					    0,1,
					 1, 0,1, 0,0,
					 3, 0,1, 0,0);

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
		int abils[3] = {
						0,
						generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0, NULL, NULL),
						count_counters(player, card, COUNTER_LEVEL) >= 1 &&
						generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ? 1 : 0
		};
		char msg[100];
		if( count_counters(player, card, COUNTER_LEVEL) < 3 ){
			scnprintf(msg, 100, "Draw & discard");
		}
		else{
			scnprintf(msg, 100, "Draw a card");
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
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
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_LEVEL);
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

	if( event == EVENT_POWER && is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_humiliated(player, card) ){
		if( check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) ){
			event_result+=2;
		}
		else{
			event_result-=2;
		}
	}

	return global_enchantment(player, card, event);
}

int card_guard_gomazoa(int player, int card, event_t event){
/* CARD_ID_GUARD_GOMAZOA	10944
Guard Gomazoa	|2|U
Creature - Jellyfish 1/3
Defender, flying
Prevent all combat damage that would be dealt to ~. */

	card_instance_t* damage = combat_damage_being_prevented(event);
	if( damage && damage->damage_target_card == card && damage->damage_target_player == player && ! is_humiliated(player, card) ){
		prevent_damage(affected_card_controller, affected_card, -1);
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

	if( ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_tapped(instance->damage_target_player, instance->damage_target_card) && upkeep_trigger(player, card, event) ){
			tap_card( instance->damage_target_player, instance->damage_target_card );
		}
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


int card_shared_discovery(int player, int card, event_t event){
/* Shared Discovery	|U	0x200f56b
 * Sorcery
 * As an additional cost to cast ~, tap four untapped creatures you control.
 * Draw three cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_CAN_CAST && basic_spell(player, card, event) ){
		return target_available(player, card, &td) >= 4;
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( current_phase >= PHASE_MAIN2 ){
			int untap_effect_found = 0;
			int my_c = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_PERMANENT) && ! is_humiliated(p, c) ){
											int csvid = get_id(p, c);
											switch( csvid ){
													case CARD_ID_AWAKENING:
														untap_effect_found = 1;
														break;
													case CARD_ID_SEEDBORN_MUSE:
													case CARD_ID_PROPHET_OF_KRUPHIX:
													{
														if( p == player ){
															untap_effect_found = 1;
														}
														break;
													}
													default:
														break;
											}
											if( p == player && is_what(p, c, TYPE_CREATURE) && ! is_tapped(p, c) ){
												my_c++;
											}
										}
									}
			);
			if( untap_effect_found ){
				ai_modifier += 48;
			}
			if( count_subtype(1-player, TYPE_CREATURE, -1) <= (my_c - 4) ){
				ai_modifier += 24;
			}
		}
		int ttc = 0;
		while( ttc < 4 ){
				char msg[100];
				scnprintf(msg, 100, "Select a creature you control to tap (%d of 4)", ttc+1);
				if( new_pick_target(&td, msg, ttc, 1 | GS_LITERAL_PROMPT) ){
					state_untargettable(instance->targets[ttc].player, instance->targets[ttc].card, 1);
					ttc++;
				}
				else{
					break;
				}
		}
		int i;
		for(i=0; i<ttc; i++){
			state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			if( ttc == 4 ){
				tap_card(instance->targets[i].player, instance->targets[i].card);
			}
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);
		kill_card(player, card, KILL_DESTROY);
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

int card_sphinx_of_magosi(int player, int card, event_t event){
/* CARD_ID_SPHINX_OF_MAGOSI	11048
Sphinx of Magosi	|3|U|U|U
Creature - Sphinx 6/6
Flying
|2|U: Draw a card, then put a +1/+1 counter on ~. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		return generic_shade(player, card, event, 0, MANACOST_XU(2, 1), 1, 1, 0, 0);
	}

	if( event == EVENT_ACTIVATE ){
		if( current_phase >= PHASE_MAIN2 ){
			ai_modifier += (current_turn == 1-player ? 24 : -12);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		draw_cards(player, 1);
		add_1_1_counter(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XU(2, 1), 0, NULL, NULL);
}

int card_surrakar_spellblade(int player, int card, event_t event){

	/* Surrakar Spellblade	|1|U|U
	 * Creature - Surrakar 2/1
	 * Whenever you cast an instant or sorcery spell, you may put a charge counter on ~.
	 * Whenever ~ deals combat damage to a player, you may draw X cards, where X is the number of charge counters on it. */

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			add_counter(player, card, COUNTER_CHARGE);
		}
	}

	int result = whenever_i_deal_damage_to_a_player(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_TRIGGER_OPTIONAL);
	if( result ){
		int amount = BYTE0(result) + BYTE1(result);
		draw_cards(player, count_counters(player, card, COUNTER_CHARGE) * amount);
	}

	return 0;
}

int card_training_grounds(int player, int card, event_t event){
/* CARD_ID_TRAINING_GROUNDS	11065
Training Grounds	|U
Enchantment
Activated abilities of creatures you control cost up to |2 less to activate.
This effect can't reduce the amount of mana an ability costs to activate to less than one mana. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		set_cost_mod_for_activated_abilities(player, -1, 2, &this_test);
	}

	if( event == EVENT_CAST_SPELL && affected_card_controller == player ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			set_cost_mod_for_activated_abilities(affected_card_controller, affected_card, 2, 0);
		}
	}


	if( leaves_play(player, card, event) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		remove_cost_mod_for_activated_abilities(player, -1, 2, &this_test);
	}

	return global_enchantment(player, card, event);
}

int card_unified_will(int player, int card, event_t event){
/*
Unified Will |1|U
Instant
Counter target spell if you control more creatures than that spell's controller.
*/
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

int card_venerated_teacher(int player, int card, event_t event){

  /* Venerated Teacher	|2|U
   * Creature - Human Wizard 2/2
   * When ~ enters the battlefield, put two level counters on each creature you control with level up. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int mod = 0;
		int c;
		card_instance_t* inst;
		for (c = 0; c < active_cards_count[player]; ++c){
			if( (inst = in_play(player, c)) && is_what(player, c, TYPE_CREATURE) &&
				get_counter_type_by_id(cards_data[inst->internal_card_id].id) == COUNTER_LEVEL)
				mod += 12;
		}
		if( ! mod ){
			ai_modifier -= 96;
		}
		else{
			ai_modifier += mod;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int c;
		card_instance_t* inst;
		for (c = 0; c < active_cards_count[player]; ++c)
			if( (inst = in_play(player, c)) && is_what(player, c, TYPE_CREATURE) &&
				get_counter_type_by_id(cards_data[inst->internal_card_id].id) == COUNTER_LEVEL)
				add_counters(player, c, COUNTER_LEVEL, 2);
	}

	return 0;
}

/********
* Black *
********/

static void destroy_self_at_end_of_combat_if_vs_le_1_power(int player, int card, int t_player, int t_card)
{
  if (get_power(t_player, t_card) <= 1)
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, player, card);
}
int card_arrogant_bloodlord(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card))
	instance->destroys_if_blocked |= DIFB_ASK_CARD;

  if (event == EVENT_CHECK_DESTROY_IF_BLOCKED && affect_me(player, card) && !is_humiliated(player, card)
	  && get_power(attacking_card_controller, attacking_card) <= 1)
	event_result |= 2;

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card))
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
			show_deck(0, deck, 1, "Baneful Omen reveals...", 0, 0x7375B0 );
			show_deck(1, deck, 1, "Baneful Omen reveals...", 0, 0x7375B0 );
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
		APNAP(p, { new_manipulate_all(player, card, p, &test, KILL_BURY); };);
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

	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_card_instance(player, card)->info_slot;
		if( new_specific_spell_played(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY, &test) ){
			lose_life(trigger_cause_controller, 1);
		}
	}

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

int card_drana_kalastrian_highborn(int player, int card, event_t event){
/*
Drana, Kalastria Bloodchief |3|B|B
Legendary Creature — Vampire Shaman 4/4
Flying
{X}{B}{B}: Target creature gets -0/-X until end of turn and Drana, Kalastria Bloodchief gets +X/+0 until end of turn.
*/
	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int amount = instance->targets[1].card;
			if( amount > 0 ){
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->parent_controller, instance->parent_card, amount, 0);
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 0, -amount);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XB(-1, 2), 0, &td, "TARGET_CREATURE");
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

	level_up_effects(player, card, event,
					    1,1,
					 2, 2,2, 0,0,
					 4, 4,4, 0,0);

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
						generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0,
												   NULL, NULL),
						count_counters(player, card, COUNTER_LEVEL) >= 2 &&
						generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td,
												   "TARGET_CREATURE") ? 1 : 0
		};
		char msg[100];
		if( count_counters(player, card, COUNTER_LEVEL) < 4 ){
			scnprintf(msg, 100, "Give -2/-2");
		}
		else{
			scnprintf(msg, 100, "Give -4/-4");
		}
		
		int choice = DIALOG(player, card, event, DLG_RANDOM,
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
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_LEVEL);
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
/*
Hellcarver Demon |3|B|B|B
Creature — Demon 6/6
Flying
Whenever Hellcarver Demon deals combat damage to a player, sacrifice all other permanents you control and discard your hand.
Exile the top six cards of your library. You may cast any number of nonland cards exiled this way without paying their mana costs.
*/
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
					show_deck( HUMAN, deck, amount, "Cards exiled by Hellcarver Demon", 0, 0x7375B0 );
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
								play_card_in_exile_for_free(player, 1-player, csvid);
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
		add_1_1_counter(player, card);
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

static int pawn_of_ulamog_effect(int player, int card, event_t event){

	//In case something is copying Pawn of Ulamog and then reverts to its original form.
	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( instance->damage_target_player > -1 ){
			if( get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_PAWN_OF_ULAMOG ){
				kill_card(player, card, KILL_EXILE);
			}
		}
	}

	if( get_card_instance(player, card)->damage_target_player > -1 && effect_follows_control_of_attachment(player, card, event) ){
		return 0;
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(affected_card_controller, affected_card) ){
			int kill_code = get_card_instance(affected_card_controller, affected_card)->kill_code;
			if( kill_code ){
				if( instance->damage_target_player > -1 ){
					int p = instance->damage_target_player;
					int c = instance->damage_target_card;
					if( affect_me(p, c) ){
						//Will trigger also if Pawn of Ulamog is not a creature when it goes into the graveyard;
						if( ! is_what(p, c, TYPE_CREATURE) ){
							count_for_gfp_ability(player, card, event, player, TYPE_PERMANENT, NULL);
						}
						instance->damage_target_player = instance->damage_target_card = -1;
						remove_status(player, card, STATUS_INVISIBLE_FX);
					}
				}
				if( ! is_token(affected_card_controller, affected_card) ){
					count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
				}
			}
		}
	}

	if( ! check_status(player, card, STATUS_INVISIBLE_FX) ){
		int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
		if( amount ){

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_ELDRAZI_SPAWN, &token);
			token.qty = amount;
			generate_token(&token);

			kill_card(player, card, KILL_EXILE);
		}
	}

	//Reset counts if the legacy is still invisible
	if( trigger_condition == TRIGGER_GRAVEYARD_FROM_PLAY && event == EVENT_END_TRIGGER ){
		get_card_instance(player, card)->targets[11].player = 0;
	}

	//If you decide to not get the Eldrazi Spawns...
	if( event == EVENT_CLEANUP && get_card_instance(player, card)->damage_target_player == -1 ){
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_pawn_of_ulamog(int player, int card, event_t event){
	/*
	  Pawn of Ulamog English
	  Creature — Vampire Shaman 2/2, 1BB (3)
	  Whenever Pawn of Ulamog or another nontoken creature you control dies, you may put a 0/1 colorless Eldrazi Spawn creature token
	  onto the battlefield. It has "Sacrifice this creature: Add {C} to your mana pool."
	*/

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)pawn_of_ulamog_effect ){
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

int card_thought_gorger(int player, int card, event_t event){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		add_1_1_counters(player, card, hand_count[player]);
		discard_all(player);
	}

	if( leaves_play(player, card, event) ){
		draw_cards(player, count_1_1_counters(player, card));
	}

	return 0;
}

/* Vendetta	|B => mercadian_masques.c
 * Instant
 * Destroy target non|Sblack creature. It can't be regenerated. You lose life equal to that creature's toughness. */

int card_virulent_swipe(int player, int card, event_t event){

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

/******
* Red *
******/

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
  level_up_effects(player, card, event,
				      2,2,
				   1, 2,3, 0,0,
				   3, 2,4, 0,0);

  if (IS_ACTIVATING(event))
	{
	  int level = count_counters_no_parent(player, card, COUNTER_LEVEL);

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	  char damage_prompt[100];
	  if (ai_is_speculating == 1)
		damage_prompt[0] = 0;
	  else
		scnprintf(damage_prompt, 100, "Deal %d damage", level >= 3 ? 3 : 1);

	  int duh_damage = level >= 3 && !IS_AI(player) && duh_mode(player);

	  enum
	  {
		CHOICE_LEVEL_UP = 1,
		CHOICE_DAMAGE
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						(level == 0 || duh_damage) ? DLG_AUTOCHOOSE_IF_1 : DLG_NO_OP,
						"Level up",
							can_sorcery_be_played(player, event) && !duh_damage,
							level < 3 ? 3 : -1,
							DLG_MANA(MANACOST_XR(3,1)),
						damage_prompt,
							level >= 1,
							1,
							DLG_TAP, DLG_TARGET(&td, "TARGET_ANY"));

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		switch (choice)
		  {
			case CHOICE_LEVEL_UP:
			  add_counter(player, card, COUNTER_LEVEL);
			  break;

			case CHOICE_DAMAGE:
			  damage_target0(player, card, level >= 3 ? 3 : 1);
			  break;
		  }
	}

  // The most straightforward parts of the AI from card_prodigal_sorcerer() in the exe, which is quite extensive.

  // Let AI know that it can damage blockers when considering what to attack with
  if (event == EVENT_CHECK_PUMP && CAN_TAP(player, card) && CAN_ACTIVATE0(player, card))
	{
	  int level = count_counters(player, card, COUNTER_LEVEL);
	  if (level >= 3)
		pumpable_toughness[1 - player] -= 3;
	  else if (level >= 1)
		pumpable_toughness[1 - player] -= 1;
	}

  if (event == EVENT_SHOULD_AI_PLAY && player == AI)
	{
	  // Need to value counters slightly more than usual, so the AI will be willing to pay |4 for the second one.
	  int level = count_counters(player, card, COUNTER_LEVEL);
	  level = MIN(3, level);
	  ai_modifier += level;

	  // Discourage AI from tapping during its own turn if it could tap to deal damage.
	  if (level > 0 && current_turn == player && CAN_TAP(player, card) && CAN_ACTIVATE0(player, card))
		ai_modifier += level >= 3 ? 72 : 24;
	}

  /* Discourage AI from attacking with this if it could tap to deal damage.  Prodigal Sorcerer also discourages blocking by subtracking 48 from
   * ai_defensive_modifier during EVENT_BLOCK_RATING; I'm omitting it for this. */
  if (event == EVENT_ATTACK_RATING && affect_me(player, card) && CAN_ACTIVATE0(player, card)
	  && count_counters(player, card, COUNTER_LEVEL) > 0)
	ai_defensive_modifier += 48;

  return 0;
}

/* Brood Birthing	|1|R
 * Sorcery
 * If you control an Eldrazi Spawn, put three 0/1 colorless Eldrazi Spawn creature tokens onto the battlefield. They have "Sacrifice this creature: Add |1 to
 * your mana pool." Otherwise, put one of those tokens onto the battlefield. */

int card_conquering_manticore(int player, int card, event_t event){

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
/*
Devastating Summons |R
Sorcery
As an additional cost to cast Devastating Summons, sacrifice X lands.
Put two X/X red Elemental creature tokens onto the battlefield.
*/
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

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, &test) ){
			pump_until_eot_merge_previous(player, card, player, card, 3, 0);
		}
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

  if ((event == EVENT_DECLARE_ATTACKERS || (xtrigger_condition() == XTRIGGER_ATTACKING && affect_me(player, card)))
	  && count_counters(player, card, COUNTER_LEVEL) >= 6 && declare_attackers_trigger(player, card, event, 0, player, card))
	new_damage_all(player, card, 1-player, 6, 0, NULL);

  return level_up(player, card, event, MANACOST_XR(1,1),
				     3,3,
				  1, 6,6, 0,0,
				  6, 6,6, 0,0);
}

/* Lust for War	|2|R
 * Enchantment - Aura
 * Enchant creature
 * Whenever enchanted creature becomes tapped, ~ deals 3 damage to that creature's controller.
 * Enchanted creature attacks each turn if able. */

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
  if (xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.power = 3;
	  test.power_flag = F5_POWER_LESSER_THAN_VALUE;

	  int attckr = declare_attackers_trigger_test(player, card, event, DAT_SEPARATE_TRIGGERS, player, -1, &test) - 1;
	  if (attckr >= 0)
		{
		  int attacking = planeswalker_its_attacking(player, attckr);
		  if (attacking != -2)
			damage_creature(1-current_turn, attacking, 1, player, attckr);
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

	if( ! is_humiliated(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
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

	if( ! IS_AURA_EVENT(player, card, event) ){
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

	check_legend_rule(player, card, event);

	return 0;
}

int card_valakut_fireboar(int player, int card, event_t event)
{
  // Whenever ~ attacks, switch its power and toughness until end of turn.
  if (declare_attackers_trigger(player, card, event, 0, player, card))
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

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (current_turn == player && current_phase < PHASE_DECLARE_ATTACKERS ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		create_legacy_effect(player, card, waw_legacy);
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
			if( IS_AI(player) && selected == -1 ){
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
		&& ! is_humiliated(player, card)
		&& player == attacking_card_controller && card == attacking_card
		&& get_power(affected_card_controller, affected_card) < get_power(player, card))
		event_result = 1;

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) )
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
  // Enchanted creature gets +2/+2 and has "Whenever this creature attacks, untap all lands you control."
  if (xtrigger_condition() == XTRIGGER_ATTACKING || event == EVENT_DECLARE_ATTACKERS)
	{
	  card_instance_t* instance = in_play(player, card);
	  if (instance && instance->damage_target_player >= 0
		  && !is_humiliated(instance->damage_target_player, instance->damage_target_card)
		  && declare_attackers_trigger(player, card, event, 0, instance->damage_target_player, instance->damage_target_card))
		manipulate_all(player, card, player, TYPE_LAND, 0, 0, 0, 0, 0, 0, 0, -1, 0, ACT_UNTAP);
	}

  return totem_armor(player, card, event, 2, 2, 0, 0);
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

	if( in_play(player, card) && ! is_humiliated(player, card) ){
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

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags2(player, card, SF2_COPIED_FROM_STACK) ){
			card_instance_t *instance = get_card_instance(player, card);
			instance->info_slot = 0;
			set_special_flags2(player, card, SF2_X_SPELL);
			int angels = charge_mana_for_double_x(player, COLOR_COLORLESS);
			if( spell_fizzled != 1 ){
				int diff = true_get_updated_casting_cost(player, card, -1, event, -1);
				angels -= diff;
				instance->info_slot = angels/2;
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);

		token_generation_t token;
		default_token_definition(player, card, CARD_ID_OOZE, &token);
		token.qty = token.pow = token.tou = instance->info_slot;
		generate_token(&token);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_gigantomancer(int player, int card, event_t event){

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
			set_pt_and_abilities_until_eot(instance->parent_controller, instance->parent_card,
											instance->targets[0].player, instance->targets[0].card, 7, 7, 0, 0, 0);
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

	level_up_effects(player, card, event,
					    1,1,
					 1, 1,2, 0,0,
					 5, 1,4, 0,0);

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
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_LEVEL);
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

	level_up_effects(player, card, event,
					    1,1,
					 2, 1,1, 0,0,
					 6, 1,1, 0,0);

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
						generic_activated_ability(player, card, event, GAA_CAN_SORCERY_BE_PLAYED, MANACOST0, 0,
												NULL, NULL),
						count_counters(player, card, COUNTER_LEVEL) >= 2 &&
						generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL) ? 1 : 0
		};
		char msg[100];
		if( count_counters(player, card, COUNTER_LEVEL) < 3 ){
			scnprintf(msg, 100, "Create a 3/3 Elephant");
		}
		else{
			scnprintf(msg, 100, "Create 2 3/3 Elephant");
		}
		int choice = DIALOG(player, card, event, DLG_RANDOM,
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
			add_counter(instance->parent_controller, instance->parent_card, COUNTER_LEVEL);
		}
		if( instance->info_slot == CHOICE_ELEPHANT ){
			generate_tokens_by_id(instance->parent_controller, instance->parent_card, CARD_ID_ELEPHANT,
									instance->targets[1].player >= 6 ? 2 : 1);
		}
	}

	return 0;
}

int card_khalni_hydra(int player, int card, event_t event){

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

	reveal_top_card(player, card, event);

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card) ){
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

  if (IS_DDBM_EVENT(event))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int packets;
	  if (instance->damage_target_card >= 0 && !is_humiliated(instance->damage_target_player, instance->damage_target_card)
		  && (packets = equipped_creature_deals_damage(player, card, event, DDBM_MUST_DAMAGE_OPPONENT|DDBM_TRIGGER_OPTIONAL)))
		draw_cards(player, packets);
	}

  return totem_armor(player, card, event, 1, 1, 0, 0);
}

int card_spider_umbra(int player, int card, event_t event){

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
	haste(player, card, event);
	return 0;
}

/* Wildheart Invoker	|2|G|G
 * Creature - Elf Shaman 4/3
 * |8: Target creature gets +5/+5 and gains trample until end of turn. */

/*************
* Multicolor *
*************/

int card_sarkhan_the_mad(int player, int card, event_t event){
	/* Sarkhan the Mad	|3|B|R	0x20024fb
	 * Legendary Planeswalker - Sarkhan (7)
	 * 0: Reveal the top card of your library and put it into your hand. ~ deals damage to himself equal to that card's converted mana cost.
	 * -2: Target creature's controller sacrifices it, then that player creates a 5/5 |Sred Dragon creature token with flying.
	 * -4: Each Dragon creature you control deals damage equal to its power to target player or planeswalker. */
	if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.preferred_controller = player;
		if( player == AI ){
			td2.power_requirement = 4 | TARGET_PT_LESSER_OR_EQUAL;
			td2.toughness_requirement = 4 | TARGET_PT_LESSER_OR_EQUAL;
		}

		target_definition_t td3;
		default_target_definition(player, card, &td3, TYPE_CREATURE);
		if( player == AI ){
			td3.power_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
			td3.toughness_requirement = 5 | TARGET_PT_GREATER_OR_EQUAL;
		}

		target_definition_t td4;
		default_target_definition(player, card, &td4, TARGET_TYPE_PLANESWALKER);
		td4.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.subtype = SUBTYPE_DRAGON;

		card_instance_t *instance = get_card_instance(player, card);

		int priority_you_sac = target_available(player, card, &td2) * 3;
		int priority_opp_sac = target_available(player, card, &td3) * 3;
		int can_sac = player == HUMAN ? can_target(&td2) : ((priority_you_sac && can_sacrifice(player, player, 1, TYPE_CREATURE, 0)) ||
															(priority_opp_sac && can_sacrifice(player, 1-player, 1, TYPE_CREATURE, 0)));
		int priority_dragon_power = check_battlefield_for_special_card(player, card, player, CBFSC_GET_TOTAL_POW, &this_test) * 2;

		enum {
			CHOICE_REVEAL = 1,
			CHOICE_SAC_TO_DRAGON,
			CHOICE_DRAGON_POWER
		}
		choice = DIALOG(player, card, event, DLG_RANDOM, DLG_PLANESWALKER,
						"Reveal & damage Sarkhan", 1, 10, 0,
						"Impose sacrifice", can_sac, priority_opp_sac > priority_you_sac ? priority_opp_sac : priority_you_sac, -2,
						"Dragon Power", 1, priority_dragon_power, -4);
		if (event == EVENT_CAN_ACTIVATE){
			if (!choice)
				return 0;	// else fall through to planeswalker()
		}

		else if (event == EVENT_ACTIVATE){
				instance->number_of_targets = 0;
				if( choice == CHOICE_SAC_TO_DRAGON ){
					if( player == AI ){
						if( priority_opp_sac > priority_you_sac ){
							pick_target(&td3, "TARGET_CREATURE");
						}
						else{
							pick_target(&td2, "TARGET_CREATURE");
						}
					}
					else{
						pick_target(&td, "TARGET_CREATURE");
					}
				}
				if( choice == CHOICE_DRAGON_POWER ){
						pick_target(&td4, "TARGET_PLAYER_OR_PLANESWALKER");
				}
		}

		else{	// event == EVENT_RESOLVE_ACTIVATION
			if( choice == CHOICE_REVEAL ){
				int *deck = deck_ptr[player];
				if( deck[0] != -1 ){
					show_deck( 1-player, deck, 1, "Sarkhan the Mad revealed...", 0, 0x7375B0 );
					int card_added = add_card_to_hand(player, deck[0]);
					remove_card_from_deck(player, 0);
					remove_counters(instance->parent_controller, instance->parent_card, COUNTER_LOYALTY, get_cmc(player, card_added));
				}
			}
			else if( choice == CHOICE_SAC_TO_DRAGON ){
					if( valid_target(&td) && can_sacrifice(player, instance->targets[0].player, 1, TYPE_CREATURE, 0) ){
						kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);

						token_generation_t token;
						default_token_definition(player, card, CARD_ID_DRAGON, &token);
						token.t_player = instance->targets[0].player;
						token.color_forced = COLOR_TEST_RED;
						token.pow = 5;
						token.tou = 5;
						generate_token(&token);
					}
			}
			else if( choice == CHOICE_DRAGON_POWER ){
					int c;
					for(c=active_cards_count[player]-1; c > -1; c--){
						if( in_play(player, c) && new_make_test_in_play(player, c, -1, &this_test) ){
							if( valid_target(&td4) ){
								damage_target0(player, card, get_power(player, c));
								get_card_instance(player, card)->number_of_targets = 0;
							}
						}
					}
			}
		}
	}
	return planeswalker(player, card, event, 7);
}

/***********
* Artifact *
***********/

/* Angelheart Vial	|5
 * Artifact
 * Whenever you're dealt damage, you may put that many charge counters on ~.
 * |2, |T, Remove four charge counters from ~: You gain 2 life and draw a card. */

int card_dreamstone_hedron(int player, int card, event_t event){
/*
Dreamstone Hedron |6
Artifact
{T}: Add {C}{C}{C} to your mana pool.
{3}, {T}, Sacrifice Dreamstone Hedron: Draw three cards.
*/

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

	if( in_play(player, card) && is_equipping(player, card) && ! is_humiliated(player, card) ){
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
		&& ! is_humiliated(player, card)
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

  if (comes_into_play(player, card, event))
	draw_a_card(player);

  // |1, |T: Add one mana of any color to your mana pool.
  if (event == EVENT_CAN_ACTIVATE)
	return (can_produce_mana(player, card) && !is_tapped(player, card) && !is_animated_and_sick(player, card)
			&& has_mana(player, COLOR_COLORLESS, 1) && get_card_instance(player, card)->targets[1].player != 66);

  if (event == EVENT_ACTIVATE)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  instance->targets[1].player = 66;
	  charge_mana(player, COLOR_COLORLESS, 1);
	  instance->targets[1].player = 0;
	  if (spell_fizzled != 1)
		produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1);
	}

  return 0;
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
	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_SPELL);
		if( new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_AI(player), &test) ){
			add_counter(player, card, COUNTER_CHARGE);

			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
			td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			card_instance_t *instance = get_card_instance(player, card);

			if( can_target(&td) && new_pick_target(&td, "TARGET_ANY", 0, 0) ){
				damage_target0(player, card, count_counters(player, card, COUNTER_CHARGE));
			}
			instance->number_of_targets = 0;
		}
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
		! is_humiliated(player, card) &&
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

	if( in_play(player, card) && ! is_humiliated(player, card) ){
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


