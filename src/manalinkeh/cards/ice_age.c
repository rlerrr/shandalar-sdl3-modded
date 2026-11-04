#include "manalink.h"

static int effect_tap_when_lose_control(int player, int card, event_t event);

/* Canonical recovered helper declarations. */
void oath_of_lim_dul_lifeloss_trigger(int player, int card, int amount);
int control_a_snow_land(int player, int must_be_untapped);

// --- FUNCTIONS FOR SNOW PERMANENTS

int is_snow_permanent(int player, int card){

	if( has_subtype(player, card, SUBTYPE_SNOW) ){
		return 1;
	}

	return 0;
}

int count_snow_permanents(int what_player, int type, int must_be_untapped){
  // what_player = 2 -> count the permanents of both players

  int result = 0;

  if( what_player == 2 ){
	 int i;
	 for(i = 0; i < 2; i++){
		 int count = active_cards_count[i]-1;
		 while( count > -1 ){
			   if( in_play(i, count) && is_what(i, count, type) && is_snow_permanent(i, count) ){
				  if( must_be_untapped == 1 ){
					 if( ! is_tapped(i, count) ){
						result++;
					 }
				  }
				  else{
					   result++;
				  }
			   }
			   count--;
		 }
	 }
  }

  else if ( what_player == 1 || what_player == 0){
		   int count = active_cards_count[what_player]-1;
		   while( count > -1 ){
				 if( in_play(what_player, count) && is_what(what_player, count, type) &&
					 is_snow_permanent(what_player, count) ){
					 if( must_be_untapped == 1 ){
						if( ! is_tapped(what_player, count) ){
						   result++;
						}
					 }
					 else{
						  result++;
					 }
				 }
				 count--;
		   }
  }

  return result;
}

int control_a_snow_land(int player, int must_be_untapped)
{
  card_instance_t *instance;
  int result;
  int card;

  card = active_cards_count[player] + -1;
  if (-1 < card)
  {
    do
    {
      instance = in_play(player, card);
      if ((((instance != NULL) && (result = is_what(player, card, 1), result != 0)) &&
           (result = has_subtype(player, card, SUBTYPE_SNOW), result != 0)) &&
          ((must_be_untapped != 1 || (result = is_tapped(player, card), result == 0))))
      {
        return 1;
      }
      card = card + -1;
    } while (card != -1);
  }
  return 0;
}

static int count_snow_landtype(int player, subtype_t subt)
{
  int count = 0, p, c;
  for (p = 0; p <= 1; ++p)
	if (p == player || player == ANYBODY)
	  for (c = 0; c < active_cards_count[p]; ++c)
		if (in_play(p, c) && is_what(p, c, TYPE_LAND) && is_snow_permanent(p, c) && has_subtype(p, c, subt))
		  ++count;
  return count;
}

// Inaccurate: it assumes each mana source produces exactly one mana.
// We need to be able to reliably interpret declare_mana_available() and declare_mana_available_hex()'s results in order to fix.
int has_snow_mana(int player, int snow, int cless)
{
  card_instance_t* instance;
  int c;
  for (c = 0; c < active_cards_count[player]; ++c)
	if ((instance = in_play(player, c)) && (cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE))
	  {
		if (snow > 0 && has_subtype(player, c, SUBTYPE_SNOW))
		  {
			if (call_card_function_i(instance, player, c, EVENT_CAN_ACTIVATE))
			  --snow;
		  }
		else if (cless > 0)
		  {
			if (call_card_function_i(instance, player, c, EVENT_CAN_ACTIVATE))
			  --cless;
		  }

		if (snow <= 0 && cless <= 0)
		  return 1;
	  }
  return 0;
}

/* Inaccurate: it counts all mana produced when the land is activated, including triggered abilities like Mana Flare.  So with a Mana Flare and a Snow-covered
 * Forest, you produce {SG}{SG}; with a snow Mana Flare and a Forest, you produce {G}{G}.  It should be {G}{SG} in both cases.  Plus, you can't use mana already
 * produced from snow sources before this is charged. */
static const char* target_is_activateable_snow_mana_source(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (has_subtype(player, card, SUBTYPE_SNOW))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if ((cards_data[instance->internal_card_id].extra_ability & EA_MANA_SOURCE) && call_card_function_i(instance, player, card, EVENT_CAN_ACTIVATE))
		return NULL;
	}
  return "must be an activateable Snow mana source";
}
int charge_snow_mana(int player, int card, int snow, int cless)
{
  target_definition_t td;
  base_target_definition(player, card, &td, TYPE_PERMANENT);
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.extra = target_predicate_extra(target_is_activateable_snow_mana_source);
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;

  char intro[300], mana_prompt[100], tap_prompt[150], prompt[450];

  // Construct beginning of prompt
  if (stack_size > 0 && ai_is_speculating != 1)
	format_stack_action_text(intro, stack_data[stack_size - 1].generating_event,
												 stack_cards[stack_size - 1].player, stack_cards[stack_size - 1].card);
  else
	*intro = 0;

  *prompt = 0;

  int mana_spent[8] = {0};
  while (snow > 0)
	{
	  // Construct rest of prompt
	  if (ai_is_speculating != 1)
		{
		  char* p = mana_prompt;
		  int i;
		  if (cless > 0)
			{
			  i = cless;
			  if (i > 16)
				while (i >= 10)
				  {
					p += sprintf(p, "|10");
					i -= 10;
				  }

			  if (i)
				p += sprintf(p, "|%d", i);
			}

		  for (i = 0; i < snow; ++i)
			{
			  *p++ = '|';
			  *p++ = 'I';
			}
		  *p = 0;

		  scnprintf(tap_prompt, 150, gs_prompt_grabmana_0/*PROMPT_GRABMANA[0]*/, mana_prompt);
		  scnprintf(prompt, 450, "%s, %s", intro, tap_prompt);
		}

	  // Select a snow mana source to activate
	  target_t tgt;
	  if (!select_target(player, card-1000, &td, prompt, &tgt))
		{
		  cancel = 1;
		  int i;
		  for (i = COLOR_COLORLESS; i <= COLOR_WHITE; ++i)
			{
			  mana_pool[player][i] += mana_spent[i];
			  mana_pool[player][COLOR_ANY] += mana_spent[i];
			}
		  return 0;
		}

	  // Back up mana pool
	  int mana_pool_start[8];
	  memcpy(mana_pool_start, mana_pool[player], sizeof(mana_pool_start));

	  // Activate it
	  force_activation_for_mana(tgt.player, tgt.card, COLOR_TEST_COLORLESS);

	  // Inspect mana pool for what changed
	  int i;
	  for (i = COLOR_COLORLESS; i <= COLOR_WHITE && snow > 0; ++i)
		{
		  int generated = mana_pool[player][i] - mana_pool_start[i];
		  if (generated > 0)
			{
			  int amt_to_pay = MIN(snow, generated);
			  mana_pool[player][i] -= amt_to_pay;
			  mana_pool[player][COLOR_ANY] -= amt_to_pay;
			  mana_spent[i] += amt_to_pay;
			  snow -= amt_to_pay;
			}
		}
	}

  // Snow mana all paid.
  if (cless > 0)
	{
	  charge_mana(player, COLOR_COLORLESS, cless);
	  if (cancel == 1)
		{
		  int i;
		  for (i = COLOR_COLORLESS; i <= COLOR_WHITE; ++i)
			{
			  mana_pool[player][i] += mana_spent[i];
			  mana_pool[player][COLOR_ANY] += mana_spent[i];
			}
		  return 0;
		}
	}

  return 1;
}

int cantrip_legacy(int player, int card, event_t event){
	card_instance_t *instance = get_card_instance(player, card);
	if( upkeep_trigger_no_check(player, card, event) ){
		int count = count_upkeeps(current_turn);
		if ( count > 0 ){
			draw_cards(instance->targets[1].player, instance->targets[1].card);
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int cantrip(int player, int card, int amount){
	// "You draw a card at the beginning of the next turn's upkeep."
	int legacy = create_legacy_effect(player, card, &cantrip_legacy);
	card_instance_t *instance = get_card_instance(player, legacy);
	instance->targets[1].player = player;
	instance->targets[1].card = amount;
	if( current_phase == PHASE_UPKEEP ){
		remove_state(player, legacy, STATE_PROCESSING);
	}
	return legacy;
}

static int scarab(int player, int card, event_t event, color_test_t clr)
{
  card_instance_t* instance = get_card_instance(player, card);
  int tp = instance->damage_target_player, tc = instance->damage_target_card;

  if (in_play(player, card) && tp >= 0 && tc >= 0)
	{
	  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(tp, tc))
		{
		  clr = get_sleighted_color_test(player, card, clr);
		  int c;
		  for (c = 0; c < active_cards_count[1-player]; ++c)
			if (in_play(1-player, c) && is_what(1-player, c, TYPE_PERMANENT) && (get_color(1-player, c) & clr))
			  {
				event_result += 2;
				return 0;
			  }
		}

	  if (event == EVENT_BLOCK_LEGALITY && tp == attacking_card_controller && tc == attacking_card
		  && (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, clr)))
		event_result = 1;
	}

  return vanilla_aura(player, card, event, player);
}

static int ia_land_destructor(int player, int card, event_t event, int effect){

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int do_effect = is_snow_permanent(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

			if( do_effect == 1 && effect == 0 ){
				damage_player(instance->targets[0].player, 1, player, card);
			}

			if( do_effect == 1 && effect == 1 ){
				gain_life(player, 1);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}


	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_LAND", 1, NULL);
}

static int ia_blast(int player, int card, event_t event, int color)
{
/* CARD_ID_HYDROBLAST	1971
Hydroblast	|U
Instant
Choose one -
* Counter target spell if it's |Sred.
* Destroy target permanent if it's |Sred. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td_permanent;
  default_target_definition(player, card, &td_permanent, TYPE_PERMANENT);

  target_definition_t td_spell;
  counterspell_target_definition(player, card, &td_spell, TYPE_ANY);

  if (event == EVENT_CAN_CAST)
	{
	  if (counterspell(player, card, event, &td_spell, 0))
		return 99;
	  return generic_spell(player, card, event, GS_CAN_TARGET, &td_permanent, "TARGET_PERMANENT", 1, NULL);
	}

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (counterspell(player, card, EVENT_CAN_CAST, &td_spell, 0))
		{
		  inst->info_slot = 66;
		  return counterspell(player, card, event, &td_spell, 0);
		}
	  else
		{
		  generic_spell(player, card, event, GS_CAN_TARGET, &td_permanent, "TARGET_PERMANENT", 1, NULL);
		  inst->info_slot = 67;
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  td_permanent.required_color = td_spell.required_color = get_sleighted_color_test(player, card, 1<<color);

	  if (inst->info_slot == 66 && counterspell_validate(player, card, &td_spell, 0))
		real_counter_a_spell(player, card, inst->targets[0].player, inst->targets[0].card);

	  if (inst->info_slot == 67 && valid_target(&td_permanent))
		kill_card(inst->targets[0].player, inst->targets[0].card, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

static int talisman(int player, int card, event_t event, int color)
{
  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) &&
      reason_for_trigger_controller == player)
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_ANY);
    test.color = get_sleighted_color_test(player, card, 1 << color);
    sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_PERMANENT);
    if (player == AI && IS_AI(player))
    {
      td.required_state |= TARGET_STATE_TAPPED;
      td.allowed_controller = player;
    }
    char buffer[100], color_text[100];
    scnprintf(buffer, 100, "Select target %s permanent.",
              get_color_text(player, color_text, 1 << color));
    if (can_target(&td) &&
        charge_mana_while_resolving(player, card, event, player, COLOR_COLORLESS, 3) &&
        new_pick_target(&td, buffer, 0, GS_LITERAL_PROMPT))
    {
      card_instance_t *instance = get_card_instance(player, card);
      untap_card(instance->targets[0].player, instance->targets[0].card);
      instance->number_of_targets = 0;
    }
  }
  return 0;
}

//--------- CARDS

int card_abyssal_specter(int player, int card, event_t event)
{
	/* Abyssal Specter	|2|B|B	0x200b2a4
	 * Creature - Specter 2/3
	 * Flying
	 * Whenever ~ deals damage to a player, that player discards a card. */

  whenever_i_deal_damage_to_a_player_he_discards_a_card(player, card, event, 0, 0);
  return 0;
}

int card_adarkar_sentinel(int player, int card, event_t event)
{
	/* Adarkar Sentinel	|5	0x200b533
	 * Artifact Creature - Soldier 3/3
	 * |1: ~ gets +0/+1 until end of turn. */

  return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1), 0, 1);
}

/* Adarkar Unicorn	|1|W|W	0x000000
 * Creature - Unicorn 2/2
 * |T: Add |U or |1|U to your mana pool. Spend this mana only to pay cumulative upkeep costs. */

/* Adarkar Wastes	""	=>functions.c:generic_painland
 * Land
 * |T: Add |1 to your mana pool.
 * |T: Add |W or |U to your mana pool. ~ deals 1 damage to you. */

int card_aegis_of_the_meek(int player, int card, event_t event){
	/* Aegis of the Meek	|3	0x2009ce2
	 * Artifact
	 * |1, |T: Target 1/1 creature gets +1/+2 until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.power_requirement = 1;
	td.toughness_requirement = 1;

	return vanilla_creature_pumper(player, card, event, MANACOST_X(1), GAA_UNTAPPED, 1, 2, 0, 0, &td);
}

int card_aggression(int player, int card, event_t event)
{
	/* Aggression	|2|R	0x200b538
	 * Enchantment - Aura
	 * Enchant non-Wall creature
	 * Enchanted creature has first strike and trample.
	 * At the beginning of the end step of enchanted creature's controller, destroy that creature if it didn't attack this turn. */

  if (in_play(player, card))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int tp = instance->damage_target_player, tc = instance->damage_target_card;
	  if (tp >= 0 && tc >= 0)
		{
		  if (event == EVENT_ABILITIES && affect_me(tp, tc))
			event_result |= KEYWORD_FIRST_STRIKE | KEYWORD_TRAMPLE;

		  if (current_turn == tp && !(get_card_instance(tp, tc)->state & (STATE_ATTACKING | STATE_ATTACKED))
			  && (eot_trigger(player, card, event) || event == EVENT_SHOULD_AI_PLAY))
			kill_card(tp, tc, KILL_DESTROY);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;
	td.required_subtype = SUBTYPE_WALL;
	td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;

	return targeted_aura(player, card, event, &td, "TARGET_NONWALL_CREATURE");
}

int card_altar_of_bone(int player, int card, event_t event){
	/* Altar of Bone	|G|W	0x2002D93
	 * Sorcery
	 * As an additional cost to cast ~, sacrifice a creature.
	 * Search your library for a creature card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_SAC_CREATURE_AS_COST, NULL, NULL, 0, NULL);
}

int card_amulet_of_quoz(int player, int card, event_t event)
{
	/* Amulet of Quoz	|6	0x200b53d
	 * Artifact
	 * Remove ~ from your deck before playing if you're not playing for ante.
	 * |T, Sacrifice ~: Target opponent may ante the top card of his or her library. If he or she doesn't, you flip a coin. If you win the flip, that player loses the game. If you lose the flip, you lose the game. Activate this ability only during your upkeep. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int tgt = instance->targets[0].player;
	  if (DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_WHO_CHOOSES(tgt),
				 "Ante top card of library", can_ante_top_card_of_library(tgt), life[tgt] > life[1-tgt] ? 3 : 1,
				 "Flip a coin", 1, 2) == 1)
		ante_top_card_of_library(tgt);
	  else if (flip_a_coin(player, card))
		lose_the_game(1 - player);
	  else
		lose_the_game(player);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME|GAA_CAN_ONLY_TARGET_OPPONENT|GAA_ONLY_ON_UPKEEP|GAA_IN_YOUR_TURN, MANACOST_X(0), 0, NULL, NULL);
}

int card_anarchy(int player, int card, event_t event){
	/* Anarchy	|2|R|R	0x200b542
	 * Sorcery
	 * Destroy all |Swhite permanents. */

	if (event == EVENT_RESOLVE_SPELL){
		test_definition_t test;
		default_test_definition(&test, TYPE_PERMANENT);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);

		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_arctic_foxes(int player, int card, event_t event)
{
	/* Arctic Foxes	|1|W	0x200b547
	 * Creature - Fox 1/1
	 * ~ can't be blocked by creatures with power 2 or greater as long as defending player controls a snow land. */

  if (event == EVENT_BLOCK_LEGALITY
	  && player == attacking_card_controller && card == attacking_card
	  && get_power(affected_card_controller, affected_card) >= 2
	  && control_a_snow_land(1-current_turn, 0)
	  && !is_humiliated(player, card, AL_STATIC))
	event_result = 1;

  return 0;
}

static int effect_arcums_whistle(int player, int card, event_t event)
{
  event_flags |= EA_FORCE_ATTACK;

  card_instance_t* instance = get_card_instance(player, card);
  int tp = instance->damage_target_player, tc = instance->damage_target_card;

  attack_if_able(tp, tc, event);

  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	{
	  if (in_play(tp, tc)
		  && !(get_card_instance(tp, tc)->state & (STATE_ATTACKING | STATE_ATTACKED)))
		kill_card(tp, tc, KILL_DESTROY);

	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

/* Arcum's Weathervane	|2	0x000000
 * Artifact
 * |2, |T: Target snow land is no longer snow.
 * |2, |T: Target nonsnow basic land becomes snow. */

int card_arcums_whistle(int player, int card, event_t event)
{
	/* Arcum's Whistle	|3	0x200b54c
	 * Artifact
	 * |3, |T: Choose target non-Wall creature the active player has controlled continuously since the beginning of the turn. That player may pay |X, where X is that creature's converted mana cost. If he or she doesn't, the creature attacks this turn if able, and at the beginning of the next end step, destroy it if it didn't attack. Activate this ability only before attackers are declared. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = current_turn;
  td.preferred_controller = 1-player;
  td.required_subtype = SUBTYPE_WALL;
  td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  td.illegal_state = TARGET_STATE_SUMMONING_SICK;

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
	  card_instance_t* instance = get_card_instance(player, card);

	  int cmc = get_cmc(instance->targets[0].player, instance->targets[0].card);
	  int tgt = instance->targets[0].player;
	  char buf[20];sprintf(buf, "Pay %d", cmc);
	  if (DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_WHO_CHOOSES(tgt),
				 "Pay CMC", has_mana(tgt, COLOR_COLORLESS, cmc), 2,
				 "Pass", 1, 1) == 2
		  || !charge_mana_multi(tgt, MANACOST_X(cmc)))
		create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_arcums_whistle,
									   instance->targets[0].player, instance->targets[0].card);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_BEFORE_ATTACKERS, MANACOST_X(3), 0, &td, "TARGET_NONWALL_CREATURE");
}

int card_arensons_aura(int player, int card, event_t event){
  /* Arenson's Aura	|2|W	0x2002D98
   * Enchantment
   * |W, Sacrifice an enchantment: Destroy target enchantment.
   * |3|U|U: Counter target enchantment spell. */

 //share code with Teferi's Care.


	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ENCHANTMENT );

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_ENCHANTMENT );
	
	test_definition_t test;
	new_default_test_definition(&test, TYPE_ENCHANTMENT, "Select an enchantment to sacrifice.");

	enum{
		CHOICE_SAC_AND_KILL_ENCHANTMENT = 1,
		CHOICE_COUNTER_ENCHANTMENT_SPELL
	};

	if(event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_XU(3, 2), 0, &td, NULL) ){
			return 99;
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_W(1), 0, &td1, NULL,
																TYPE_ENCHANTMENT, NULL, 0);
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = CHOICE_SAC_AND_KILL_ENCHANTMENT;
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SPELL_ON_STACK, MANACOST_XU(3, 2), 0, &td, NULL) ){
			instance->info_slot = CHOICE_COUNTER_ENCHANTMENT_SPELL;
			return generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK, MANACOST_XU(3, 2), 0, &td, NULL);
		}
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_W(1), 0, &td1, "TARGET_ENCHANTMENT",
																0, &test, 0);
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( instance->info_slot == CHOICE_SAC_AND_KILL_ENCHANTMENT && valid_target(&td1) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		if( instance->info_slot == CHOICE_COUNTER_ENCHANTMENT_SPELL ){
			if( counterspell_validate(player, card, &td, 0) ){
				real_counter_a_spell(instance->parent_controller, instance->parent_card,
									instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_armor_of_faith(int player, int card, event_t event)
{
	/* Armor of Faith	|W	0x200b551
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature gets +1/+1.
	 * |W: Enchanted creature gets +0/+1 until end of turn. */

	if (event == EVENT_RESOLVE_ACTIVATION)
	{
		card_instance_t* instance = get_card_instance(player, card);
		pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, instance->damage_target_player, instance->damage_target_card, 0, 1);
	}

  return generic_aura(player, card, event, player, 1, 1, 0, 0, -1, 0, 0) || generic_activated_ability(player, card, event, 0, MANACOST_W(1), 0, NULL, NULL);
}

int card_arnjlots_ascent(int player, int card, event_t event)
{
	/* Arnjlot's Ascent	|1|U|U	0x200b556
	 * Enchantment
	 * Cumulative upkeep |U
	 * |1: Target creature gains flying until end of turn. */

	cumulative_upkeep(player, card, event, MANACOST_U(1));

	if (!IS_GAA_EVENT(event))
		return global_enchantment(player, card, event);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return vanilla_creature_pumper(player, card, event, MANACOST_X(1), 0, 0, 0, KEYWORD_FLYING, 0, &td);
}

int card_ashen_ghoul(int player, int card, event_t event){
	/* Ashen Ghoul	|3|B	0x200210E
	 * Creature - Zombie 3/1
	 * Haste
	 * |B: Return ~ from your graveyard to the battlefield. Activate this ability only during your upkeep and only if three or more creature cards are above ~. */

	haste(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		int ag_pos = 0;
		int victims = 0;
		int i;
		for(i=0; i<count_graveyard(player); i++){
			if( cards_data[get_grave(player)[i]].id == CARD_ID_ASHEN_GHOUL ){
				ag_pos = i;
				break;
			}
		}
		for(i=ag_pos+1; i<count_graveyard(player); i++){
			if( is_what(-1, get_grave(player)[i], TYPE_CREATURE) ){
				victims++;
			}
		}
		if( victims >= 3){
			int choice = do_dialog(player, player, card, -1, -1," Return Ashen Ghoul\n Do not return Ashen Ghoul", 0);
			if( choice == 0 ){
				charge_mana_multi(player, MANACOST_B(1));
				if( spell_fizzled != 1 ){
					put_into_play(player, card);
					return -1;
				}
			}
		}
		return -2;
	}

	return 0;
}

/* Aurochs	|3|G	=>coldsnap.c:card_bull_aurochs
 * Creature - Aurochs 2/3
 * Trample
 * Whenever ~ attacks, it gets +1/+0 until end of turn for each other attacking ~. */

int card_avalanche(int player, int card, event_t event){
	/* Avalanche	|X|2|R|R	0x200b560
	 * Sorcery
	 * Destroy X target snow lands. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_SNOW;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST)
		return generic_spell(player, card, event, GS_X_SPELL+GS_CAN_TARGET, &td, "TARGET_SNOW_LAND", 0, NULL);

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		instance->number_of_targets = 0;
		int i = 0;
		while( can_target(&td) && has_mana(player, COLOR_COLORLESS, i) ){
				if(new_pick_target(&td, "TARGET_SNOW_LAND", i, 0) ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
					i++;
				}
				else{
					break;
				}
		}
		remove_untargettable_from_all();
		if( i < 1 ){
			spell_fizzled = 1;
			return 0;
		}
		charge_mana(player, COLOR_COLORLESS, i);
	}

	if (event == EVENT_RESOLVE_SPELL){
		int i;
		for (i = 0; i < instance->number_of_targets; ++i){
			if (validate_target(player, card, &td, i)){
				kill_card(instance->targets[i].player, instance->targets[i].card, KILL_DESTROY);
			}
		}

		kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

/* Balduvian Barbarians	|1|R|R	=>vanilla
 * Creature - Human Barbarian 3/2 */

/* Balduvian Bears	|1|G	=>vanilla
 * Creature - Bear 2/2 */

int card_balduvian_conjurer(int player, int card, event_t event)
{
	/* Balduvian Conjurer	|1|U	0x200b565
	 * Creature - Human Wizard 0/2
	 * |T: Target snow land becomes a 2/2 creature until end of turn. It's still a land. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.preferred_controller = player;
  td.required_subtype = SUBTYPE_SNOW;
  if (player == AI)
	td.illegal_type = TYPE_CREATURE;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  land_animation2(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, 2, 2, 0, 0, 0, NULL);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(0), 0, &td, "TARGET_SNOW_LAND");
}

static int balduvian_hydra_player = 0, balduvian_hydra_card = 0;
/* ManalinkEh 2022: 0x23ef140; original cards/ice_age.c:588. */
int card_arcums_sleigh(int player, int card, event_t event)
{
  /* Arcum's Sleigh	|1	0x000000
   * Artifact
   * |2, |T: Target creature gains vigilance until end of turn. Activate this ability only during combat and only if defending player controls a snow land. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  result = beginning_of_combat(player, card, event, current_turn);
  if ((((((result != 0) && (result = can_use_activated_abilities(player, card), result != 0)) &&
         (result = has_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0), result != 0)) &&
        ((result = is_tapped(player, card),
          result == 0 && (result = is_animated_and_sick(player, card), result == 0)))) &&
       ((result = control_a_snow_land(1 - current_turn, 0),
         result != 0 &&
             ((result = can_target(&td),
               result != 0 &&
                   (result = new_pick_target(
                        &td, "Arcum\'s Sleigh: activate now to select untapped target creature.", 0,
                        0x80),
                    result != 0)))))) &&
      (result = charge_mana_for_activated_ability(player, card, 2, 0, 0, 0, 0, 0), result != 0))
  {
    add_state(player, card, 0x10);
    tapped_for_mana_color = -1;
    pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0,
                           0, 0, 8);
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (((current_phase - 0x16U & 0xfffffffd) == 0) &&
        (result = control_a_snow_land(1 - current_turn, 0), result != 0))
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 2, 0, 0, 0, 0, 0, 0,
                                         &td, "TARGET_CREATURE");
      return result;
    }
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 2, 0, 0, 0, 0, 0, 0, &td,
                                         "TARGET_CREATURE");
      return result;
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             instance->targets[0].player, instance->targets[0].card, 0, 0, 0, 8);
      return 0;
    }
  }
  return 0;
}

static const char* target_is_damaging_balduvian_hydra(int who_chooses, int player, int card)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (instance->internal_card_id == damage_card
	  && instance->damage_target_player == balduvian_hydra_player
	  && instance->damage_target_card == balduvian_hydra_card
	  && !(instance->state & STATE_TAPPED)
	  && instance->info_slot > 0)
	return NULL;
  else
	return "damage dealt to Balduvian Hydra";
}

int card_balduvian_hydra(int player, int card, event_t event)
{
  /* Balduvian Hydra	|X|R|R	0x200b56a
   * Creature - Hydra 0/1
   * ~ enters the battlefield with X +1/+0 counters on it.
   * Remove a +1/+0 counter from ~: Prevent the next 1 damage that would be dealt to ~ this turn.
   * |R|R|R: Put a +1/+0 counter on ~. Activate this ability only during your upkeep. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        (generic_spell(player, card, EVENT_CAST_SPELL, 4, NULL, NULL, 0, NULL), player == 1))
    {
      instance = get_card_instance(1, card);
      ai_modifier = ai_modifier + (instance->info_slot * 3 + -9) * 8;
    }
    instance = get_card_instance(player, card);
    enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_P1_P0,
                                         instance->info_slot);
  }
  else
  {
    instance = get_card_instance(player, card);
    enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P0, instance->info_slot);
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      default_target_definition(player, card, &td, 0);
      td.extra = (int32_t)target_is_damaging_balduvian_hydra;
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      balduvian_hydra_player = player;
      balduvian_hydra_card = card;
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        result = can_use_activated_abilities(player, card);
        if (result == 0)
        {
          return 0;
        }
        if (((((unsigned char)land_can_be_played & 4) != 0) &&
             (result = count_counters(player, card, COUNTER_P1_P0), 0 < result)) &&
            ((result = can_target(&td), result != 0 && (result = has_mana_for_activated_ability(
                                                            player, card, 0, 0, 0, 0, 0, 0),
                                                        result != 0))))
        {
          return 99;
        }
        if (current_phase != 4)
        {
          return 0;
        }
        if (current_turn != player)
        {
          return 0;
        }
        result = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 3, 0);
        if (result == 0)
        {
          return 0;
        }
        if (player == 1)
        {
          if (2 < hand_count[1] - landsofcolor_controlled[1][4])
          {
            g_activation_event_flags = g_activation_event_flags | 3;
            return 1;
          }
          return 1;
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
      default_target_definition(player, card, &td, 0);
      td.extra = (int32_t)target_is_damaging_balduvian_hydra;
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      balduvian_hydra_player = player;
      balduvian_hydra_card = card;
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      if (((unsigned char)land_can_be_played & 4) == 0)
      {
        charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 3, 0);
      }
      else
      {
        result = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
        if ((result != 0) &&
            (result = pick_next_target_noload(&td, "Select damage dealt to Balduvian Hydra"),
             result != 0))
        {
          remove_counters(player, card, player, card, COUNTER_P1_P0, 1);
        }
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      if (instance->number_of_targets == '\0')
      {
        add_counters(player, card, player, card, COUNTER_P1_P0, 1);
        return 0;
      }
      balduvian_hydra_player = instance->parent_controller;
      balduvian_hydra_card = instance->parent_card;
      result = valid_target(&td);
      if (result != 0)
      {
        prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23eb9d0; original cards/ice_age.c:997. */
static char *doesnt_have_cumulative_upkeep(int who_chooses, int player, int card)
{
  int result;

  result = dispatch_event(player, card, 0x5900);
  if (result != 0)
  {
    return "must not have cumulative upkeep";
  }
  return NULL;
}

/* ManalinkEh 2022: 0x23ed800; original cards/ice_age.c:1004. */
static int effect_basha(int player, int card, event_t event)
{
  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (instance->damage_target_player < '\0')
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  if (((event == EVENT_QUERY_CUMULATIVE_UPKEEP) &&
       (instance->damage_target_card == affected_card)) &&
      (instance->damage_target_player == affected_card_controller))
  {
    event_result = event_result | 1;
  }
  cumulative_upkeep_arbitrary(player, card, (int)instance->damage_target_player,
                              instance->damage_target_card, event, 1, 0, 0, 0, 0, 0);
  return 0;
}

static int effect_snow_landwalk_until_eot_then_bounce(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  int tp = instance->damage_target_player, tc = instance->damage_target_card;

  if (event == EVENT_SET_LEGACY_EFFECT_NAME)
	scnprintf(set_legacy_effect_name_addr, 52, get_hacked_land_text(-1, -1, "Snow %lwalk", instance->targets[1].player));

  if (event == EVENT_SET_LEGACY_EFFECT_TEXT)
	scnprintf(set_legacy_effect_name_addr, 400, get_hacked_land_text(-1, -1, "Target creature you control gains snow %lwalk until end of turn. "
																	 "Return that creature to its owner's hand at the beginning of the next end step.",
																	 instance->targets[1].player));

  if (event == EVENT_BLOCK_LEGALITY
	  && tp == attacking_card_controller && tc == attacking_card
	  && !is_humiliated(tp, tc, AL_STATIC))
	{
	  int i;
	  for (i = 0; i < active_cards_count[1-tp]; ++i)
		if (in_play(1-tp, i) && is_what(1-tp, i, TYPE_LAND)
			&& has_subtype(1-tp, i, SUBTYPE_SNOW) && has_subtype(1-tp, i, instance->targets[1].player))
		  {
			event_result = 1;
			return 0;
		  }
	}

	if( eot_trigger(player, card, event)){
		if( in_play(tp, tc) ){
			bounce_permanent(tp, tc);
		}
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_barbarian_guides(int player, int card, event_t event)
{
	/* Barbarian Guides	|2|R	0x200b56f
	 * Creature - Human Barbarian 1/2
	 * |2|R, |T: Choose a land type. Target creature you control gains snow landwalk of the chosen type until end of turn. Return that creature to its owner's hand at the beginning of the next end step. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = player;
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  int land_subtypes[6] = { 0, SUBTYPE_SWAMP, SUBTYPE_ISLAND, SUBTYPE_FOREST, SUBTYPE_MOUNTAIN, SUBTYPE_PLAINS };
	  int choice = 0;
	  if (IS_AI(player))
		{
		  int i, j;
		  for (i = 0; i < active_cards_count[1-player]; ++i)
			if (in_play(1-player, i) && is_what(1-player, i, TYPE_LAND) && has_subtype(1-player, i, SUBTYPE_SNOW))
			  for (j = 0; j < 5; ++j)
				if (has_subtype(1-player, i, land_subtypes[j]))
				  {
					choice = j;
					goto break2;
				  }
		break2:;
		}

	  choice = choose_a_color_exe(player, "Snow land type?", 0, choice, COLOR_TEST_ANY_COLORED);
	  if (choice < 0)	// tried to cancel, but it's too late to honor that
		choice = 0;

	  card_instance_t* instance = get_card_instance(player, card);
	  int leg = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_snow_landwalk_until_eot_then_bounce,
											   instance->targets[0].player, instance->targets[0].card);
	  if (leg != -1)
		{
		  card_instance_t* legacy = get_card_instance(instance->parent_controller, leg);
		  legacy->targets[1].player = land_subtypes[choice];
		  legacy->eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME | EVENT_SET_LEGACY_EFFECT_TEXT;
		}
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XR(2,1), 0, &td, "TARGET_CREATURE");
}

int card_barbed_sextant(int player, int card, event_t event)
{
	/* Barbed Sextant	|1	0x200b574
	 * Artifact
	 * |1, |T, Sacrifice ~: Add one mana of any color to your mana pool. Draw a card at the beginning of the next turn's upkeep. */

	if (event == EVENT_COUNT_MANA && affect_me(player, card)
	    && !is_tapped(player, card) && !is_animated_and_sick(player, card)
	    && can_produce_mana(player, card) && can_sacrifice_this_as_cost(player, card)
	    && has_mana_for_activated_ability(player, card, MANACOST_X(1))){
	    declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, 1);
	    declare_mana_available(player, COLOR_COLORLESS, -1);
	}
	if (event == EVENT_CAN_ACTIVATE){
	    return !is_tapped(player, card) && !is_animated_and_sick(player, card)
	        && can_produce_mana(player, card)
	        && has_mana_for_activated_ability(player, card, MANACOST_X(1))
	        && get_card_instance(player, card)->targets[1].player != 1
	        && can_sacrifice_this_as_cost(player, card);
	}
	if (event == EVENT_ACTIVATE){
	    card_instance_t* instance = get_card_instance(player, card);
	    color_t color = choose_mana_all_one_color(player, COLOR_TEST_ANY_COLORED, 1, "What kind of mana?", COLOR_TEST_COLORLESS);
	    if (color == -1){ spell_fizzled = 1; return 0; }
	    instance->targets[1].player = 1;
	    charge_mana_for_activated_ability(player, card, MANACOST_X(1));
	    instance->targets[1].player = 0;
	    if (spell_fizzled != 1){
	        kill_card(player, card, KILL_SACRIFICE);
	        produce_mana_tapped(player, card, color, 1);
	        cantrip(player, card, 1);
	    }
	}
	return 0;
}

int card_baton_of_morale(int player, int card, event_t event){
	/* Baton of Morale	|2	0x2009ce7
	 * Artifact
	 * |2: Target creature gains banding until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	return vanilla_creature_pumper(player, card, event, MANACOST_X(2), GAA_UNTAPPED | GAA_NONSICK, 0, 0, KEYWORD_BANDING, 0, &td);
}

static int effect_battle_cry(int player, int card, event_t event)
{
  if (event == EVENT_DECLARE_BLOCKERS)
	{
	  int p = 1-current_turn, parent = get_card_instance(player, card)->damage_source_card, i;

	  for (i = 0; i < active_cards_count[p]; ++i)
		if (in_play(p, i) && is_what(p, i, TYPE_CREATURE) && blocking(p, i, event))
		  pump_until_eot(player, parent, p, i, 0, 1);
	}

  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	kill_card(player, card, KILL_DESTROY);

  return 0;
}

int card_battle_cry(int player, int card, event_t event){
	/* Battle Cry	|2|W	0x200b579
	 * Instant
	 * Untap all |Swhite creatures you control.
	 * Whenever a creature blocks this turn, it gets +0/+1 until end of turn. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	  new_manipulate_all(player, card, player, &test, ACT_UNTAP);

	  create_legacy_effect(player, card, effect_battle_cry);

	  kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_battle_frenzy(int player, int card, event_t event){
	/* Battle Frenzy	|2|R	0x2002D9D
	 * Instant
	 * |SGreen creatures you control get +1/+1 until end of turn.
	 * Non|Sgreen creatures you control get +1/+0 until end of turn. */

	if( event==EVENT_RESOLVE_SPELL ){
		int count = active_cards_count[player] -1;
		while( count > -1 ){
				if( in_play(player, count) && is_what(player, count, TYPE_CREATURE) ){
					if( get_color(player, count) & get_sleighted_color_test(player, card, COLOR_TEST_GREEN) ){
						pump_until_eot(player, card, player, count, 1, 1);
					}
					else{
						pump_until_eot(player, card, player, count, 1, 0);
					}
				}
				count--;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_binding_grasp(int player, int card, event_t event){
	/* Binding Grasp	|3|U	0x200551b
	 * Enchantment - Aura
	 * Enchant creature
	 * At the beginning of your upkeep, sacrifice ~ unless you pay |1|U.
	 * You control enchanted creature.
	 * Enchanted creature gets +0/+1. */

	basic_upkeep(player, card, event, MANACOST_XU(1, 1));

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		if( event == EVENT_TOUGHNESS && affect_me(instance->damage_target_player, instance->damage_target_card) &&
			! is_humiliated(player, card, AL_STATIC) )
		{
			event_result++;
		}
	}

	return card_control_magic(player, card, event);
}

int card_black_scarab(int player, int card, event_t event)
{
  /* Black Scarab	|W	0x200b57e
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature can't be blocked by |Sblack creatures.
	 * Enchanted creature gets +2/+2 as long as an opponent controls a |Sblack permanent. */

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS || event == EVENT_BLOCK_LEGALITY)
    return scarab(player, card, event, COLOR_TEST_BLACK);
  return 0;
}

int card_blessed_wine(int player, int card, event_t event)
{
	/* Blessed Wine	|1|W	0x200b583
	 * Instant
	 * You gain 1 life.
	 * Draw a card at the beginning of the next turn's upkeep. */

	if (event == EVENT_RESOLVE_SPELL)
	{
	  gain_life(player, 1);
	  cantrip(player, card, 1);
	  kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_blinking_spirit(int player, int card, event_t event){
	/* Blinking Spirit	|3|W	0x200cb95
	 * Creature - Spirit 2/2
	 * |0: Return ~ to its owner's hand. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		char buffer[100];
		int iid = get_internal_card_id_from_csv_id(0x38c);
		scnprintf(buffer, sizeof(buffer), " %d", iid);
		do_dialog(player, player, card, -1, -1, buffer, 0);
		card_instance_t *instance = get_card_instance(player, card);
		if( in_play(instance->parent_controller, instance->parent_card) ){
			bounce_permanent(instance->parent_controller, instance->parent_card);
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL);
}

int card_blizzard(int player, int card, event_t event)
{
	/* Blizzard	|G|G	0x200b588
	 * Enchantment
	 * Cast ~ only if you control a snow land.
	 * Cumulative upkeep |2
	 * Creatures with flying don't untap during their controllers' untap steps. */

  cumulative_upkeep(player, card, event, MANACOST_X(2));

	if (event == EVENT_CAN_CAST && generic_spell(player, card, event, 0, NULL, NULL, 0, NULL) )
		return control_a_snow_land(player, 0);

	if (event == EVENT_UNTAP && in_play(player, card)
		&& is_what(affected_card_controller, affected_card, TYPE_CREATURE)
		&& check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING))
			get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;

  return global_enchantment(player, card, event);
}

int card_blue_scarab(int player, int card, event_t event)
{
  /* Blue Scarab	|W	0x200b58d
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature can't be blocked by |Sblue creatures.
	 * Enchanted creature gets +2/+2 as long as an opponent controls a |Sblue permanent. */

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS || event == EVENT_BLOCK_LEGALITY)
    return scarab(player, card, event, COLOR_TEST_BLUE);
  return 0;
}

static int effect_bone_shaman(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);

  if (instance->damage_target_player >= 0 && !is_humiliated(instance->damage_target_player, instance->damage_target_card, AL_STATIC))
	{
	  damage_effects(player, card, event);

	  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player && instance->targets[2].player > 3)
		{
		  if (event == EVENT_TRIGGER)
			event_result = RESOLVE_TRIGGER_MANDATORY;
		  else if (event == EVENT_RESOLVE_TRIGGER)
			{
			  int i;
			  for (i=3; i < instance->targets[2].player; ++i)
				cannot_regenerate_until_eot(instance->damage_source_player, instance->damage_source_card, instance->targets[i].player, instance->targets[i].card);

			  instance->targets[2].player = 3;
			}
		}
	}

  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	kill_card(player, card, KILL_DESTROY);

  return 0;
}

int card_bone_shaman(int player, int card, event_t event)
{
	/* Bone Shaman	|2|R|R	0x200b592
	 * Creature - Giant Shaman 3/3
	 * |B: Until end of turn, ~ gains "Creatures dealt damage by ~ this turn can't be regenerated this turn." */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_RESOLVE_ACTIVATION && in_play(instance->parent_controller, instance->parent_card))
	{
		int leg = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, effect_bone_shaman,
												instance->parent_controller, instance->parent_card);
		if (leg != -1)
			get_card_instance(instance->parent_controller, leg)->targets[2].card = CARD_ID_BONE_SHAMAN;
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL);
}

int card_brainstorm(int player, int card, event_t event){
	/* Brainstorm	|U	0x200133F
	 * Instant
	 * Draw three cards, then put two cards from your hand on top of your library in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 3);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_ANY, "Select a card to put on top.");

		int tpb = MIN(2, hand_count[player]);
		while( tpb > 0 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &this_test);
				if( selected != -1 ){
					put_on_top_of_deck(player, selected);
					tpb--;
				}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* Brushland	""	=>functions.c:generic_painland
 * Land
 * |T: Add |1 to your mana pool.
 * |T: Add |G or |W to your mana pool. ~ deals 1 damage to you. */

int card_brand_of_ill_omen(int player, int card, event_t event)
{
	/* Brand of Ill Omen	|3|R	0x200b597
	 * Enchantment - Aura
	 * Enchant creature
	 * Cumulative upkeep |R
	 * Enchanted creature's controller can't cast creature spells. */

	cumulative_upkeep(player, card, event, MANACOST_R(1));

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == instance->damage_target_player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
			infinite_casting_cost();
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, 1-player);
}

/* ManalinkEh 2022: 0x23f1bf0; original cards/ice_age.c:1489. */
int card_breath_of_dreams(int player, int card, event_t event)
{
  /* Breath of Dreams	|2|U|U	0x000000
   * Enchantment
   * Cumulative upkeep |U
   * |SGreen creatures have "Cumulative upkeep |1." */

  int p;
  int c;
  int age_or_eligible;
  int result;
  int active_player_or_result;
  card_instance_t *instance;
  char marked[151];
  test_definition_t this_test;

  upkeep_trigger_ability(player, card, event, 2);
  active_player_or_result = current_turn;
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (current_turn == player)
    {

      add_counters(current_turn, card, current_turn, card, COUNTER_AGE, 1);
      age_or_eligible = count_counters(active_player_or_result, card, COUNTER_AGE);
      result = dialog_impl(active_player_or_result, card, EVENT_ACTIVATE, DLG_FULLCARD(
                           active_player_or_result, card), DLG_SMALLCARD(active_player_or_result, card),
                           DLG_WHO_CHOOSES(active_player_or_result), DLG_RANDOM, DLG_NO_STORAGE, DLG_AUTOCHOOSE_IF_1,
                           DLG_NO_CANCEL, "Pay cumulative upkeep",
                           has_mana_multi(active_player_or_result, 0, 0, age_or_eligible, 0, 0, 0),
                           1, "Decline", 1, 1, 0, 0, 0, 0);
      if ((result != 1) || (active_player_or_result = charge_mana_multi_while_resolving(
                                active_player_or_result, card, EVENT_RESOLVE_TRIGGER,
                                active_player_or_result, 0, 0, age_or_eligible, 0, 0, 0),
                            active_player_or_result == 0))
      {
        spell_fizzled = 0;
        kill_card(player, card, KILL_SACRIFICE);
        return 1;
      }
    }

    p = current_turn;
    memset(marked, 0, sizeof(marked));
    c = 0;
    if (0 < active_cards_count[current_turn])
    {
      do
      {
        instance = in_play(p, c);
        if ((instance != NULL) && (result = is_what(p, c, 2), result != 0))
        {
          marked[c] = '\x01';
        }
        c = c + 1;
      } while (c < active_cards_count[p]);
    }
    default_test_definition(&this_test, 2);
    c = 0;
    this_test.color = get_sleighted_color_test(player, card, 8);
    if (0 < active_cards_count[p])
    {
      do
      {
        if ((((marked[c] != '\0') && (instance = in_play(p, c), instance != NULL)) &&
             ((instance->token_status & 0x80) == 0)) &&
            ((result = new_make_test_in_play(p, c, -1, &this_test),
              result != 0 &&
                  (result = cumulative_upkeep_arbitrary(
                       player, card, p, c, EVENT_UPKEEP_TRIGGER_ABILITY, 1, 0, 0, 0, 0, 0),
                   result != 0))))
        {
          kill_card(p, c, KILL_SACRIFICE);
        }
        c = c + 1;
      } while (c < active_cards_count[p]);
    }
  }
  else if (event == EVENT_QUERY_CUMULATIVE_UPKEEP)
  {

    default_test_definition(&this_test, 2);
    this_test.color = get_sleighted_color_test(player, card, 8);
    if (0 < active_cards_count[0])
    {
      active_player_or_result = 0;
      do
      {
        if ((((active_player_or_result == affected_card) && (affected_card_controller == 0)) &&
             (instance = in_play(0, active_player_or_result), instance != NULL)) &&
            (age_or_eligible = new_make_test_in_play(0, active_player_or_result, -1, &this_test),
             age_or_eligible != 0))
        {
          event_result = event_result | 1;
        }
        active_player_or_result = active_player_or_result + 1;
      } while (active_player_or_result < active_cards_count[0]);
    }
  }
  return global_enchantment(player, card, event);
}

int card_brine_shaman(int player, int card, event_t event){
	/* Brine Shaman	|1|B	0x2009c97
	 * Creature - Human Cleric Shaman 1/1
	 * |T, Sacrifice a creature: Target creature gets +2/+2 until end of turn.
	 * |1|U|U, Sacrifice a creature: Counter target creature spell. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td1;
	counterspell_target_definition(player, card, &td1, TYPE_CREATURE);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SPELL_ON_STACK | GAA_SACRIFICE_CREATURE, MANACOST_XU(1, 2), 0, &td1, NULL) ){
			return 99;
		}
		if( player == HUMAN ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
		}
		else{
			if( count_subtype(player, TYPE_CREATURE, -1) > 1 ){
				return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_SPELL_ON_STACK | GAA_SACRIFICE_CREATURE, MANACOST_XU(1, 2), 0, &td1, NULL)){
			if( charge_mana_for_activated_ability(player, card, MANACOST_XU(1, 2)) ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if (!sac){
					cancel = 1;
					return 0;
				}
				instance->targets[0].player = card_on_stack_controller;
				instance->targets[0].card = card_on_stack;
				instance->number_of_targets = 1;
				instance->info_slot = 67;
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
		else{
			if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
				if (!sac){
					cancel = 1;
					return 0;
				}
				instance->number_of_targets = 0;
				if( pick_target(&td, "TARGET_CREATURE") ){
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
					tap_card(player, card);
					instance->number_of_targets = 1;
					instance->info_slot = 66;
				}
				else{
					state_untargettable(BYTE2(sac), BYTE3(sac), 0);
				}
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 66 && valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
		if( instance->info_slot == 67 ){
			if (counterspell_validate(player, card, &td1, 0)){
				real_counter_a_spell(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_brown_ouphe(int player, int card, event_t event)
{
	/* Brown Ouphe	|G	0x200b59c
	 * Creature - Ouphe 1/1
	 * |1|G, |T: Counter target activated ability from an artifact source. */

  card_instance_t* instance = get_card_instance(player, card);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t default_td;
    counter_activated_target_definition(player, card, &default_td, TYPE_ARTIFACT);

  if (event == EVENT_CAN_ACTIVATE)
	return (can_counter_activated_ability(player, card, event, &default_td)
			&& !is_tapped(player, card)
			&& !is_sick(player, card)
			&& can_use_activated_abilities(player, card)
			&& has_mana_for_activated_ability(player, card, MANACOST_XG(1,1))) ? 99 : 0;

  if (event == EVENT_ACTIVATE)
	{
	  instance->number_of_targets = 0;
	  if (charge_mana_for_activated_ability(player, card, MANACOST_XG(1,1))
		  && cast_counter_activated_ability(player, card, 0))
		tap_card(player, card);
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	resolve_counter_activated_ability(player, card, &default_td, 0);

  return 0;
}

/* Centaur Archer	|1|R|G	=>antiquities.c:card_grapeshot_catapult
 * Creature - Centaur Archer 3/2
 * |T: ~ deals 1 damage to target creature with flying. */

int card_burnt_offering(int player, int card, event_t event)
{
	/* Burnt Offering	|B	0x200b5a1
	 * Instant
	 * As an additional cost to cast ~, sacrifice a creature.
	 * Add X mana in any combination of |B and/or |R to your mana pool, where X is the sacrificed creature's converted mana cost. */

	if( event == EVENT_CAN_CAST ){
		if( basic_spell(player, card, event) ){
			return can_sacrifice_type_as_cost(player, 1, TYPE_CREATURE);
		}
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		card_instance_t* instance = get_card_instance(player, card);
		int sac = controller_sacrifices_a_permanent(player, card, TYPE_CREATURE, SAC_RETURN_CHOICE);
		if (!sac)
			cancel = 1;
		else
		{
			ai_modifier -= hand_count[player] >= 8 ? 24 : 60;
			instance->info_slot = get_cmc(BYTE2(sac), BYTE3(sac));
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (instance->info_slot > 0)
			FORCE(produce_mana_any_combination_of_colors(player, COLOR_TEST_BLACK|COLOR_TEST_RED, instance->info_slot, NULL));

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int most_common_nontoken_color(int player)
{
  int card, col, colors[COLOR_WHITE+1] = {0};
  for (card = 0; card < active_cards_count[player]; ++card)
	if (in_play(player, card) && is_what(player, card, TYPE_PERMANENT) && !is_token(player, card))
	  {
		color_test_t cols = get_color(player, card);
		if (cols & COLOR_TEST_ANY_COLORED)
		  for (col = COLOR_BLACK; col <= COLOR_WHITE; ++col)
			if (cols & (1 << col))
			  ++colors[col];
	  }
  color_t most = COLOR_COLORLESS;
  int highest = 0;
  for (col = COLOR_BLACK; col <= COLOR_WHITE; ++col)
	if (colors[col] == highest)
	  most = COLOR_COLORLESS;
	else if (colors[col] > highest)
	  {
		most = col;
		highest = colors[col];
	  }
  return most;
}

int card_call_to_arms(int player, int card, event_t event)
{
  /* Call to Arms	|1|W	0x200d725
   * Enchantment
   * As ~ enters the battlefield, choose a color and an opponent.
   * |SWhite creatures get +1/+1 as long as the chosen color is the most common color among nontoken permanents the chosen player controls but isn't tied for most common.
   * When the chosen color isn't the most common color among nontoken permanents the chosen player controls or is tied for most common, sacrifice ~. */

	if (event == EVENT_RESOLVE_SPELL)
	{
		color_t chosen;
		if (IS_AI(player))
		{
			chosen = most_common_nontoken_color(1-player);
			if (chosen == COLOR_COLORLESS)
			{
			  ai_modifier -= 96;
			  chosen = COLOR_WHITE;
			}
		}
		else
			chosen = choose_a_color(player, COLOR_BLACK);

		create_card_name_legacy(player, card, CARD_ID_BLACK + chosen-1);
		card_instance_t* inst = get_card_instance(player, card);
		inst->targets[1].player = CARD_ID_CALL_TO_ARMS + 1-player;
		inst->targets[1].card = CARD_ID_CALL_TO_ARMS + chosen;
	}

	if (event == EVENT_STATIC_EFFECTS)
	{
		card_instance_t* inst = get_card_instance(player, card);
		int chosen_player = inst->targets[1].player - CARD_ID_CALL_TO_ARMS;
		int chosen_color = inst->targets[1].card - CARD_ID_CALL_TO_ARMS;
		if (!(chosen_player >= 0 && chosen_player <= 1)
			|| !(chosen_color >= COLOR_BLACK && chosen_color <= COLOR_WHITE)
			|| most_common_nontoken_color(chosen_player) != chosen_color)
			kill_card(player, card, KILL_SACRIFICE);
	}

  return card_crusade(player, card, event);
}

/* Circle of Protection: Black	|1|W	=>unlimited.c:card_circle_of_protection_black
 * Enchantment
 * |1: The next time a |Sblack source of your choice would deal damage to you this turn, prevent that damage. */

/* Circle of Protection: Blue	|1|W	=>unlimited.c:card_circle_of_protection_blue
 * Enchantment
 * |1: The next time a |Sblue source of your choice would deal damage to you this turn, prevent that damage. */

/* Circle of Protection: Green	|1|W	=>unlimited.c:card_circle_of_protection_green
 * Enchantment
 * |1: The next time a |Sgreen source of your choice would deal damage to you this turn, prevent that damage. */

/* Circle of Protection: Red	|1|W	=>unlimited.c:card_circle_of_protection_red
 * Enchantment
 * |1: The next time a |Sred source of your choice would deal damage to you this turn, prevent that damage. */

/* Circle of Protection: White	|1|W	=>unlimited.c:card_circle_of_protection_white
 * Enchantment
 * |1: The next time a |Swhite source of your choice would deal damage to you this turn, prevent that damage. */

/* ManalinkEh 2022: 0x23f2b30; original cards/ice_age.c:1762. */
int card_caribou_range(int player, int card, event_t event)
{
  /* Caribou Range	|2|W|W	0x000000
   * Enchantment - Aura
   * Enchant land you control
   * Enchanted land has "|W|W, |T: Put a 0/1 |Swhite Caribou creature token onto the battlefield."
   * Sacrifice a Caribou token: You gain 1 life. */

  enum
  {
    CHOICE_TOKEN = 1,
    CHOICE_SAC = 2
  };

  int choice;
  unsigned char attachment_signs;
  card_instance_t *instance;
  int attached_card_or_available;
  int granted_to_player;
  int mode_or_available;
  target_definition_t td;
  test_definition_t test;

  instance = get_card_instance(player, card);
  new_default_test_definition(&test, 0x47, "Select a Caribou to sacrifice.");
  test.subtype = 0x1d;
  granted_to_player = (int)instance->damage_target_player;
  attached_card_or_available = instance->damage_target_card;
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (((-1 < granted_to_player) && (-1 < attached_card_or_available)) &&
        (attached_card_or_available = granted_generic_activated_ability(
             player, card, granted_to_player, attached_card_or_available, EVENT_CAN_ACTIVATE, 1, 0,
             0, 0, 0, 0, 3, 0, NULL, NULL),
         attached_card_or_available != 0))
    {
      return 1;
    }
    return generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE,
                                                            GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL,
                                                            NULL, TYPE_NONE, &test, SARV_0);
  }
  if (event == EVENT_ACTIVATE)
  {

    mode_or_available = 0;
    attachment_signs = ((unsigned char)((unsigned int)attached_card_or_available >> 0x18) ^ 0xff) &
                       (instance->damage_target_player >> 7 ^ 0xffU);
    if ((char)attachment_signs < '\0')
    {
      mode_or_available = granted_generic_activated_ability(
          player, card, granted_to_player, attached_card_or_available, EVENT_CAN_ACTIVATE, 1, 0, 0,
          0, 0, 0, 3, 0, NULL, NULL);
    }
    choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Create Caribou token",
                         mode_or_available, 5, "Sac a Caribou token",
                         generic_activated_ability_with_sacrifice_as_cost(
                             player, card, EVENT_CAN_ACTIVATE, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL,
                             NULL, TYPE_NONE, &test, SARV_0),
                         10, 0, 0, 0, 0);
    if (choice == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (choice == CHOICE_TOKEN)
    {
      if ((char)attachment_signs < '\0')
      {
        add_state(granted_to_player, attached_card_or_available, 0x10);
        granted_generic_activated_ability(player, card, granted_to_player,
                                          attached_card_or_available, EVENT_ACTIVATE, 0, 0, 0, 0, 0,
                                          0, 2, 0, NULL, NULL);
      }
      if (spell_fizzled == 1)
      {
        remove_state(granted_to_player, attached_card_or_available, 0x10);
        return 0;
      }
    }
    else if (choice == CHOICE_SAC)
    {
      generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_ACTIVATE, GAA_NONE, 0, 0,
                                                       0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test,
                                                       SARV_0);
      return 0;
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    mode_or_available = instance->info_slot;
    if (mode_or_available == CHOICE_TOKEN)
    {
      if (granted_to_player < 0)
      {
        return 0;
      }
      if (attached_card_or_available < 0)
      {
        return 0;
      }
      generate_tokens_by_id(player, card, CARD_ID_CARIBOU, 1);
      mode_or_available = instance->info_slot;
    }
    if (mode_or_available == CHOICE_SAC)
    {
      gain_life(player, 1);
      return 0;
    }
  }
  else if (event != EVENT_CLEANUP)
  {
    default_target_definition(player, card, &td, 1);
    td.preferred_controller = player;
    td.allowed_controller = player;
    return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
  }
  return 0;
}

int card_celestial_sword(int player, int card, event_t event){
	/* Celestial Sword	|6	0x2009cec
	 * Artifact
	 * |3, |T: Target creature you control gets +3/+3 until end of turn. Its controller sacrifices it at the beginning of the next end step. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.allowed_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy = pump_ability_until_eot(instance->parent_controller, instance->parent_card,
												instance->targets[0].player, instance->targets[0].card, 3, 3, 0, SP_KEYWORD_DIE_AT_EOT);
			card_instance_t *leg = get_card_instance(player, legacy);
			leg->targets[3].card = KILL_SACRIFICE;

		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_CREATURE");
}

int card_chaos_lord(int player, int card, event_t event)
{
  /* Chaos Lord	|4|R|R|R	0x200d72a
   * Creature - Human 7/7
   * First strike
   * At the beginning of your upkeep, target opponent gains control of ~ if the number of permanents is even.
   * ~ can attack as though it had haste unless it entered the battlefield this turn. */
  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY
	  && player == current_turn)	// otherwise, the broken Paradox Haze approximation will make it swap back and forth on the same turn
	{
	  // It's kind of appalling that count_permenants_by_type can't possibly work, but count_subtype can.
	  int perms = count_subtype(ANYBODY, TYPE_PERMANENT, -1);
	  if (perms % 2 == 0)
		{
		  target_definition_t td;
		  default_target_definition(player, card, &td, 0);
		  td.zone = TARGET_ZONE_PLAYERS;
		  if (would_validate_arbitrary_target(&td, 1-player, -1))
			give_control_of_self(player, card);
		}
	}

  if (event == EVENT_RESOLVE_SPELL)
	get_card_instance(player, card)->info_slot = turn_count;

  if (event == EVENT_ABILITIES && affect_me(player, card) && get_card_instance(player, card)->info_slot != turn_count)
	haste(player, card, event);

  return 0;
}

/* ManalinkEh 2022: 0x23ec100; original cards/ice_age.c:1911. */
int chaos_moon_legacy(int player, int card, event_t event)
{
  int p;
  subtype_t hacked_land;

  if (event == EVENT_CAST_SPELL)
  {
    if (is_what(affected_card_controller, affected_card, 1) != 0)
    {
      hacked_land = get_hacked_subtype(player, card, 0x5006);
      if (has_subtype(affected_card_controller, affected_card, hacked_land) != 0)
      {
        set_special_flags2(affected_card_controller, affected_card, 0x40);
      }
    }
  }
  else if (event == EVENT_COUNT_MANA)
  {
    for (p = 0; p < 2; ++p)
    {
      int initial_count = active_cards_count[p];
      for (int color = 0; color < 7; ++color)
        mana_color_override[p][color] = 0;
      if (initial_count > 0)
        for (int land = 0; land < active_cards_count[p]; ++land)
          if (in_play(p, land) && is_what(p, land, 1))
          {
            subtype_t hacked = get_hacked_subtype(player, card, 0x5006);
            if (has_subtype(p, land, hacked))
              set_special_flags2(p, land, 0x40);
          }
    }
  }
  if (eot_trigger(player, card, event) != 0)
  {
    kill_card(player, card, KILL_EXILE);
    cant_be_responded_to = 1;
    return 0;
  }
  return 0;
}

/* Counterspell	|U|U	=>unlimited.c:card_counterspell
 * Instant
 * Counter target spell. */

/* ManalinkEh 2022: 0x23ec2f0; original cards/ice_age.c:1940. */
int chaos_moon_legacy_odd(int player, int card, event_t event)
{
  int result;
  subtype_t subtype;
  card_instance_t *instance;

  if ((((uint32_t)(event - EVENT_COUNT_MANA) & 0xfffffffd) == 0) &&
      (result = is_what(affected_card_controller, affected_card, 1), result != 0))
  {
    subtype = get_hacked_subtype(player, card, 0x5006);
    result = has_subtype(affected_card_controller, affected_card, subtype);
    if (result != 0)
    {
      instance = in_play(player, card);
      if (instance == NULL)
      {
        return 0;
      }
      if (event == EVENT_COUNT_MANA)
      {
        result = is_tapped(affected_card_controller, affected_card);
        if (result != 0)
        {
          return 0;
        }
        result = is_animated_and_sick(affected_card_controller, affected_card);
        if (result != 0)
        {
          return 0;
        }
        result = can_produce_mana(affected_card_controller, affected_card);
        if (result == 0)
        {
          return 0;
        }
        declare_mana_available(affected_card_controller, 4, 1);
      }
      else if (-1 < tapped_for_mana_color)
      {
        produce_mana(affected_card_controller, 4, 1);
      }
    }
  }
  result = eot_trigger(player, card, event);
  if (result == 0)
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  cant_be_responded_to = 1;
  return 0;
}

/* ManalinkEh 2022: 0x23f3380; original cards/ice_age.c:1973. */
int card_chaos_moon(int player, int card, event_t event)
{
  /* Chaos Moon	|3|R	0x000000
   * Enchantment
   * At the beginning of each upkeep, count the number of permanents. If the number is odd, until end of turn, |Sred creatures get +1/+1
   * 		and whenever a player taps |Ha Mountain for mana, that player adds |R to his or her mana pool.
   * If the number is even, until end of turn, |Sred creatures get -1/-1 and if a player taps |Ha Mountain for mana,
   * 		that |H2Mountain produces colorless mana instead of any other type. */

  int result;
  unsigned int permanents;
  test_definition_t this_test;

  upkeep_trigger_ability(player, card, event, 2);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {

    permanents = count_permanents_by_type(2, TYPE_PERMANENT);
    default_test_definition(&this_test, 2);
    this_test.color = get_sleighted_color_test(player, card, 0x10);
    this_test.color_flag = 0;
    if ((permanents & 1) == 0)
    {
      pump_creatures_until_eot(player, card, 2, 2, -1, -1, 0, 0, &this_test);
      result = create_legacy_effect(player, card, chaos_moon_legacy);
      alternate_legacy_text(4, player, result);
    }
    else
    {
      pump_creatures_until_eot(player, card, 2, 1, 1, 1, 0, 0, &this_test);
      result = create_legacy_effect(player, card, chaos_moon_legacy_odd);
      alternate_legacy_text(3, player, result);
    }
  }
  return global_enchantment(player, card, event);
}

int card_chromatic_armor(int player, int card, event_t event){
	/* Chromatic Armor	|1|W|U	0x2002DA2
	 * Enchantment - Aura
	 * Enchant creature
	 * As ~ enters the battlefield, choose a color.
	 * ~ enters the battlefield with a sleight counter on it.
	 * Prevent all damage that would be dealt to enchanted creature by sources of the last chosen color.
	 * |X: Put a sleight counter on ~ and choose a color. X is the number of sleight counters on ~. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		instance->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, 1);
		add_counters(player, card, player, card, COUNTER_SLEIGHT, 1);
	}

	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		if( event == EVENT_CAN_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(count_counters(player, card, COUNTER_SLEIGHT)), 0, NULL, NULL);
		}
		if( event == EVENT_ACTIVATE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_X(count_counters(player, card, COUNTER_SLEIGHT)), 0, NULL, NULL);
		}
		if( event == EVENT_RESOLVE_ACTIVATION ){
			card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);
			int clr = choose_a_color(player, get_deck_color(player, 1-player));
			parent->info_slot = 1<<clr;
			if( instance->targets[1].card > -1 ){
				kill_card(player, instance->targets[1].card, KILL_EXILE);
			}
			parent->targets[1].card = create_card_name_legacy(instance->parent_controller, instance->parent_card, CARD_ID_BLACK+(clr-1));
			add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_SLEIGHT, 1);
		}
		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card == t_card && damage->damage_target_player == t_player ){
					if( ! check_state(affected_card_controller, affected_card, STATE_CANNOT_TARGET) ){
						int clr = damage->initial_color;
						if( in_play(damage->damage_source_player, damage->damage_source_card) ){
							clr = get_color(damage->damage_source_player, damage->damage_source_card);
						}
						if( clr & instance->info_slot ){
							int amount = damage->info_slot;
							prevent_damage(affected_card_controller, affected_card, amount);
						}
					}
				}
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }
	return vanilla_aura(player, card, event, player);
}

/* Dark Ritual	|B	=>unlimited.c:card_dark_ritual
 * Instant
 * Add |B|B|B to your mana pool. */

/* Death Ward	|W	=>unlimited.c:card_death_ward
 * Instant
 * Regenerate target creature. */

/* Deflection	|3|U	0x000000
 * Instant
 * Change the target of target spell with a single target. */

int card_chub_toad(int player, int card, event_t event)
{
  /* Chub Toad	|2|G	0x200b5a6
   * Creature - Frog 1/1
   * Whenever ~ blocks or becomes blocked, it gets +2/+2 until end of turn. */

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      card_instance_t *instance = get_card_instance(player, card);
      if (((instance->state & 4) && !is_unblocked(player, card)) ||
          (current_turn != player && blocking(player, card, EVENT_DECLARE_BLOCKERS)))
        pump_until_eot(player, card, player, card, 2, 2);
    }
  }
  else if ((uint32_t)(event - EVENT_POW_BOOST) < 2 && !is_humiliated(player, card, AL_STATIC))
    return 2;
  return 0;
}

int card_clairvoyance(int player, int card, event_t event){
	/* Clairvoyance	|U	0x200b5ab
	 * Instant
	 * Look at target player's hand.
	 * Draw a card at the beginning of the next turn's upkeep. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_RESOLVE_SPELL){
		if (valid_target(&td)){
			reveal_target_player_hand(get_card_instance(player, card)->targets[0].player);
			cantrip(player, card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x23f3be0; original cards/ice_age.c:2130. */
int card_cloak_of_confusion(int player, int card, event_t event)
{
  /* Cloak of Confusion	|1|B	0x000000
   * Enchantment - Aura
   * Enchant creature you control
   * Whenever enchanted creature attacks and isn't blocked, you may have it assign no combat damage this turn. If you do, defending player discards a card at random. */

  int p;
  card_instance_t *instance = in_play(player, card);
  int attached_card;
  int eligible;
  int power;
  int target_player;
  unsigned int argument7;
  if (instance && (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    target_player = (int)instance->damage_target_player;
    attached_card = instance->damage_target_card;
    eligible = is_attacking(target_player, attached_card);
    if (event == EVENT_DECLARE_BLOCKERS && eligible != 0)
    {
      eligible = is_humiliated(player, card, AL_TRIG);
      if ((eligible == 0) && (eligible = is_unblocked(target_player, attached_card), eligible != 0))
      {

        argument7 = 0;
        if (target_player == 1)
        {
          p = 1 - player;
          if ((hand_count[p] < 1) ||
              ((power = get_power(1, attached_card), 3 < power && (life[p] < 0xd))))
          {
            argument7 = 1;
          }
          else
          {
            power = get_power(1, attached_card);
            argument7 = (unsigned int)(life[p] <= power);
          }
        }
        if (do_dialog(player, player, card, -1, -1, " Do not assign\nAssign damage", argument7) ==
            0)
        {
          negate_combat_damage_this_turn(player, card, target_player, attached_card, 0);
          discard(1 - player, 1, player);
        }
      }
    }
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  return generic_aura(player, card, event, player, 0, 0, 0, 0, 0, 0, 0);
}

int card_cold_snap(int player, int card, event_t event)
{
	/* Cold Snap	|2|W	0x200b5b0
	 * Enchantment
	 * Cumulative upkeep |2
	 * At the beginning of each player's upkeep, ~ deals damage to that player equal to the number of snow lands he or she controls. */

  upkeep_trigger_ability(player, card, event, ANYBODY);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  if (player == current_turn)
		{
		  cumulative_upkeep(player, card, event, MANACOST_X(2));	// Also calls upkeep_trigger_ability()
		  if (!in_play(player, card))
			return 0;
		}

	  int count = count_snow_permanents(current_turn, TYPE_LAND, 0);
	  if (count > 0)
		damage_player(current_turn, count, player, card);
	}

  return global_enchantment(player, card, event);
}

/* Disenchant	|1|W	=>unlimited.c:card_disenchant
 * Instant
 * Destroy target artifact or enchantment. */

/* Dread Wight	|3|B|B	0x000000
 * Creature - Zombie 3/4
 * At end of combat, put a paralyzation counter on each creature blocking or blocked by ~ and tap those creatures. Each of those creatures doesn't untap during its controller's untap step for as long as it has a paralyzation counter on it. Each of those creatures gains "|4: Remove a paralyzation counter from this creature." */

int card_conquer(int player, int card, event_t event){
	/* Conquer	|3|R|R	0x200cb5e
	 * Enchantment - Aura
	 * Enchant land
	 * You control enchanted land. */

	if( ! IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS )
		return 0;

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	return generic_stealing_aura(player, card, event, &td, "TARGET_LAND");
}

int card_cooperation(int player, int card, event_t event)
{
	/* Cooperation	|2|W	0x200b5b5
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature has banding. */

  return generic_aura(player, card, event, player, 0, 0, KEYWORD_BANDING, 0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x23f40c0; original cards/ice_age.c:2217. */
int card_crown_of_the_ages(int player, int card, event_t event)
{
  /* Crown of the Ages	|2	0x000000
   * Artifact
   * |4, |T: Attach target Aura attached to a creature to another creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td1;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td1, 4);
    td1.required_subtype = 0x4002;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 4, 0, 0, 0, 0, 0, 0,
                                         &td1, NULL);
      return result;
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      add_state(player, card, 0x10);
      tapped_for_mana_color = -1;
      result = charge_mana_for_activated_ability(player, card, 4, 0, 0, 0, 0, 0);
      if (result != 0)
      {
        result = new_pick_target(&td1, "Select target Aura attached to a creature.", 0, 0x81);
        if (result != 0)
        {
          call_card_function(instance->targets[0].player, instance->targets[0].card,
                             EVENT_CAN_MOVE_AURA);
        }
        set_special_flags3(instance->targets[0].player, instance->targets[0].card,
                           ((player == 0) ? 32768U : 65536U));
        call_card_function(instance->targets[0].player, instance->targets[0].card, EVENT_MOVE_AURA);
      }
      tapped_for_mana_color = -1;
      if (spell_fizzled != 1)
      {
        tapped_for_mana_color = -1;
        return 0;
      }
      remove_state(player, card, 0x10);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td1, 4);
    td1.required_subtype = 0x4002;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td1), result != 0))
  {
    call_card_function(instance->targets[0].player, instance->targets[0].card,
                       EVENT_RESOLVE_MOVING_AURA);
    remove_special_flags3(instance->targets[0].player, instance->targets[0].card, 0);
    return 0;
  }
  return 0;
}

/* Drought	|2|W|W	0x000000
 * Enchantment
 * At the beginning of your upkeep, sacrifice ~ unless you pay |W|W.
 * Spells cost an additional "Sacrifice |Ha Swamp" to cast for each |Sblack mana symbol in their mana costs.
 * Activated abilities cost an additional "Sacrifice |Ha Swamp" to activate for each |Sblack mana symbol in their activation costs. */

int card_curse_of_marit_lage(int player, int card, event_t event)
{
	/* Curse of Marit Lage	|3|R|R
	 * Enchantment
	 * When ~ enters the battlefield, tap all |H1Islands.
	 * |H1Islands don't untap during their controllers' untap steps. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_LAND);
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);

		new_manipulate_all(player, card, ANYBODY, &test, ACT_TAP);
	}

	if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && in_play(player, card) &&
		is_what(affected_card_controller, affected_card, TYPE_LAND) &&
		has_subtype(affected_card_controller, affected_card, get_hacked_subtype(player, card, SUBTYPE_ISLAND)) &&
		! is_humiliated(player, card, AL_STATIC) )
	{
		get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;
	}

	return global_enchantment(player, card, event);
}

int card_dance_of_the_dead(int player, int card, event_t event){
	/* Dance of the Dead	|1|B	0x2002DA7
	 * Enchantment - Aura
	 * Enchant creature card in a graveyard
	 * When ~ enters the battlefield, if it's on the battlefield, it loses "enchant creature card in a graveyard" and gains "enchant creature
	 put onto the battlefield with ~." Put enchanted creature card onto the battlefield tapped under your control and attach ~ to it.
	 When ~ leaves the battlefield, that creature's controller sacrifices it.
	 * Enchanted creature gets +1/+1 and doesn't untap during its controller's untap step.
	 * At the beginning of the upkeep of enchanted creature's controller, that player may pay |1|B. If he or she does, untap that creature. */

	// See also card_animate_dead() in recoded_cards.c and card_necromancy() in visions.c

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance( player, card );

		modify_pt_and_abilities(instance->damage_target_player, instance->damage_target_card, event, 1, 1, 0);

		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);

		if( current_turn == instance->damage_target_player && upkeep_trigger(player, card, event) ){
			if( has_mana_multi(instance->damage_target_player, MANACOST_XB(1, 1)) ){
				int ai_choice = is_tapped(instance->damage_target_player, instance->damage_target_card) ? 1 : 0;
				int choice = do_dialog(instance->damage_target_player, player, card, -1, -1, " Untap\n Pass", ai_choice);
				if( choice == 0 ){
					charge_mana_multi(instance->damage_target_player, MANACOST_XB(1, 1));
					if( spell_fizzled != 1 ){
						untap_card(instance->damage_target_player, instance->damage_target_card);
					}
				}
			}
		}

		leaves_play(player, card, event, 1, 1);
		if (event == EVENT_LEAVES_PLAY_ABILITY){
			kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
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
			reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_ATTACH_AS_AURA | REANIMATE_TAP);
		}
		else{
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
}

int card_dark_banishing(int player, int card, event_t event){
	/* Dark Banishing	|2|B	0x200cbef
	 * Instant
	 * Destroy target non|Sblack creature. It can't be regenerated. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance( player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);

}

int card_demonic_consultation(int player, int card, event_t event){
/* Demonic Consultation	|B	0x20016C3
 * Instant
 * Name a card. Exile the top six cards of your library, then reveal cards from the top of your library until you reveal the named card.
 * Put that card into your hand and exile all other cards revealed this way. */

	if (event == EVENT_RESOLVE_SPELL){
		for (int i = 0; i < 6; ++i){ rfg_top_card_of_deck(player); }
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Choose a card name.");
		int choice;
		if (IS_AI(player)){
			choice = ai_name_a_random_csvid(player, card, player, TYPE_NONE, &test);
		}else{
			choice = new_card_from_list(player, 3, &test);
		}
		test_definition_t test1;
		default_test_definition(&test1, TYPE_ANY);
		test1.id = choice;
		reveal_card_until_you_reveal_test(player, player, &test1, RCUYRT_EXILE_REST | RCUYRT_ADD_TEST_TO_HAND);
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_despotic_scepter(int player, int card, event_t event){
	/* Despotic Scepter	|1	0x2002DB6
	 * Artifact
	 * |T: Destroy target permanent you own. It can't be regenerated. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.allowed_controller = player | TARGET_PLAYER_OWNER;
	td.preferred_controller = player | TARGET_PLAYER_OWNER;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT_YOU_OWN");
}

int card_diabolic_vision(int player, int card, event_t event){
	/* Diabolic Vision	|U|B	0x2002DBB
	 * Sorcery
	 * Look at the top five cards of your library. Put one of them into your hand and the rest on top of your library in any order. */

	if( event==EVENT_RESOLVE_SPELL ){
		int amount = MIN(5, count_deck(player));
		if( amount ){
			impulse_effect(player, amount, 2);
			rearrange_top_x(player, player, amount-1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_dire_wolves(int player, int card, event_t event)
{
	/* Dire Wolves	|2|G	0x200b5bf
	 * Creature - Wolf 2/2
	 * ~ has banding as long as you control |Ha Plains. */

  if (event == EVENT_ABILITIES && affect_me(player, card) && !is_humiliated(player, card, AL_STATIC)
	  && check_battlefield_for_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS)))
	event_result |= KEYWORD_BANDING;

  return 0;
}

static int effect_dotd(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->damage_target_player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->damage_target_player;
    int c = instance->damage_target_card;

    cumulative_upkeep_arbitrary(player, card, p, c, event, MANACOST_X(2));

    other_leaves_play(player, card, p, c, event, 1, 1);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      add_card_to_rfg(get_owner(p, c), get_original_internal_card_id(p, c));
    }
    if (event == EVENT_QUERY_CUMULATIVE_UPKEEP && affect_me(p, c))
      event_result |= 1;
  }

  return 0;
}

int card_dreams_of_the_dead(int player, int card, event_t event){
	/* Dreams of the Dead	|3|U	0x2002DC0
	 * Enchantment
	 * |1|U: Return target |Swhite or |Sblack creature card from your graveyard to the battlefield. That creature gains "Cumulative upkeep |2." If the creature would leave the battlefield, exile it instead of putting it anywhere else. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	char buffer[100];
	int pos = scnprintf(buffer, 100, get_sleighted_color_text(player, card, "Select a %s ", COLOR_BLACK));
	scnprintf(buffer + pos, 100-pos, get_sleighted_color_text(player, card, "or %s creature card.", COLOR_WHITE));
	new_default_test_definition(&this_test, TYPE_CREATURE, buffer);
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK) | get_sleighted_color_test(player, card, COLOR_TEST_WHITE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			int result = reanimate_permanent(instance->parent_controller, instance->parent_card, player, selected, REANIMATE_DEFAULT);
			if( result > -1 ){
				add_status(instance->parent_controller, result, STATUS_TOKEN);
				set_special_flags(instance->parent_controller, result, SF_UNEARTH); // Not really, but otherwise it will be seen as a "true" token.
				create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &effect_dotd, instance->parent_controller, result);
			}
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_drift_of_the_dead(int player, int card, event_t event)
{
  /* Drift of the Dead	|3|B	0x200d72f
   * Creature - Wall 100/100
   * Defender
   * ~'s power and toughness are each equal to the number of snow lands you control. */
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && !is_humiliated(player, card, AL_STATIC) && player != -1)
	set_characteristic_pt(count_snow_permanents(player, TYPE_LAND, 0));
  return 0;
}

int card_dwarven_armory(int player, int card, event_t event){
	/* Dwarven Armory	|2|R|R	0x2002DC5
	 * Enchantment
	 * |2, Sacrifice a land: Put a +2/+2 counter on target creature. Activate this ability only during any upkeep step. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_P2_P2, 1);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_ONLY_ON_UPKEEP,
															MANACOST_X(2), 0, &td, "TARGET_CREATURE", TYPE_LAND, NULL, 0);
}

int card_earthlink(int player, int card, event_t event)
{
  /* Earthlink	|3|B|R|G	0x200d739
   * Enchantment
   * At the beginning of your upkeep, sacrifice ~ unless you pay |2.
   * Whenever a creature dies, that creature's controller sacrifices a land. */

  basic_upkeep(player, card, event, MANACOST_X(2));

  return card_burning_sands(player, card, event);
}

/* ManalinkEh 2022: 0x23f53e0; original cards/ice_age.c:2614. */
int card_earthlore(int player, int card, event_t event)
{
  /* Earthlore	|G	0x000000
   * Enchantment - Aura
   * Enchant land you control
   * Tap enchanted land: Target blocking creature gets +1/+2 until end of turn. Activate this ability only if enchanted land is untapped. */

  card_instance_t *instance;
  int result;
  int argument1;
  target_definition_t td;
  instance = in_play(player, card);
  if (instance && (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_BLOCKING;
    argument1 = (int)instance->damage_target_player;
    result = instance->damage_target_card;
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (((-1 < argument1) && (-1 < result)) &&
          (result = is_tapped(argument1, result), result == 0))
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 6, 0, 0, 0, 0, 0, 0, 0,
                                         &td, "Select target blocking creature.");
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      if ((-1 < argument1) && (-1 < result))
      {
        add_state(argument1, result, 0x10);
        generic_activated_ability(player, card, EVENT_ACTIVATE, 6, 0, 0, 0, 0, 0, 0, 0, &td,
                                  "Select target blocking creature.");
      }
      if (spell_fizzled == 1)
      {
        remove_state(argument1, result, 0x10);
      }
    }
    else
    {
      result = event == EVENT_RESOLVE_ACTIVATION ? valid_target(&td) : 0;
      if (result != 0)
      {
        pump_until_eot(instance->parent_controller, instance->parent_card,
                       instance->targets[0].player, instance->targets[0].card, 1, 2);
      }
    }
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION ||
      event == EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 1);
  td.preferred_controller = player;
  td.allowed_controller = player;
  return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_elder_druid(int player, int card, event_t event)
{
  /* Elder Druid	|3|G	0x200d74d
   * Creature - Elf Cleric Druid 2/2
   * |3|G, |T: You may tap or untap target artifact, creature, or land. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);
  td.preferred_controller = ANYBODY;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	twiddle(player, card, 0);

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_XG(3,1), 0, &td, "TWIDDLE");
}

/* Fear	|B|B	=>unlimited.c:card_fear
 * Enchantment - Aura
 * Enchant creature
 * Enchanted creature has fear. */

int card_elemental_augury(int player, int card, event_t event){
	/* Elemental Augury	|U|B|R	0x2002DCA
	 * Enchantment
	 * |3: Look at the top three cards of target player's library, then put them back in any order. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			rearrange_top_x(instance->targets[0].player, player, 3);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_PLAYER");
}

/* Flame Spirit	|4|R	=>fifth_dawn.c:card_furnace_whelp
 * Creature - Elemental Spirit 2/3
 * |R: ~ gets +1/+0 until end of turn. */

/* ManalinkEh 2022: 0x23f59c0; original cards/ice_age.c:2711. */
int card_elkin_bottle(int player, int card, event_t event)
{
  /* Elkin Bottle	|3	0x000000
   * Artifact
   * |3, |T: Exile the top card of your library. Until the beginning of your next upkeep, you may play that card. */

  enum
  {
    MPCFE_UNTIL_YOUR_NEXT_UPKEEP = 64
  };

  if (!IS_GAA_EVENT(event))
    return 0;

  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    int iid = *deck_ptr[player];
    if (iid != -1)
    {
      int csvid = cards_data[iid].id;
      rfg_top_card_of_deck(player);
      create_may_play_card_from_exile_effect(player, card, player, csvid,
                                             MPCFE_UNTIL_YOUR_NEXT_UPKEEP);
    }
  }
  return generic_activated_ability(player, card, event, 1, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* Flooded Woodlands	|2|U|B	0x000000
 * Enchantment
 * |SGreen creatures can't attack unless their controller sacrifices a land for each |Sgreen creature he or she controls that's attacking. */

/* ManalinkEh 2022: 0x23f5ab0; original cards/ice_age.c:2730. */
int card_elvish_healer(int player, int card, event_t event)
{
  /* Elvish Healer	|2|W	0x000000
   * Creature - Elf Cleric 1/2
   * |T: Prevent the next 1 damage that would be dealt to target creature or player this turn. If that creature is |Sgreen, prevent the next 2 damage instead. */

  int target_player;
  const char *prompt;
  int result;
  card_instance_t *instance;
  card_instance_t *parent;
  unsigned int target_color;
  unsigned int sleighted_color;
  int amount;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0,
                                         &td, NULL);
      if (result == 0)
      {
        return 0;
      }
      return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      instance->number_of_targets = '\0';
      prompt = get_sleighted_color_text(
          player, card, "Select any target to prevent next 1 damage or 2 for a %s creature.", 3);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 0, 0, &td, prompt);
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
  }
  td.preferred_controller = player;
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  result = valid_target(&td);
  if (result == 0)
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  parent = get_card_instance(instance->parent_controller, instance->parent_card);
  result = parent->targets[0].card;
  target_player = parent->targets[0].player;
  amount = 1;
  if (result >= 0)
  {
    target_color = get_color(target_player, result);
    sleighted_color =
        get_sleighted_color_test(instance->parent_controller, instance->parent_card, 8);
    if ((target_color & sleighted_color) != 0)
      amount = 2;
  }
  prevent_the_next_n_damage(instance->parent_controller, instance->parent_card, target_player, result,
                            amount, 0, 0, 0);
  return 0;
}

static int enduring_renewal_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_ENDURING_RENEWAL)
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
            SET_BYTE2(instance->targets[11].player) = 1;
          }
        }
        count_for_gfp_ability_and_store_values(
            player, card, event, player, TYPE_CREATURE, NULL,
            GFPC_TRACK_DEAD_CREATURES | GFPC_INTO_WHOSE_GRAVE | GFPC_EXTRA_SKIP_TOKENS, 1);
      }
    }
  }

  int resolve = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (resolve)
  {
    if (!check_status(player, card, STATUS_INVISIBLE_FX))
    {
      card_instance_t *instance = get_card_instance(player, card);
      int i;
      for (i = 1; i < 10; i++)
      {
        int iid = instance->targets[i].player;
        if (iid != -1)
        {
          instance->targets[i].player = -1;
          seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id,
                                         REANIMATEXTRA_RETURN_TO_HAND2);
        }
        else
        {
          break;
        }
        iid = instance->targets[i].card;
        if (iid != -1)
        {
          instance->targets[i].card = -1;
          seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id,
                                         REANIMATEXTRA_RETURN_TO_HAND2);
        }
        else
        {
          break;
        }
      }
      kill_card(player, card, KILL_EXILE);
    }
  }

  if (!check_status(player, card, STATUS_INVISIBLE_FX) && event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_enduring_renewal(int player, int card, event_t event){
/* Enduring Renewal	|2|W|W	0x2002DD4
 * Enchantment
 * Play with your hand revealed.
 * If you would draw a card, reveal the top card of your library instead. If it's a creature card, put it into your graveyard.
 * Otherwise, draw a card.
 * Whenever a creature is put into your graveyard from the battlefield, return it to your hand. */

	if (event == EVENT_STATIC_EFFECTS){
		player_bits[player] |= PB_HAND_REVEALED;
	}

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && !suppress_draw &&
		reason_for_trigger_controller == player && ! is_humiliated(player, card, AL_TRIG) )
	{
		if(event == EVENT_TRIGGER){
			event_result |= RESOLVE_TRIGGER_MANDATORY;
		}
		else if(event == EVENT_RESOLVE_TRIGGER){
				int iid = deck_ptr[player][0];
				if( iid != -1 ){
					if( is_what(-1, iid, TYPE_CREATURE) ){
						mill(player, 1);
						suppress_draw = 1;
					}
				}
		}
	}

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) ){
		int found = 0;
		int c;
		for(c=0; c<active_cards_count[player]; c++){
			if( in_play(player, c) && is_what(player, c, TYPE_EFFECT) ){
				card_instance_t *inst = get_card_instance(player, c);
				if( inst->info_slot == (int)enduring_renewal_effect ){
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
			int legacy = create_targetted_legacy_effect(player, card, &enduring_renewal_effect, player, card);
			card_instance_t *inst = get_card_instance(player, legacy);
			inst->targets[0].player = player;
			inst->targets[0].card = card;
			inst->number_of_targets = 1;
			add_status(player, legacy, STATUS_INVISIBLE_FX);
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		if( get_owner(affected_card_controller, affected_card) == player ){
			count_for_gfp_ability_and_store_values(player, card, event, player, TYPE_CREATURE, NULL, GFPC_INTO_WHOSE_GRAVE |
													GFPC_EXTRA_SKIP_TOKENS, 0);
		}
	}

	if( get_card_instance(player, card)->kill_code <= 0 ){
		if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
			card_instance_t *instance = get_card_instance(player, card);
			int i;
			for(i=0; i<10; i++){
				int iid = instance->targets[i].player;
				if( iid != -1 ){
					instance->targets[i].player = -1;
					seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id, REANIMATEXTRA_RETURN_TO_HAND2);
				}
				else{
					break;
				}
				iid = instance->targets[i].card;
				if( iid != -1 ){
					instance->targets[i].card = -1;
					seek_grave_for_id_to_reanimate(player, -1, player, cards_data[iid].id, REANIMATEXTRA_RETURN_TO_HAND2);
				}
				else{
					break;
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

/* Forest	""	=>magic.exe:card_forest
 * Basic Land - |H2Forest */

int card_energy_storm(int player, int card, event_t event){
	/* Energy Storm	|1|W	0x2009c9c
	 * Enchantment
	 * Cumulative upkeep |1
	 * Prevent all damage that would be dealt by instant and sorcery spells.
	 * Creatures with flying don't untap during their controllers' untap steps. */

	cumulative_upkeep(player, card, event, MANACOST_X(1));

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( !(damage->targets[3].player & TYPE_PERMANENT) ){
					int amount = damage->info_slot;
					prevent_damage(affected_card_controller, affected_card, amount);
				}
			}
		}

		if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP ){
			if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
				check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING) )
			{
				does_not_untap(affected_card_controller, affected_card, event);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_enervate(int player, int card, event_t event)
{
  /* Enervate	|1|U	0x200d7cf
   * Instant
   * Tap target artifact, creature, or land.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);
  td.preferred_controller = 1-player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  if (player == AI && (get_card_instance(inst->targets[0].player, inst->targets[0].card)->state & STATE_TAPPED))
			ai_modifier -= 48;
		  tap_card(inst->targets[0].player, inst->targets[0].card);
		  cantrip(player, card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TWIDDLE", 1, NULL);
}

int card_errant_minion(int player, int card, event_t event)
{
  /* Errant Minion	|2|U	0x200d80b
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of the upkeep of enchanted creature's controller, that player may pay any amount of mana.
   * ~ deals 2 damage to that player. Prevent X of that damage, where X is the amount of mana that player paid this way. */


	upkeep_trigger_ability(player, card, event, get_card_instance(player, card)->damage_target_player);

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
		card_instance_t* inst = get_card_instance(player, card);
		int dmg = 2;
		if (IS_AI(player))
		{
			int mana_to_charge = MIN(2, has_mana(inst->damage_target_player, COLOR_COLORLESS, 2));
			charge_mana(inst->damage_target_player, COLOR_COLORLESS, mana_to_charge);
			if (cancel != 1)
				dmg -= mana_to_charge;
		}
		else if (charge_mana_while_resolving(player, card, event, inst->damage_target_player, COLOR_COLORLESS, -1))
			dmg -= x_value;

		if (dmg > 0)
			damage_player(inst->damage_target_player, dmg, player, card);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, 1-player);
}

int card_errantry(int player, int card, event_t event)
{
  /* Errantry	|1|R	0x200d833
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +3/+0 and can only attack alone. */

  if (event == EVENT_ATTACK_LEGALITY)
	{
	  card_instance_t* inst = in_play(player, card);
	  if (!inst || inst->damage_target_player != affected_card_controller)
		return 0;

	  card_instance_t* aff = get_card_instance(affected_card_controller, affected_card);
	  if (aff->state & STATE_ATTACKING)
		return 0;

	  if (affected_card != inst->damage_target_card)
		{
		  // Another creature - fail if enchanted creature was already declared an attacker
		  if (get_card_instance(inst->damage_target_player, inst->damage_target_card)->state & STATE_ATTACKING)
			event_result = 1;
		}
	  else
		{
		  // Enchanted creature - fail if any other creature was already declared an attacker
		  int c, p = inst->damage_target_player;
		  for (c = 0; c < active_cards_count[p]; ++c)
			if ((aff = in_play(p, c)) && (aff->state & STATE_ATTACKING))
			  {
				event_result = 1;
				break;
			  }
		}
	}

  return generic_aura(player, card, event, player, 3, 0, 0, 0, 0, 0, 0);
}

int card_essence_filter(int player, int card, event_t event)
{
  /* Essence Filter	|1|G|G	0x200d7f2
   * Sorcery
   * Destroy all enchantments or all non|Swhite enchantments. */

  if (IS_GS_EVENT(player, card, event))
	{
	  enum
	  {
		CHOICE_ALL = 1,
		CHOICE_NONWHITE
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"All enchantments",	1, 1,
						get_sleighted_color_text(player, card, "Non%s enchantments", COLOR_WHITE),	1, 1);

	  if (event == EVENT_CAN_CAST)
		return choice;
	  else if (event == EVENT_RESOLVE_SPELL)
		{
		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_ENCHANTMENT, "");
		  if (choice == CHOICE_NONWHITE)
			{
			  test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			  test.color_flag = DOESNT_MATCH;
			}
		  new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
		  kill_card(player, card, KILL_DESTROY);
		}
	}

  return 0;
}

int card_essence_flare(int player, int card, event_t event)
{
  /* Essence Flare	|U	0x200d770
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +2/+0.
   * At the beginning of the upkeep of enchanted creature's controller, put a -0/-1 counter on that creature. */

  card_instance_t* inst = get_card_instance(player, card);
  if (inst->damage_target_player >= 0)
	upkeep_trigger_ability(player, card, event, inst->damage_target_player);

  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
	add_counters(player, card, inst->damage_target_player, inst->damage_target_card, COUNTER_M0_M1, 1);

  int rval = generic_aura(player, card, event, player, 2,0, 0,0, 0,0,0);
  if (event == EVENT_CAST_SPELL && affect_me(player, card) && spell_fizzled != 1
	  && (is_sick(inst->targets[0].player, inst->targets[0].card)
		  || is_tapped(inst->targets[0].player, inst->targets[0].card)))
	ai_modifier -= 99;

  if (event == EVENT_SHOULD_AI_PLAY && inst->damage_target_player >= 0)
	{
	  card_instance_t* ench = get_card_instance(inst->damage_target_player, inst->damage_target_card);
	  inst->counter_toughness -= 2;
	  inst->regen_status |= KEYWORD_RECALC_TOUGHNESS;
	  if (inst->damage_target_player == AI && current_turn == AI && !(ench->state & STATE_ATTACKED))
		ai_modifier -= 60;
	}
  return rval;
}

int card_essence_vortex(int player, int card, event_t event){
	/* Essence Vortex	|1|U|B	0x2002DCF
	 * Instant
	 * Destroy target creature unless its controller pays life equal to its toughness. A creature destroyed this way can't be regenerated. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			int life_to_pay = get_toughness(instance->targets[0].player, instance->targets[0].card);
			char prompt[100];
			scnprintf(prompt, 100, "Pay %d life", life_to_pay);
			if( can_pay_life(instance->targets[0].player, life_to_pay) &&
				DIALOG(player, card, EVENT_ACTIVATE, DLG_NO_STORAGE, DLG_NO_CANCEL, DLG_RANDOM, DLG_WHO_CHOOSES(instance->targets[0].player),
						prompt, 1, 1,
						"Pass", 1, 1) == 1
				  ){
					lose_life(instance->targets[0].player, life_to_pay);
			}
			else{
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_fanatical_fever(int player, int card, event_t event)
{
  /* Fanatical Fever	|2|G|G	0x200d7a7
   * Instant
   * Target creature gets +3/+0 and gains trample until end of turn. */
  return vanilla_instant_pump(player, card, event, ANYBODY, player, 3,0, KEYWORD_TRAMPLE,0);
}

int card_fiery_justice(int player, int card, event_t event){
	/* Fiery Justice	|R|G|W	0x2002230
	 * Sorcery
	 * ~ deals 5 damage divided as you choose among any number of targets. Target opponent gains 5 life. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
		return would_validate_arbitrary_target(&td1, 1-player, -1);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int i;
		int good = 0;
		for(i=0; i<5; i++){
			if( new_pick_target(&td, "TARGET_ANY", i, 0) ){
				if( instance->targets[i].card != -1 ){
					add_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
				}
				good++;
			}
			else{
				break;
			}
		}
		for(i=0; i<good; i++){
			remove_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
		}
		if( good == 5 ){
			instance->targets[5].player = 1-player;
			instance->targets[5].card = -1;
			instance->number_of_targets = 6;
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		instance->number_of_targets = 5;	// Number being damaged
		divide_damage(player, card, &td);
		instance->number_of_targets = 6;
		if( validate_target(player, card, &td1, 5) ){
			gain_life(instance->targets[5].player, 5);
		}
		kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

/* Fyndhorn Elder	|2|G	=>zendikar.c:card_greenweaver_druid
 * Creature - Elf Druid 1/1
 * |T: Add |G|G to your mana pool. */

/* Fyndhorn Elves	|G	=>m14.c:card_elvish_mystic
 * Creature - Elf Druid 1/1
 * |T: Add |G to your mana pool. */

/* ManalinkEh 2022: 0x23ed060; original cards/ice_age.c:3254. */
char *not_indestructible(int who_chooses, int player, int card)
{
  int result;
  char *error;
  result = check_for_special_ability(player, card, 0x800);
  error = NULL;
  if (result != 0)
  {
    error = "This is indestructible";
  }
  return error;
}

/* Game of Chaos	|R|R|R	0x000000
 * Sorcery
 * Flip a coin. If you win the flip, you gain 1 life and target opponent loses 1 life, and you decide whether to flip again. If you lose the flip, you lose 1 life and that opponent gains 1 life, and that player decides whether to flip again. Double the life stakes with each flip. */

/* ManalinkEh 2022: 0x23f7250; original cards/ice_age.c:3262. */
int card_fire_covenant(int player, int card, event_t event)
{
  /* Fire Covenant	|1|B|R	0x000000
   * Instant
   * As an additional cost to cast ~, pay X life.
   * ~ deals X damage divided as you choose among any number of target creatures. */

  int p;
  int c;
  card_instance_t *instance;
  int token_or_adjustment;
  card_instance_t *creature;
  int value;
  int active_player_or_life;
  int eligible_or_toughness;
  int life_payment;
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    if (player == 1)
    {
      td.illegal_abilities = get_protections_from(1, card);
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (int32_t)not_indestructible;
    }
    if (basic_spell(player, card, EVENT_CAN_CAST) != 0)
    {
      return can_pay_life_as_cost_for_spells_or_activated_abilities(player, 1);
    }
  }
  else
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
    else if ((event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET) &&
             (event != EVENT_CHANGE_TARGET))
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    default_target_definition(player, card, &td, 2);
    if (player == 1)
    {
      td.illegal_abilities = get_protections_from(1, card);
      td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
      td.extra = (int32_t)not_indestructible;
    }
    if (event == EVENT_CAST_SPELL)
    {
      if (((card == affected_card) && (player == affected_card_controller)) &&
          (token_or_adjustment = is_token(player, card), active_player_or_life = current_turn,
           token_or_adjustment == 0))
      {
        c = active_cards_count[current_turn] + -1;
        life_payment = 0;
        if (-1 < c)
        {
          do
          {
            if ((((active_player_or_life == 1 - player) &&
                  (creature = in_play(active_player_or_life, c), creature != NULL)) &&
                 ((value = is_what(active_player_or_life, c, 2),
                   value != 0 && ((value = can_target(&td),
                                   value != 0 && (value = check_for_ability(active_player_or_life,
                                                                            c, 0x64800),
                                                  value == 0)))))) &&
                (value = check_for_special_ability(
                     active_player_or_life, c, (SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_HEXPROOF)),
                 value == 0))
            {
              value = get_toughness(active_player_or_life, c);
              life_payment = life_payment + value;
            }
            c = c + -1;
          } while (c != -1);
        }
        active_player_or_life = current_turn;
        p = 1 - current_turn;
        c = active_cards_count[p] + -1;
        if (-1 < c)
        {
          do
          {
            if (((((player == active_player_or_life) &&
                   (creature = in_play(p, c), creature != NULL)) &&
                  (eligible_or_toughness = is_what(p, c, 2), eligible_or_toughness != 0)) &&
                 ((eligible_or_toughness = can_target(&td),
                   eligible_or_toughness != 0 &&
                       (eligible_or_toughness = check_for_ability(p, c, 0x64800),
                        eligible_or_toughness == 0)))) &&
                (eligible_or_toughness = check_for_special_ability(
                     p, c, (SP_KEYWORD_INDESTRUCTIBLE | SP_KEYWORD_HEXPROOF)),
                 eligible_or_toughness == 0))
            {
              eligible_or_toughness = get_toughness(p, c);
              life_payment = life_payment + eligible_or_toughness;
            }
            c = c + -1;
          } while (c != -1);
        }
        active_player_or_life = life[player];
        if (active_player_or_life - life_payment < 6)
        {
          token_or_adjustment = 0;
          value = active_player_or_life - (life_payment + -1);
          if (value < 7)
          {
            token_or_adjustment = value + -6;
          }
          life_payment = token_or_adjustment + life_payment + -1;
        }
        if (player == 0)
        {
          life_payment = choose_a_number(0, "Pay how much life?", active_player_or_life);
        }
        if ((life_payment < 1) || (life[player] < life_payment))
        {
          spell_fizzled = 1;
        }
        else
        {
          lose_life(player, life_payment);
          instance->info_slot = life_payment;
        }
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      target_and_divide_damage(player, card, &td, "TARGET_CREATURE", instance->info_slot);
      kill_card(player, card, KILL_SACRIFICE);
      return 0;
    }
  }
  return 0;
}

/* Gaze of Pain	|1|B	0x000000
 * Sorcery
 * Until end of turn, whenever a creature you control attacks and isn't blocked, you may choose to have it deal damage equal to its power to a target creature. If you do, it assigns no combat damage this turn. */

/* General Jarkeld	|3|W	0x000000
 * Legendary Creature - Human Soldier 1/2
 * |T: Switch the blocking creatures of two target attacking creatures. Activate this ability only during the declare blockers step. */

/* Ghostly Flame	|B|R	0x000000
 * Enchantment
 * |SBlack and/or |Sred permanents and spells are colorless sources of damage. */

/* Giant Growth	|G	=>unlimited.c:card_giant_growth
 * Instant
 * Target creature gets +3/+3 until end of turn. */

int card_flare(int player, int card, event_t event)
{
	/* Flare	|2|R	0x200d7ac
	 * Instant
	 * ~ deals 1 damage to any target.
	 * Draw a card at the beginning of the next turn's upkeep. */
  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  damage_target0(player, card, 1);
		  cantrip(player, card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_flow_of_maggots(int player, int card, event_t event)
{
  /* Flow of Maggots	|2|B	0x200d7b1
   * Creature - Insect 2/2
   * Cumulative upkeep |1
   * ~ can't be blocked by non-Wall creatures. */

  cumulative_upkeep(player, card, event, MANACOST_X(1));

  if (event == EVENT_BLOCK_LEGALITY && attacking_card == card && attacking_card_controller == player
	  && !has_subtype(affected_card_controller, affected_card, SUBTYPE_WALL)
	  && !is_humiliated(player, card, AL_STATIC))
	event_result = 1;

  return 0;
}
int card_folk_of_the_pines(int player, int card, event_t event)
{
  /* Folk of the Pines	|4|G	0x200d7bb
   * Creature - Dryad 2/5
   * |1|G: ~ gets +1/+0 until end of turn. */
  return generic_shade_merge_pt(player, card, event, 0, MANACOST_XG(1,1), 1,0);
}
/* ManalinkEh 2022: 0x23f78b0; original cards/ice_age.c:3379. */
int card_forbidden_lore(int player, int card, event_t event)
{
  /* Forbidden Lore	|2|G	0x000000
   * Enchantment - Aura
   * Enchant land
   * Enchanted land has "|T: Target creature gets +2/+1 until end of turn." */

  card_instance_t *instance;
  target_definition_t td;
  if (!is_humiliated(player, card, AL_STATIC) &&
      (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    default_target_definition(attached_player, attached_card, &td, 2);
    td.preferred_controller = player;
    if (event == EVENT_CAN_ACTIVATE)
      return granted_generic_activated_ability(player, card, attached_player, attached_card, event,
                                               5, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      if (pick_target(&td, "TARGET_CREATURE"))
        return granted_generic_activated_ability(player, card, attached_player, attached_card,
                                                 event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    }
    else if (event == EVENT_RESOLVE_ACTIVATION &&
             validate_target(attached_player, attached_card, &td, 0))
    {
      card_instance_t *land = get_card_instance(attached_player, attached_card);
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             land->targets[0].player, land->targets[0].card, 2, 1, 0, 0);
    }
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
  td.preferred_controller = player;
  return targeted_aura(player, card, event, &td, "TARGET_LAND");
}

/* Glacial Wall	|2|U	=>vanilla
 * Creature - Wall 0/7
 * Defender */

int card_force_void(int player, int card, event_t event)
{
  /* Force Void	|2|U	0x200d7f7
   * Instant
   * Counter target spell unless its controller pays |1.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)))
	return counterspell(player, card, event, NULL, 0);

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (counterspell_resolve_unless_pay_x(player, card, NULL, 0, 1))
		cantrip(player, card, 1);
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

int card_forgotten_lore(int player, int card, event_t event)
{
	/* Forgotten Lore	|G	0x200b5c4
	 * Sorcery
	 * Target opponent chooses a card in your graveyard. You may pay |G. If you do, repeat this process except that opponent can't choose a card already chosen for ~. Then put the last chosen card into your hand. */

  return shrouded_lore_impl(player, card, event, COLOR_GREEN);
}

int card_formation(int player, int card, event_t event)
{
  /* Formation	|1|W	0x200d78e
   * Instant
   * Target creature gains banding until end of turn.
   * Draw a card at the beginning of the next turn's upkeep. */
  return vanilla_instant_pump(player, card, event, ANYBODY, player, 0,0, VANILLA_PUMP_CANTRIP|KEYWORD_BANDING,0);
}

int card_foul_familiar(int player, int card, event_t event)
{
  /* Foul Familiar	|2|B	0x200d7fc
   * Creature - Spirit 3/1
   * ~ can't block.
   * |B, Pay 1 life: Return ~ to its owner's hand. */

  cannot_block(player, card, event);

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		bounce_permanent(inst->parent_controller, inst->parent_card);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_B(1), 1, NULL, NULL);
}

int card_foxfire(int player, int card, event_t event)
{
  /* Foxfire	|2|G	0x200d874
   * Instant
   * Untap target attacking creature. Prevent all combat damage that would be dealt to and dealt by that creature this turn.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_state = TARGET_STATE_ATTACKING;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  alternate_legacy_text(1, player, maze_of_ith_effect(player, card, inst->targets[0].player, inst->targets[0].card));
		  alternate_legacy_text(2, player, cantrip(player, card, 1));
		  untap_card(inst->targets[0].player, inst->targets[0].card);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ATTACKING_CREATURE", 1, NULL);
}

int card_freyalise_supplicant(int player, int card, event_t event){
	/* Freyalise Supplicant	|1|G	0x2009ca1
	 * Creature - Human Cleric 1/1
	 * |T, Sacrifice a |Sred or |Swhite creature: ~ deals damage to any target equal to half the sacrificed creature's power, rounded down. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED | COLOR_TEST_WHITE);
    scnprintf(test.message, 100, "%s", get_sleighted_color_text2(player, card, "Select a %s or %s creature to sacrifice.", COLOR_RED, COLOR_WHITE));

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST0, 0, &td, NULL,
																0, &test, 0);
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			int result = new_sacrifice(player, card, player, SAC_RETURN_CHOICE | SAC_JUST_MARK, &test);
			if( result ){
				int s_player = BYTE2(result);
				int s_card = BYTE3(result);
				if( pick_target(&td, "TARGET_ANY") ){
					instance->info_slot = get_power(s_player, s_card);
					kill_card(s_player, s_card, KILL_SACRIFICE);
					tap_card(player, card);
					instance->number_of_targets = 1;
				}
				else{
					state_untargettable(s_player, s_card, 0);
					spell_fizzled = 1;
				}
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, round_down_value(instance->info_slot));
		}
	}

	return 0;
}

int card_freyalises_charm(int player, int card, event_t event)
{
  /* Freyalise's Charm	|G|G	0x200d89c
   * Enchantment
   * Whenever an opponent casts a |Sblack spell, you may pay |G|G. If you do, you draw a card.
   * |G|G: Return ~ to its owner's hand. */

  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && reason_for_trigger_controller == player){
    test_definition_t test;
    default_test_definition(&test, TYPE_ANY);
    test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
    sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA){
    if (charge_mana_while_resolving(player, card, event, player, COLOR_GREEN, 2)){ draw_a_card(player); }
  }
  else
  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		bounce_permanent(inst->parent_controller, inst->parent_card);
	}

  if (event == EVENT_SHOULD_AI_PLAY || event == EVENT_CAN_CAST)
	return global_enchantment(player, card, event);

  return generic_activated_ability(player, card, event, 0, MANACOST_G(2), 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x23f8570; original cards/ice_age.c:3605. */
int card_freyalises_winds(int player, int card, event_t event)
{
  /* Freyalise's Winds	|2|G|G	0x000000
   * Enchantment
   * Whenever a permanent becomes tapped, put a wind counter on it.
   * If a permanent with a wind counter on it would untap during its controller's untap step, remove all wind counters from it instead. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_TAP_CARD)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = is_what(affected_card_controller, affected_card, 0x47);
      if (result != 0)
      {

        instance = get_card_instance(player, card);
        result = affected_card;
        instance->targets[1].card = 0x42;
        add_counters(player, card, affected_card_controller, result, COUNTER_WIND, 1);
        instance->targets[1].card = 0;
      }
    }
  }
  else if ((current_phase == 1) && (event == EVENT_UNTAP))
  {
    result = is_what(affected_card_controller, affected_card, 0x47);
    if (result != 0)
    {
      result = count_counters(affected_card_controller, affected_card, COUNTER_WIND);
      if (result != 0)
      {

        instance = get_card_instance(affected_card_controller, affected_card);
        instance->untap_status = instance->untap_status & 0xfffffffc;
        remove_all_counters(player, card, affected_card_controller, affected_card,
                                        COUNTER_WIND);
      }
    }
  }
  result = global_enchantment(player, card, event);
  return result;
}

int card_fumarole(int player, int card, event_t event)
{
	/* Fumarole	|3|B|R	0x200bef2
	 * Sorcery
	 * As an additional cost to cast ~, pay 3 life.
	 * Destroy target creature and target land. */

	if( !IS_CASTING(player, card, event) ){
		return 0;
	}

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);

	target_definition_t td_land;
	default_target_definition(player, card, &td_land, TYPE_LAND);

	card_instance_t* instance = get_card_instance(player, card);

	if(event == EVENT_CAN_CAST){
		int result = generic_spell(player, card, event, GS_CAN_TARGET, &td_creature, NULL, 1, NULL);
		if( result ){
			return can_target(&td_land) && (is_token(player, card) || can_pay_life_as_cost_for_spells_or_activated_abilities(player, 3));
		}
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		instance->number_of_targets = 0;
		if( pick_target(&td_creature, "TARGET_CREATURE") && new_pick_target(&td_land, "TARGET_LAND", 1, 1) ){
			if( ! is_token(player, card) ){
				lose_life(player, 3);
			}
		}
	}

	if (event == EVENT_RESOLVE_SPELL){
		if (validate_target(player, card, &td_creature, 0)){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}

		if (validate_target(player, card, &td_land, 1)){
			kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* ManalinkEh 2022: 0x23f8a30; original cards/ice_age.c:3683. */
int card_fylgja(int player, int card, event_t event)
{
  /* Fylgja	|W	0x000000
   * Enchantment - Aura
   * Enchant creature
   * ~ enters the battlefield with four healing counters on it.
   * Remove a healing counter from ~: Prevent the next 1 damage that would be dealt to enchanted creature this turn.
   * |2|W: Put a healing counter on ~. */

  card_instance_t *instance;
  int choice;
  bool can_prevent;
  int value;
  card_instance_t *aura_instance;
  int healing_counters;
  card_instance_t *parent;
  target_definition_t td;

  enters_the_battlefield_with_counters(player, card, event, COUNTER_HEALING, 4);
  bool aura_event =
      event == EVENT_CAN_CAST ||
      (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) ||
      event == EVENT_RESOLVE_SPELL || event == EVENT_CAN_CHANGE_TARGET ||
      event == EVENT_CHANGE_TARGET ||
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) == EVENT_CAN_MOVE_AURA || (uint32_t)(event - EVENT_POWER) < 3 ||
      event == EVENT_RESOLVE_MOVING_AURA || event == EVENT_CHANGE_TYPE ||
      event == EVENT_SET_COLOR || event == EVENT_GET_SELECTED_CARD || event == EVENT_STATIC_EFFECTS;
  if (!aura_event)
  {
    if ((((event == EVENT_ACTIVATE) || ((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2)) ||
         (event == EVENT_CLEANUP)) &&
        ((aura_instance = in_play(player, card),
          aura_instance != NULL && (aura_instance = get_card_instance(player, card),
                                    -1 < aura_instance->damage_target_player))))
    {
      instance = get_card_instance(player, card);
      default_target_definition(player, card, &td, 2);
      td.illegal_abilities = 0;
      value = generic_activated_ability(player, card, event, 0, 0, 0, 0, 0, 0, 0, 0, &td, NULL);
      if (event == EVENT_CAN_ACTIVATE)
      {
        healing_counters = count_counters(player, card, COUNTER_HEALING);
        if (healing_counters < 1)
        {
          return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 2, 0, 0, 0, 0, 1, 0,
                                           NULL, NULL);
        }
        if (value == 0)
        {
          return 0;
        }
        return (((land_can_be_played & 4U) == 0) ? 1U : 99U);
      }
      if (event == EVENT_ACTIVATE)
      {

        instance->number_of_targets = '\0';
        value = has_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
        can_prevent = false;
        if (value != 0)
        {
          value = count_counters(player, card, COUNTER_HEALING);
          can_prevent = 0 < value;
        }
        choice =
            dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Prevent 1 damage",
                        (unsigned int)can_prevent, 0xf, "Add healing counter",
                        generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 2, 0, 0, 0,
                                                  0, 1, 0, NULL, NULL),
                        5, 0, 0, 0, 0);
        if (choice == 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice == 1)
        {
          remove_counters(player, card, player, card, COUNTER_HEALING, 1);
          return 0;
        }
        if (choice == 2)
        {
          if (((ai_is_speculating == 1) ||
               ((player == 1 && (((unsigned char)trace_mode & 2) == 0)))) &&
              (current_turn != player))
          {
            ai_modifier = ai_modifier + 0x30;
          }
          generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 2, 0, 0, 0, 0, 1, 0, NULL,
                                    NULL);
          return 0;
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        value = instance->info_slot;
        if (value == 1)
        {
          if (valid_target(&td) != 0)
          {

            parent = get_card_instance(instance->parent_controller, instance->parent_card);
            prevent_the_next_n_damage(instance->parent_controller, instance->parent_card,
                                      (int)parent->damage_target_player, parent->damage_target_card,
                                      1, 0, 0, 0);
          }
          value = instance->info_slot;
        }
        if (value == 2)
        {
          add_counters(player, card, player, card, COUNTER_HEALING, 1);
          return 0;
        }
      }
    }
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

/* Hoar Shade	|3|B	=>torment.c:card_nantuko_shade
 * Creature - Shade 1/2
 * |B: ~ gets +1/+1 until end of turn. */

/* Howl from Beyond	|X|B	=>unlimited.c:card_howl_from_beyond
 * Instant
 * Target creature gets +X/+0 until end of turn. */

/* Hurricane	|X|G	=>unlimited.c:card_hurricane
 * Sorcery
 * ~ deals X damage to each creature with flying and each player. */

int card_fyndhorn_bow(int player, int card, event_t event){
	/* Fyndhorn Bow	|2	0x2009cf1
	 * Artifact
	 * |3, |T: Target creature gains first strike until end of turn. */

	if (event == EVENT_CHECK_ABILITIES){
		if (affected_card_controller == player && card_fyndhorn_bow(player, card, EVENT_CAN_ACTIVATE)){
			check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
		}
	}
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	return vanilla_creature_pumper(player, card, event, MANACOST_X(3), GAA_UNTAPPED, 0, 0, KEYWORD_FIRST_STRIKE, 0, &td);
}

int card_fyndhorn_brownie(int player, int card, event_t event)
{
  /* Fyndhorn Brownie	|2|G	0x200d789
   * Creature - Ouphe 1/1
   * |2|G, |T: Untap target creature. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  untap_card(inst->targets[0].player, inst->targets[0].card);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_XG(2,1), 0, &td, "TARGET_CREATURE");
}

int card_fyndhorn_pollen(int player, int card, event_t event){
	/* Fyndhorn Pollen	|2|G	0x2002DD9
	 * Enchantment
	 * Cumulative upkeep |1
	 * All creatures get -1/-0.
	 * |1|G: All creatures get -1/-0 until end of turn. */

	cumulative_upkeep(player, card, event, MANACOST_X(1));

	if( event == EVENT_POWER && is_what(affected_card_controller, affected_card, TYPE_CREATURE) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result--;
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		APNAP(p, {pump_subtype_until_eot(instance->parent_controller, instance->parent_card, p, -1, -1, 0, 0, 0);};);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_XG(1, 1), 0, NULL, NULL);
}

/* Ice Cauldron	|4	0x000000
 * Artifact
 * |X, |T: Put a charge counter on ~ and exile a nonland card from your hand. You may cast that card for as long as it remains exiled. Note the type and amount of mana spent to pay this activation cost. Activate this ability only if there are no charge counters on ~.
 * |T, Remove a charge counter from ~: Add ~'s last noted type and amount of mana to your mana pool. Spend this mana only to cast the last card exiled with ~. */

int card_gangrenous_zombies(int player, int card, event_t event){
	/* Gangrenous Zombies	|1|B|B	0x2002DDE
	 * Creature - Zombie 2/2
	 * |T, Sacrifice ~: ~ deals 1 damage to each creature and each player. If you control a snow |H2Swamp, ~ deals 2 damage to each creature and each player instead. */

	if( event == EVENT_RESOLVE_ACTIVATION){
		int count = 0;
		int damage = 1;
		while( count < active_cards_count[player] ){
			   if( in_play(player, count) && is_snow_permanent(player, count) && has_subtype(player, count, get_hacked_subtype(player, card, SUBTYPE_SWAMP)) ){
				   damage++;
				   break;
				}
				count++;
		}
		new_damage_all(player, card, ANYBODY, damage, NDA_PLAYER_TOO, NULL);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
}

int card_giant_trap_door_spider(int player, int card, event_t event){
	/* Giant Trap Door Spider	|1|R|G	0x2002DE3
	 * Creature - Spider 2/3
	 * |1|R|G, |T: Exile ~ and target creature without flying that's attacking you. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;
	td.required_state = TARGET_STATE_ATTACKING;
	td.illegal_abilities |= KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_RFG_ME | GAA_LITERAL_PROMPT, MANACOST_XGR(1, 1, 1), 0,
									&td, "Select target creature without flying that's attacking you.");
}

static int can_pay_2_life_thunk(int player, int card, int number_of_age_counters)
{
  return can_pay_life(player, 2 * number_of_age_counters);
}

/* Icy Manipulator	|4	=>unlimited.c:card_icy_manipulator
 * Artifact
 * |1, |T: Tap target artifact, creature, or land. */

static int lose_2_life_thunk(int player, int card, int number_of_age_counters)
{
  lose_life(player, 2 * number_of_age_counters);
  return 1;
}

int card_glacial_chasm(int player, int card, event_t event)
{
  /* Glacial Chasm	""	0x2002DE8
   * Land
   * Cumulative upkeep-Pay 2 life.
   * When ~ enters the battlefield, sacrifice a land.
   * Creatures you control can't attack.
   * Prevent all damage that would be dealt to you. */

  int result;
  card_instance_t *instance;
  test_definition_t test;
  bool skip_prevention = false;
  if (event == EVENT_RESOLVE_SPELL)
  {
    play_land_sound_effect(player, card);
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    skip_prevention = is_humiliated(player, card, AL_TRIG) != 0;
    if (!skip_prevention)
      cumulative_upkeep_general(player, card, event, can_pay_2_life_thunk, lose_2_life_thunk);
  }
  else
  {
    etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (event == EVENT_ETB)
    {
      if (!is_humiliated(player, card, AL_TRIG))
      {
        new_default_test_definition(&test, 1, "Select a land to sacrifice.");
        new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test);
      }
      skip_prevention = is_humiliated(player, card, AL_TRIG) != 0;
      if (!skip_prevention)
        cumulative_upkeep_general(player, card, event, can_pay_2_life_thunk, lose_2_life_thunk);
    }
    else if (!is_humiliated(player, card, AL_TRIG))
      cumulative_upkeep_general(player, card, event, can_pay_2_life_thunk, lose_2_life_thunk);
  }
  if (!skip_prevention)
  {
    if ((((event == EVENT_PREVENT_DAMAGE) &&
          (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
         (instance = get_card_instance(affected_card_controller, affected_card),
          instance->internal_card_id == damage_card)) &&
        (((instance->damage_target_player == player && (instance->damage_target_card == -1)) &&
          ((result = damage_is_to_planeswalker(instance),
            result == 0 && (0 < instance->info_slot))))))
    {
      prevent_damage(affected_card_controller, affected_card, instance->info_slot);
    }
  }
  result = is_humiliated(player, card, AL_STATIC);
  if (result == 0)
  {
    nobody_can_attack(player, card, event, player);
    result = mana_colors_added[player];
  }
  else
  {
    result = mana_colors_added[player];
  }
  if (result == 0)
  {
    return 0;
  }
  return plain_mana_producer(player, card, event);
}

/* Illusionary Presence	|1|U|U	0x000000
 * Creature - Illusion 2/2
 * Cumulative upkeep |U
 * At the beginning of your upkeep, choose a land type. ~ gains landwalk of the chosen type until end of turn. */

/* ManalinkEh 2022: 0x23f9930; original cards/ice_age.c:3963. */
int card_glacial_crevasses(int player, int card, event_t event)
{
  /* Glacial Crevasses	|2|R	0x000000
   * Enchantment
   * Sacrifice a snow |H2Mountain: Prevent all combat damage that would be dealt this turn. */

  const char *land_prompt;
  int c;
  int value;
  card_instance_t *instance;
  int unblocked_power;
  test_definition_t test2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    land_prompt = get_hacked_land_text(player, card, "Select a snow %s card.", 0x5006);
    new_default_test_definition(&test2, 1, land_prompt);
    test2.subtype = get_hacked_subtype(player, card, 0x5006);
    test2.sub2 = 0x2003;
    test2.subtype_flag = 4;
    if (event == EVENT_ACTIVATE)
    {

      if ((1 - player == current_turn) && (c = active_cards_count[current_turn] + -1, -1 < c))
      {
        unblocked_power = 0;
        do
        {
          instance = in_play(current_turn, c);
          if ((((instance != NULL) && (value = is_what(current_turn, c, 2), value != 0)) &&
               (value = is_attacking(current_turn, c), value != 0)) &&
              (value = is_unblocked(current_turn, c), value != 0))
          {
            value = get_power(current_turn, c);
            unblocked_power = unblocked_power + value;
          }
          if ((4 < unblocked_power) ||
              (value = ai_modifier + -2, life[current_turn] <= unblocked_power))
          {
            value = ai_modifier + 0x40;
          }
          c = c + -1;
          ai_modifier = value;
        } while (c != -1);
      }
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      }
      return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_NONE, 0, 0,
                                                              0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                              &test2, SARV_0);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return global_enchantment(player, card, event);
    }
    land_prompt = get_hacked_land_text(player, card, "Select a snow %s card.", 0x5006);
    new_default_test_definition(&test2, 1, land_prompt);
    test2.subtype = get_hacked_subtype(player, card, 0x5006);
  }
  test2.sub2 = 0x2003;
  test2.subtype_flag = 4;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    fog_effect(instance->parent_controller, instance->parent_card);
  }
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  return generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_NONE, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test2, SARV_0);
}

/* Illusionary Wall	|4|U	=>card_illusionary_forces
 * Creature - Illusion Wall 7/4
 * Defender, flying, first strike
 * Cumulative upkeep |U */

int card_glaciers(int player, int card, event_t event)
{
  /* Glaciers	|2|W|U	0x200d748
   * Enchantment
   * At the beginning of your upkeep, sacrifice ~ unless you pay |W|U.
   * All |H1Mountains are |H1Plains. */
  basic_upkeep(player, card, event, MANACOST_WU(1,1));
  change_lands_into_new_land_type(player, card, event, SUBTYPE_MOUNTAIN, MATCH, SUBTYPE_PLAINS);
  return global_enchantment(player, card, event);
}

int card_goblin_lyre(int player, int card, event_t event)
{
	/* Goblin Lyre	|3	0x200d860
	 * Artifact
	 * Sacrifice ~: Flip a coin. If you win the flip, ~ deals damage to target opponent or planeswalker equal to the number of creatures you control. If you lose the flip, ~ deals damage to you equal to the number of creatures that opponent or that planeswalker's controller controls. */
  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  opponent_or_pw_target_definition(player, card, &td);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (flip_a_coin(player, card))
		damage_target0(player, card, creature_count[player]);
	  else
		damage_player(player, creature_count[inst->targets[0].player], player, card);
	}

  return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_OPPONENT_OR_PLANESWALKER");
}

int card_goblin_mutant(int player, int card, event_t event){
	/* Goblin Mutant	|2|R|R	0x2002DED
	 * Creature - Goblin Mutant 5/3
	 * Trample
	 * ~ can't attack if defending player controls an untapped creature with power 3 or greater.
	 * ~ can't block creatures with power 3 or greater. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( current_turn == player && current_phase == PHASE_DECLARE_ATTACKERS && event == EVENT_ATTACK_LEGALITY && affect_me(player, card) ){
			int count = 0;
			while( count < active_cards_count[1-player] ){
					if( in_play(1-player, count) && is_what(1-player, count, TYPE_CREATURE) && get_power(1-player, count) > 2 &&
						! is_tapped(1-player, count)
					  ){
						event_result = 1;
						break;
					}
					count++;
			}
		}
		if( current_turn == 1-player && current_phase == PHASE_DECLARE_BLOCKERS && event == EVENT_BLOCK_LEGALITY &&
			affect_me(player, card)
		  ){
			if( get_power(attacking_card_controller, attacking_card) > 2 ){
				event_result = 1;
			}
		}
	}

	return 0;
}

/* Infernal Denizen	|7|B	0x000000
 * Creature - Demon 5/7
 * At the beginning of your upkeep, sacrifice two |H1Swamps. If you can't, tap ~, and an opponent may gain control of a creature you control of his or her choice for as long as ~ remains on the battlefield.
 * |T: Gain control of target creature for as long as ~ remains on the battlefield. */

/* ManalinkEh 2022: 0x23f9fc0; original cards/ice_age.c:4080. */
int card_goblin_sappers(int player, int card, event_t event)
{
  /* Goblin Sappers	|1|R	0x000000
   * Creature - Goblin 1/1
   * |R|R, |T: Target creature you control can't be blocked this turn. Destroy it and ~ at end of combat.
   * |R|R|R|R, |T: Target creature you control can't be blocked this turn. Destroy it at end of combat. */

  enum
  {
    CHOICE_KILL_BOTH = 1,
    CHOICE_KILL_TARGET = 2
  };

  card_instance_t *instance;
  int choice_or_available;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 2, 0, 0,
                                       &td, "TARGET_CREATURE");
    }
    if (event == EVENT_ACTIVATE)
    {

      choice_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0,
                                                      0, 2, 0, 0, &td, NULL);
      choice_or_available =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Unblockable both die",
                      choice_or_available, 5, "Unblockable target dies",
                      generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 4,
                                                0, 0, &td, NULL),
                      0x14, 0, 0, 0, 0);
      if (choice_or_available == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice_or_available == CHOICE_KILL_BOTH)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 0, 0, 0, 0, 2, 0, 0, &td,
                                  "TARGET_CREATURE");
        if (((current_turn == player) && (current_phase < 0x17)) &&
            (choice_or_available = can_target(&td), choice_or_available != 0))
        {
          ai_modifier = ai_modifier + 10;
          return 0;
        }
        ai_modifier = ai_modifier + -100;
        return 0;
      }
      if (choice_or_available != CHOICE_KILL_TARGET)
      {
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 0, 0, 0, 0, 4, 0, 0, &td,
                                "TARGET_CREATURE");
      if (((current_turn == player) && (current_phase < 0x17)) &&
          (choice_or_available = can_target(&td), choice_or_available != 0))
      {
        ai_modifier = ai_modifier + 0xf;
        return 0;
      }
      ai_modifier = ai_modifier + -0x32;
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
    td.allowed_controller = player;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (choice_or_available = valid_target(&td), choice_or_available != 0))
  {
    if (instance->info_slot == CHOICE_KILL_BOTH)
    {
      alternate_legacy_text(2, player,
                            create_targetted_legacy_effect(player, card, die_at_end_of_combat,
                                                           instance->targets[0].player,
                                                           instance->targets[0].card));
    }
    alternate_legacy_text(3, player,
                          create_targetted_legacy_effect(player, card, die_at_end_of_combat,
                                                         instance->parent_controller,
                                                         instance->parent_card));
    alternate_legacy_text(1, player,
                          pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                                                 instance->targets[0].player,
                                                 instance->targets[0].card, 0, 0, 0, 0x10));
    return 0;
  }
  return 0;
}

/* Island	""	=>magic.exe:card_island
 * Basic Land - |H2Island */

int card_goblin_ski_patrol(int player, int card, event_t event){
	/* Goblin Ski Patrol	|1|R	0x2002DF2
	 * Creature - Goblin 1/1
	 * |1|R: ~ gets +2/+0 and gains flying. Its controller sacrifices it at the beginning of the next end step.
	 * Activate this ability only once and only if you control a snow |H2Mountain. */

	 if (event == EVENT_CAN_ACTIVATE){
		card_instance_t *instance = get_card_instance( player, card );
		if (instance->targets[3].card == 42){
			return 0;
		}

		int found = 0;
		int c;
		for (c = 0; c < active_cards_count[player]; ++c){
			if (in_play(player, c) && is_snow_permanent(player, c) && has_subtype(player, c, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN))){
				found = 1;
				break;
			}
		}
		if( found ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XR(1,1), 0, NULL, NULL);
		}
	}

	if (event == EVENT_ACTIVATE && cancel != 1){
		generic_activated_ability(player, card, event, 0, MANACOST_XR(1,1), 0, NULL, NULL);
		if( spell_fizzled != 1 ){
			get_card_instance( player, card )->targets[3].card = 42;	// not activateable by anyone for the rest of the game
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance( player, card );
		int leg = pump_ability_until_eot(instance->parent_controller, instance->parent_card,
										instance->parent_controller, instance->parent_card, 2, 0, KEYWORD_FLYING, SP_KEYWORD_DIE_AT_EOT);
		card_instance_t* legacy = get_card_instance(player, leg);
		legacy->targets[3].card = KILL_SACRIFICE;
	}

	return 0;
}

int card_gorilla_pack(int player, int card, event_t event)
{
  /* Gorilla Pack	|2|G	0x200d752
   * Creature - Ape 3/3
   * ~ can't attack unless defending player controls |Ha Forest.
   * When you control no |H1Forests, sacrifice ~. */
  landhome(player, card, event, SUBTYPE_FOREST);
  return 0;
}

int card_gravebind(int player, int card, event_t event)
{
  /* Gravebind	|B	0x200d856
   * Instant
   * Target creature can't be regenerated this turn.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  if (player == AI && IS_AI(player))
	{
	  td.special = TARGET_SPECIAL_REGENERATION;
	  td.required_state = TARGET_STATE_DESTROYED;
	}
  else if (event == EVENT_CAN_CAST && (land_can_be_played & LCBP_REGENERATION))
	return 0;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  alternate_legacy_text(1, player, cannot_regenerate_until_eot(player, card, inst->targets[0].player, inst->targets[0].card));
		  alternate_legacy_text(2, player, cantrip(player, card, 1));
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event,
					   GS_CAN_TARGET | (player == AI && IS_AI(player) ? GS_REGENERATION : 0),
					   &td, "TARGET_CREATURE", 1, NULL);
}

int card_green_scarab(int player, int card, event_t event)
{
  /* Green Scarab	|W	0x200b5c9
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature can't be blocked by |Sgreen creatures.
	 * Enchanted creature gets +2/+2 as long as an opponent controls a |Sgreen permanent. */

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS || event == EVENT_BLOCK_LEGALITY)
    return scarab(player, card, event, COLOR_TEST_GREEN);
  return 0;
}

/* Juniper Order Druid	|2|G	=>unlimited.c:card_ley_druid
 * Creature - Human Cleric Druid 1/1
 * |T: Untap target land. */

/* Karplusan Forest	""	=>functions.c:generic_painland
 * Land
 * |T: Add |1 to your mana pool.
 * |T: Add |R or |G to your mana pool. ~ deals 1 damage to you. */

/* ManalinkEh 2022: 0x23fadf0; original cards/ice_age.c:4312. */
int card_grizzled_wolverine(int player, int card, event_t event)
{
  /* Grizzled Wolverine	|1|R|R	0x000000
   * Creature - Wolverine 2/2
   * |R: ~ gets +2/+0 until end of turn. Activate this ability only during the declare blockers step, only if at least one creature is blocking ~, and only once each turn. */

  int result;
  event_t event_00;
  if (event == EVENT_POW_BOOST)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x2000, 0, 0, 0, 0, 0, 0,
                                       0, NULL, NULL);
    if (result != 0)
    {
      event_00 = EVENT_POW_BOOST;
      return generic_shade_merge_pt(player, card, event_00, 0, 0, 0, 0, 0, 1, 0, 2, 0);
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    event_00 = EVENT_RESOLVE_ACTIVATION;
    return generic_shade_merge_pt(player, card, event_00, 0, 0, 0, 0, 0, 1, 0, 2, 0);
  }
  if (((current_phase == 0x18) &&
       (result =
            generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 1, 0, 0, NULL, NULL),
        result != 0)) &&
      (current_turn == player))
  {
    result = count_my_blockers(player, card);
    return (unsigned int)(0 < result);
  }
  return 0;
}

/* ManalinkEh 2022: 0x23fb140; original cards/ice_age.c:4359. */
int card_halls_of_mist(int player, int card, event_t event)
{
  /* Halls of Mist	""	0x000000
   * Land
   * Cumulative upkeep |1
   * Creatures that attacked during their controller's last turn can't attack. */

  unsigned int attacker_state;
  card_instance_t *instance;
  int result;
  int card_00;

  cumulative_upkeep(player, card, event, 1, 0, 0, 0, 0, 0);
  if (0 < active_cards_count[current_turn])
  {
    card_00 = 0;
    do
    {
      while (true)
      {
        if (event == EVENT_DECLARE_ATTACKERS)
        {
          result = is_attacking(current_turn, card_00);
          if (((result != 0) && (result = is_what(current_turn, card_00, 2), result != 0)) &&
              (result = is_humiliated(player, card, AL_STATIC), result == 0))
          {
            instance = get_card_instance(current_turn, card_00);
            instance->targets[1].player = 0x42;
          }
        }
        else if ((event == EVENT_CLEANUP) && (instance = get_card_instance(current_turn, card_00),
                                              instance->targets[1].player == 0x42))
        {
          instance = get_card_instance(current_turn, card_00);
          attacker_state = instance->state;
          instance = get_card_instance(current_turn, card_00);
          instance->info_slot = attacker_state & 0x40;
        }
        instance = get_card_instance(current_turn, card_00);
        if (instance->info_slot != 0)
          break;
        card_00 = card_00 + 1;
        if (active_cards_count[current_turn] <= card_00)
        {
          return 0;
        }
      }
      cannot_attack(current_turn, card_00, event);
      card_00 = card_00 + 1;
    } while (card_00 < active_cards_count[current_turn]);
  }
  return 0;
}

int card_heal(int player, int card, event_t event)
{
	/* Heal	|W	0x200d847
	 * Instant
	 * Prevent the next 1 damage that would be dealt to any target this turn.
	 * Draw a card at the beginning of the next turn's upkeep. */
  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ANY);
  td.extra = damage_card;
  td.illegal_abilities = 0;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  card_instance_t* dmg = get_card_instance(inst->targets[0].player, inst->targets[0].card);
		  if (dmg->info_slot > 0)
			--dmg->info_slot;
		  cantrip(player, card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET|GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
}

int card_hecatomb(int player, int card, event_t event){
	/* Hecatomb	|1|B|B	0x200380b
	 * Enchantment
	 * When ~ enters the battlefield, sacrifice ~ unless you sacrifice four creatures.
	 * Tap an untapped |H2Swamp you control: ~ deals 1 damage to any target. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		test.qty = 4;
		if( ! new_sacrifice(player, card, player, SAC_CAUSED | SAC_NO_CANCEL, &test) ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
		test.qty = 4;
		if( ! new_can_sacrifice(player, card, player, &test) && ! check_for_cip_effects_removal(player, card) ){
			ai_modifier-=1000;
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	card_instance_t *instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND );
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.required_subtype = get_hacked_subtype(player, card, SUBTYPE_SWAMP);
	td.illegal_state = TARGET_STATE_TAPPED;
	td.illegal_abilities = 0;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL) ){
			return can_target(&td1);
		}
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		char buffer[100];
		scnprintf(buffer, 100, "%s", get_hacked_land_text(player, card, "Select an untapped %s you control to tap.", SUBTYPE_SWAMP));
		if( new_pick_target(&td, buffer, 0, 1 | GS_LITERAL_PROMPT) ){
			int swamp = instance->targets[0].card;
			instance->number_of_targets = 0;
			if( pick_target(&td1, "TARGET_ANY") ){
				tap_card(player, swamp);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td1) ){
			damage_target0(player, card, 1);
		}
	}

	return 0;
}

/* Kjeldoran Phalanx	|5|W	=>vanilla
 * Creature - Human Soldier 2/5
 * First strike; banding */

int card_hematite_talisman(int player, int card, event_t event)
{
  /* Hematite Talisman	|2	0x200d883
   * Artifact
   * Whenever a player casts a |Sred spell, you may pay |3. If you do, untap target permanent. */
  return talisman(player, card, event, COLOR_RED);
}

/* ManalinkEh 2022: 0x23fb8a0; original cards/ice_age.c:4493. */
int card_hipparion(int player, int card, event_t event)
{
  /* Hipparion	|1|W	0x000000
   * Creature - Horse 1/3
   * ~ can't block creatures with power 3 or greater unless you pay |1. */

  card_instance_t *instance;
  int result;

  if (event == EVENT_CHANGE_TYPE)
  {
    event_flags = event_flags | 0x100000;
  }
  if (trigger_condition != TRIGGER_BLOCKER_CHOSEN)
  {
    return 0;
  }
  if (current_phase != 0x17)
  {
    return 0;
  }
  if (reason_for_trigger_controller == current_turn)
  {
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

  instance = in_play(player, card);
  if (instance == NULL)
  {
    return 0;
  }
  if (0x96 < instance->blocking)
  {
    return 0;
  }
  if (trigger_cause != card)
  {
    return 0;
  }
  if (trigger_cause_controller != player)
  {
    return 0;
  }
  result = get_power(current_turn, (unsigned int)instance->blocking);
  if (result < 3)
  {
    return 0;
  }
  result = has_mana(player, 7, 1);
  if (result == 0)
  {
    forbid_attack = 1;
  }
  else
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      result = charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player,
                                           COLOR_COLORLESS, 1);
      if (result != 0)
      {
        forbid_attack = 0;
        return 0;
      }
      forbid_attack = 1;
      spell_fizzled = 0;
      remove_state(player, card, 0x8008);
      instance->blocking = 0xff;
      forbid_attack = 0;
      return 0;
    }
    if (forbid_attack == 0)
    {
      return 0;
    }
  }
  remove_state(player, card, 0x8008);
  instance->blocking = 0xff;
  forbid_attack = 0;
  return 0;
}

/* Kjeldoran Skycaptain	|4|W	=>vanilla
 * Creature - Human Soldier 2/2
 * Flying; first strike; banding */

/* Kjeldoran Skyknight	|2|W	=>vanilla
 * Creature - Human Knight 1/1
 * Flying; first strike; banding */

/* Kjeldoran Warrior	|W	=>vanilla
 * Creature - Human Warrior 1/1
 * Banding */

/* Knight of Stromgald	|B|B	=>fallen_empires.c:card_order_of_the_ebon_hand
 * Creature - Human Knight 2/1
 * Protection from |Swhite
 * |B: ~ gains first strike until end of turn.
 * |B|B: ~ gets +1/+0 until end of turn. */

/* ManalinkEh 2022: 0x23fba20; original cards/ice_age.c:4540. */
int card_hot_springs(int player, int card, event_t event)
{
  /* Hot Springs	|1|G	0x000000
   * Enchantment - Aura
   * Enchant land you control
   * Enchanted land has "|T: Prevent the next 1 damage that would be dealt to any target this turn." */

  card_instance_t *instance;
  target_definition_t td;
  if (!is_humiliated(player, card, AL_STATIC) &&
      (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    default_target_definition(attached_player, attached_card, &td, 0xff);
    td.extra = damage_card;
    td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
    td.illegal_abilities = 0;
    if (event == EVENT_CAN_ACTIVATE)
      return granted_generic_activated_ability(player, card, attached_player, attached_card, event,
                                               0x1000005, 0, 0, 0, 0, 0, 0, 0, &td,
                                               "TARGET_DAMAGE");
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      if (pick_target(&td, "TARGET_DAMAGE"))
        return granted_generic_activated_ability(player, card, attached_player, attached_card,
                                                 event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    }
    else if (event == EVENT_RESOLVE_ACTIVATION &&
             validate_target(attached_player, attached_card, &td, 0))
    {
      card_instance_t *land = get_card_instance(attached_player, attached_card);
      prevent_damage(land->targets[0].player, land->targets[0].card, 1);
    }
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
  td.preferred_controller = player;
  td.allowed_controller = player;
  return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_hyalopterous_lemure(int player, int card, event_t event)
{
  /* Hyalopterous Lemure	|4|B	0x200d7c0
   * Creature - Spirit 4/3
   * |0: ~ gets -1/-0 and gains flying until end of turn. */
  return generic_shade(player, card, event, 0, MANACOST0, -1,0, KEYWORD_FLYING,0);
}

int card_hydroblast(int player, int card, event_t event)
{
  /* Hydroblast	|U	0x200d86a
   * Instant
   * Choose one -
   * * Counter target spell if it's |Sred.
   * * Destroy target permanent if it's |Sred. */
  return ia_blast(player, card, event, COLOR_RED);
}

int card_hymn_of_rebirth(int player, int card, event_t event){
	/* Hymn of Rebirth	|3|G|W	0x2002DF7
	 * Sorcery
	 * Put target creature card from a graveyard onto the battlefield under your control. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance( player, card );
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
		   reanimate_permanent(player, card, instance->targets[0].player, selected, REANIMATE_DEFAULT);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 0, &this_test);
}

int card_ice_floe(int player, int card, event_t event)
{
  /* Ice Floe	""	0x200d8a6
   * Land
   * You may choose not to untap ~ during your untap step.
   * |T: Tap target creature without flying that's attacking you. It doesn't untap during its controller's untap step for as long as ~ remains tapped. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_ABILITY = 2
  };



  if (event == EVENT_RESOLVE_SPELL)
	play_land_sound_effect(player, card);

  if (!is_humiliated(player, card, AL_STATIC)){ choose_to_untap(player, card, event); }

  if (event == EVENT_COUNT_MANA && mana_colors_added[player] && !is_tapped(player, card) &&
      !is_animated_and_sick(player, card) && can_produce_mana(player, card)){
    declare_mana_available_maybe_hex(player, mana_colors_added[player], 1);
    return 0;
  }
  if (!IS_GAA_EVENT(event)){
    return mana_colors_added[player] ? mana_producer(player, card, event) : 0;
  }
  if (event == EVENT_CAN_ACTIVATE && mana_colors_added[player]){
    return mana_producer(player, card, event);
  }

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = 1-player;
  td.required_state = TARGET_STATE_ATTACKING;
  td.illegal_abilities |= KEYWORD_FLYING;

  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_ACTIVATE){
    if (!mana_colors_added[player]){ instance->info_slot = CHOICE_ABILITY; }
    else if (paying_mana() || !generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
        GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(1), 0, &td,
        "Select target creature without flying that's attacking you.")){
      instance->info_slot = CHOICE_MANA;
      mana_producer(player, card, event);
    } else {
      instance->info_slot = CHOICE_ABILITY;
      int choice = IS_AI(player) ? CHOICE_ABILITY : DIALOG(player, card, event, DLG_RANDOM,
          "Get mana", 1, 1, "Use Ability", 1, 1);
      if (!choice){ spell_fizzled = 1; return 0; }
      if (choice == CHOICE_MANA){ mana_producer(player, card, event); }
      else if (choice == CHOICE_ABILITY){
        generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
            MANACOST0, 0, &td, "Select target creature without flying that's attacking you.");
      }
    }
  }

  if (event == EVENT_RESOLVE_ACTIVATION && validate_target(player, card, &td, 0) && instance->info_slot == CHOICE_ABILITY)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  card_instance_t* parent = in_play(inst->parent_controller, inst->parent_card);
	  if (parent)
		{
		  parent->targets[1] = inst->targets[0];	// struct copy
		  does_not_untap_until_im_tapped(inst->parent_controller, inst->parent_card,
										 inst->targets[0].player, inst->targets[0].card);
		}
	  tap_card(inst->targets[0].player, inst->targets[0].card);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST0, 0,
								   &td, "Select target creature without flying that's attacking you.");
}

int card_iceberg(int player, int card, event_t event)
{
  /* Iceberg	|X|U|U	0x2009ca6
   * Enchantment
   * ~ enters the battlefield with X ice counters on it.
   * |3: Put an ice counter on ~.
   * Remove an ice counter from ~: Add |1 to your mana pool. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_COUNTER = 2
  };

  card_instance_t *instance;
  card_instance_t *entry_record;
  int ice_count;

  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 4, NULL, NULL, 0, NULL);
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      if ((player == 1) && (x_value < 1))
      {
        ai_modifier = ai_modifier + -0x24;
      }
      instance->info_slot = x_value;
    }
    entry_record = get_card_instance(player, card);
    enters_the_battlefield_with_counters(player, card, EVENT_CAST_SPELL, COUNTER_ICE,
                                         entry_record->info_slot);
  }
  else
  {
    entry_record = get_card_instance(player, card);
    enters_the_battlefield_with_counters(player, card, event, COUNTER_ICE, entry_record->info_slot);
    if ((event == EVENT_RESOLVE_SPELL) && (player == 1))
    {
      if (((unsigned char)trace_mode & 2) == 0)
      {
        entry_record = get_card_instance(1, card);
        entry_record->state = entry_record->state | 0x40000;
      }
    }
    else
    {
      if (event == EVENT_SHOULD_AI_PLAY)
      {
        ice_count = count_counters(player, card, COUNTER_ICE);
        ai_modifier = ai_modifier + ice_count * 0xc;
        return 0;
      }
      if (event == EVENT_COUNT_MANA)
      {
        if (card != affected_card)
        {
          return 0;
        }
        if (player != affected_card_controller)
        {
          return 0;
        }
        if (can_produce_mana(player, card) == 0)
        {
          return 0;
        }
        declare_mana_available(player, 0, count_counters(player, card, COUNTER_ICE));
        return 0;
      }
    }
  }
  if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_ACTIVATE))
  {
    return 0;
  }

  unsigned int can_generate = 0;
  unsigned int can_add = 0;
  bool direct_mana = false;
  if (!can_produce_mana(player, card))
  {
    if (!paying_mana())
      can_add = can_use_activated_abilities(player, card) != 0;
  }
  else
  {
    int counters = count_counters(player, card, COUNTER_ICE);
    can_generate = counters > 0;
    int paying = paying_mana();
    direct_mana = event == EVENT_ACTIVATE && counters > 0;
    if (!paying)
      can_add = can_use_activated_abilities(player, card) != 0;
  }
  bool generate = false;
  unsigned int priority;
  if (direct_mana)
  {
    if (paying_mana())
    {
      instance->info_slot = 1;
      generate = true;
    }
    else
      priority = paying_mana() == 0 ? 0xffffffff : 3;
  }
  else
    priority = paying_mana() == 0 ? 0xffffffff : 3;
  if (!generate)
  {
    int choice =
        dialog_impl(player, card, event, "Generate mana", can_generate, priority, "Add ice counter",
                    can_add, 1, DLG_MANA6(3, 0, 0, 0, 0, 0), 0, 0, 0, 0);
    if (event == EVENT_CAN_ACTIVATE)
      return choice;
    if (event != EVENT_ACTIVATE)
    {
      if (choice == CHOICE_COUNTER && in_play(instance->parent_controller, instance->parent_card))
        add_counters(player, card, player, card, COUNTER_ICE, 1);
      return 0;
    }
    if (choice != CHOICE_MANA)
      return 0;
  }
  ai_modifier -= 0xc;
  remove_counters(player, card, player, card, COUNTER_ICE, 1);
  produce_mana(player, 0, 1);
  tapped_for_mana_color = -2;
  return 0;
}

int card_icequake(int player, int card, event_t event){
	/* Icequake	|1|B|B	0x2002DFC
	 * Sorcery
	 * Destroy target land. If that land was a snow land, ~ deals 1 damage to that land's controller. */

  return ia_land_destructor(player, card, event, 0);
}

/* Lava Tubes	""	=>card_land_cap
 * Land
 * ~ doesn't untap during your untap step if it has a depletion counter on it.
 * At the beginning of your upkeep, remove a depletion counter from ~.
 * |T: Add |B or |R to your mana pool. Put a depletion counter on ~. */

int card_icy_prison(int player, int card, event_t event){
	/* Icy Prison	|U|U	0x2002E06
	 * Enchantment
	 * When ~ enters the battlefield, exile target creature.
	 * At the beginning of your upkeep, sacrifice ~ unless any player pays |3.
	 * When ~ leaves the battlefield, return the exiled card to the battlefield under its owner's control. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		card_instance_t *instance = get_card_instance(player, card);
		int kill_me = 1;
		APNAP(p,{
					if( has_mana(p, COLOR_COLORLESS, 3) && kill_me){
						int ai_choice = instance->targets[7].player == p ? 1 : 0;
						if( do_dialog(p, player, card, -1, -1, " Pay upkeep\n Pass", ai_choice) == 0 ){
							charge_mana(p, COLOR_COLORLESS, 3);
							if( spell_fizzled != 1 ){
								kill_me = 0;
							}
						}
					}
				};
		);
		if( kill_me ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return card_journey_to_nowhere(player, card, event);
}

int card_illusionary_forces(int player, int card, event_t event)
{
	/* Illusionary Forces	|3|U	0x2002E0B
	 * Creature - Illusion 4/4
	 * Flying
	 * Cumulative upkeep |U */

	cumulative_upkeep(player, card, event, MANACOST_U(1));
	return 0;
}

int card_illusionary_terrain(int player, int card, event_t event)
{
  /* Illusionary Terrain	|U|U	0x200d8ab
   * Enchantment
   * Cumulative upkeep |2
   * As ~ enters the battlefield, choose two basic land types.
   * Basic lands of the first chosen type are the second chosen type. */

  cumulative_upkeep(player, card, event, MANACOST_X(2));

  if (event == EVENT_RESOLVE_SPELL)
	{
	  int color1 = choose_a_land(player, COLOR_BLACK + recorded_rand(player, COLOR_WHITE - COLOR_BLACK + 1));
	  int color2 = choose_a_land(player, COLOR_BLACK + recorded_rand(player, COLOR_WHITE - COLOR_BLACK + 1));

	  int landtypes[6] = { 0, SUBTYPE_SWAMP, SUBTYPE_ISLAND, SUBTYPE_FOREST, SUBTYPE_MOUNTAIN, SUBTYPE_PLAINS };

	  card_instance_t* inst = get_card_instance(player, card);
	  inst->targets[2].player = landtypes[color1] | ((CARD_ID_ILLUSIONARY_TERRAIN & 0xFFFF) << 16);
	  inst->targets[2].card = landtypes[color2] | ((CARD_ID_ILLUSIONARY_TERRAIN & 0xFFFF) << 16);
	}

  if (event == EVENT_CHANGE_TYPE || event == EVENT_CLEANUP)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (HIWORD(inst->targets[2].player) == CARD_ID_ILLUSIONARY_TERRAIN
		  && HIWORD(inst->targets[2].card) == CARD_ID_ILLUSIONARY_TERRAIN
		  && LOWORD(inst->targets[2].player) != LOWORD(inst->targets[2].card))
		change_lands_into_new_land_type(player, card, event, LOWORD(inst->targets[2].player),
										MATCH|STATUS_BASICLAND_DEPENDANT|TARGET_TYPE_HACK_SLEIGHT_LEGACY,
										LOWORD(inst->targets[2].card));
	}

  return global_enchantment(player, card, event);
}

/* Lure	|1|G|G	=>unlimited.c:card_lure
 * Enchantment - Aura
 * Enchant creature
 * All creatures able to block enchanted creature do so. */

int card_illusions_of_grandeur(int player, int card, event_t event)
{
  /* Illusions of Grandeur	|3|U	0x2001CE0
   * Enchantment
   * Cumulative upkeep |2
   * When ~ enters the battlefield, you gain 20 life.
   * When ~ leaves the battlefield, you lose 20 life. */

  int result;
  result = is_unlocked(player, card, event, 0x15);
  if (result == 0)
  {
    return 0;
  }
  cumulative_upkeep(player, card, event, 2, 0, 0, 0, 0, 0);
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    gain_life(player, 0x14);
    leaves_play(player, card, EVENT_ETB, 1, 1);
    return global_enchantment(player, card, event);
  }
  leaves_play(player, card, event, 1, 1);
  if (event != EVENT_LEAVES_PLAY_ABILITY)
  {
    if (event != EVENT_SHOULD_AI_PLAY)
      return global_enchantment(player, card, event);
    result = is_stolen(player, card);
    if (result == 0)
      return global_enchantment(player, card, event);
  }
  lose_life(player, 0x14);
  return global_enchantment(player, card, event);
}

int card_imposing_visage(int player, int card, event_t event)
{
  /* Imposing Visage	|R	0x200d842
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has menace. */

  card_instance_t* inst = in_play(player, card);
  if (inst && inst->damage_target_player >= 0)
	menace(inst->damage_target_player, inst->damage_target_card, event);

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS)
    return vanilla_aura(player, card, event, player);
  return 0;
}

int card_incinerate(int player, int card, event_t event){
	/* Incinerate	|1|R	0x20017e0
	 * Instant
	 * ~ deals 3 damage to any target. A creature dealt damage this way can't be regenerated this turn. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *instance = get_card_instance( player, card );
			int dc = damage_target0(player, card, 3);
			if( instance->targets[0].card != -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
				get_card_instance(player, dc)->targets[3].card |= DMG_CANT_REGENERATE_IF_DEALT_DAMAGE_THIS_WAY;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x23edb50; original cards/ice_age.c:4997. */
static int can_pay_1_life_b_thunk(int player, int card, int number_of_age_counters)
{
  int result;

  result = can_pay_life(player, number_of_age_counters);
  if (result == 0)
  {
    return 0;
  }
  result = has_mana(player, 1, number_of_age_counters);
  return (unsigned int)(result != 0);
}

/* Melee	|4|R	0x000000	[Odric, Master Tactician]
 * Instant
 * Cast ~ only during your turn and only during combat before blockers are declared.
 * You choose which creatures block this combat and how those creatures block.
 * Whenever a creature attacks and isn't blocked this combat, untap it and remove it from combat. */

/* Melting	|3|R	0x000000
 * Enchantment
 * All lands are no longer snow. */

/* Mercenaries	|3|W	0x000000
 * Creature - Human Mercenary 3/3
 * |3: The next time ~ would deal damage to you this turn, prevent that damage. Any player may activate this ability. */

/* ManalinkEh 2022: 0x23ed010; original cards/ice_age.c:5001. */
static int lose_1_life_b_thunk(int player, int card, int number_of_age_counters)
{
  charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_BLACK,
                              number_of_age_counters);
  lose_life(player, number_of_age_counters);
  return 1;
}

/* ManalinkEh 2022: 0x23fd160; original cards/ice_age.c:5008. */
int card_infernal_darkness(int player, int card, event_t event)
{
  /* Infernal Darkness	|2|B|B	0x000000
   * Enchantment
   * Cumulative upkeep-Pay |B and 1 life.
   * If a land is tapped for mana, it produces |B instead of any other type. */

  int p;

  cumulative_upkeep_general(player, card, event, can_pay_1_life_b_thunk, lose_1_life_b_thunk);
  if (event == EVENT_CAST_SPELL)
  {
    if (is_what(affected_card_controller, affected_card, 1) != 0)
    {
      set_special_flags2(affected_card_controller, affected_card, 0x40);
    }
  }
  else if ((event == EVENT_MODIFY_MANA_PROD) || (event == EVENT_RESOLVE_SPELL))
  {
    for (p = 0; p < 2; ++p)
    {
      int initial_count = active_cards_count[p];
      for (int color = 0; color < 7; ++color)
        mana_color_override[p][color] = 1;
      if (initial_count > 0)
        for (int land = 0; land < active_cards_count[p]; ++land)
          if (in_play(p, land) && is_what(p, land, 1))
          {
            set_special_flags2(p, land, 0x40);
          }
    }
  }
  return global_enchantment(player, card, event);
}

int card_infuse(int player, int card, event_t event)
{
  /* Infuse	|2|U	0x200d7d4
   * Instant
   * Untap target artifact, creature, or land.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_LAND);
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  if (player == AI && !(get_card_instance(inst->targets[0].player, inst->targets[0].card)->state & STATE_TAPPED))
			ai_modifier -= 48;
		  untap_card(inst->targets[0].player, inst->targets[0].card);
		  cantrip(player, card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TWIDDLE", 1, NULL);
}

int card_jesters_cap(int player, int card, event_t event){
	/* Jester's Cap	|4	0x2001808
	 * Artifact
	 * |2, |T, Sacrifice ~: Search target player's library for three cards and exile them. Then that player shuffles his or her library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			int amount = MIN(3, count_deck(instance->targets[0].player));
			if( amount ){
				char msg[100] = "Select a card to exile.";
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, msg);
				this_test.qty = amount;
				new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
			}
			shuffle(instance->targets[0].player);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_X(2), 0,
									&td1, "TARGET_PLAYER");
}

int card_jesters_mask(int player, int card, event_t event){
	/* Jester's Mask	|5	0x2002E10
	 * Artifact
	 * ~ enters the battlefield tapped.
	 * |1, |T, Sacrifice ~: Target opponent puts the cards from his or her hand on top of his or her library.
	 * 		Search that player's library for that many cards. That player puts those cards into his or her hand, then shuffles his or
	 *		her library. */

	comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			int amount = 0;
			int count = active_cards_count[1-player];
			while( count > -1 ){
					if( in_hand(1-player, count) ){
						 put_on_top_of_deck(1-player, count);
						 amount++;
					}
					count--;
			}
			int crds = MIN(amount, count_deck(1-player));
			if( crds ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, 0, "Select a card to put in opponent's hand.");
				this_test.qty = crds;
				new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_TPLAYER_HAND, 1, AI_MIN_VALUE, &this_test);
			}
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_ONLY_TARGET_OPPONENT|GAA_SACRIFICE_ME, MANACOST_X(1), 0,
									&td, "TARGET_PLAYER");
}

/* ManalinkEh 2022: 0x23fddb0; original cards/ice_age.c:5293. */
int card_jeweled_amulet(int player, int card, event_t event)
{
  /* Jeweled Amulet	|0	0x000000
   * Artifact
   * |1, |T: Put a charge counter on ~. Note the type of mana spent to pay this activation cost. Activate this ability only if there are no charge counters on ~.
   * |T, Remove a charge counter from ~: Add one mana of ~'s last noted type to your mana pool. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_COUNTER = 2
  };

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);
  if (event == EVENT_COUNT_MANA)
  {
    if (card != affected_card)
    {
      return 0;
    }
    if (player != affected_card_controller)
    {
      return 0;
    }
    result = can_produce_mana(player, card);
    if (result == 0)
    {
      return 0;
    }
    result = count_counters(player, card, COUNTER_CHARGE);
    if (result == 0)
    {
      return 0;
    }
    declare_mana_available(player, instance->targets[1].player, 1);
    return 0;
  }
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE)
  {
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
    if (instance->info_slot != 2)
    {
      return 0;
    }
    add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_CHARGE,
                 1);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    if ((result != 0) && (result = count_counters(player, card, COUNTER_CHARGE), result == 0))
    {
      return 1;
    }
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0,
                                       0x17b0100, NULL, NULL);
    if (result == 0)
    {
      return 0;
    }
    result = can_produce_mana(player, card);
    return (unsigned int)(result != 0);
  }

  bool direct_mana = false;
  bool can_charge = false;
  unsigned int can_generate = 0;
  result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
  if (result == 0)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0,
                                       0x17b0100, NULL, NULL);
    if (result == 0)
      direct_mana = true;
    else
      can_generate = can_produce_mana(player, card) != 0;
  }
  else
  {
    can_charge = count_counters(player, card, COUNTER_CHARGE) == 0;
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0,
                                       0x17b0100, NULL, NULL);
    if (result != 0)
      can_generate = can_produce_mana(player, card) != 0;
  }
  instance->info_slot = 1;
  result = paying_mana();
  if (!direct_mana && result == 0 && can_charge)
  {
    result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Get Mana",
                         can_generate, 5, "Add Charge counter", 1, 10, 0, 0, 0, 0);
    if (result == 0)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (result != CHOICE_MANA)
    {
      if (result != CHOICE_COUNTER)
      {
        return 0;
      }

      generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0, NULL, NULL);
      result = 0;
      do
      {
        if (mana_paid[result] != 0)
        {
          instance->targets[1].player = result;
        }
        result = result + 1;
      } while (result != 6);
      return 0;
    }
  }
  remove_counters(player, card, player, card, COUNTER_CHARGE, 1);
  produce_mana_tapped(player, card, instance->targets[1].player, 1);
  return 0;
}

int card_johtull_wurm(int player, int card, event_t event)
{
  /* Johtull Wurm	|5|G	0x200d8b0
   * Creature - Wurm 6/6
   * Whenever ~ becomes blocked, it gets -2/-1 until end of turn for each creature blocking it beyond the first. */

    if (event == EVENT_DECLARE_BLOCKERS && current_turn == player && !is_humiliated(player, card, AL_STATIC))
	{
	  int num_blocking = count_my_blockers(player, card);
	  if (num_blocking > 1)
		pump_until_eot(player, card, player, card, -2 * (num_blocking - 1), -(num_blocking - 1));
	}

  return 0;
}

int card_jokulhaups(int player, int card, event_t event)
{
	/* Jokulhaups	|4|R|R	0x200bae2
	 * Sorcery
	 * Destroy all artifacts, creatures, and lands. They can't be regenerated. */
	// Used externally
	if (event == EVENT_RESOLVE_SPELL){
		manipulate_type(player, card, ANYBODY, TYPE_ARTIFACT | TYPE_CREATURE | TYPE_LAND, KILL_BURY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x23fe430; original cards/ice_age.c:5389. */
int card_justice(int player, int card, event_t event)
{
  /* Justice	|2|W|W	0x000000
   * Enchantment
   * At the beginning of your upkeep, sacrifice ~ unless you pay |W|W.
   * Whenever a |Sred creature or spell deals damage, ~ deals that much damage to that creature's or spell's controller. */

  basic_upkeep(player, card, event, 0, 0, 0, 0, 0, 2);
  card_instance_t *damage = damage_being_dealt(event);
  if (!damage)
    return global_enchantment(player, card, event);
  unsigned int source_flags = damage->targets[3].player;
  bool red_creature = false;
  if (source_flags & 2)
  {
    char initial_color = damage->initial_color;
    unsigned int red = get_sleighted_color_test(player, card, 0x10);
    red_creature = ((int)initial_color & red) != 0;
    if (!red_creature)
      source_flags = damage->targets[3].player;
  }
  if (!red_creature && !(source_flags & 0x38))
    return global_enchantment(player, card, event);
  if (damage->info_slot > 0 && !is_humiliated(player, card, AL_TRIG))
    damage_player(damage->damage_source_player, damage->info_slot, player, card);
  return global_enchantment(player, card, event);
}

/* Moor Fiend	|3|B	=>vanilla
 * Creature - Horror 3/3
 * |H2Swampwalk */

/* Mountain	""	=>magic.exe:card_mountain
 * Basic Land - |H2Mountain */

/* Mountain Goat	|R	=>vanilla
 * Creature - Goat 1/1
 * |H2Mountainwalk */

/* ManalinkEh 2022: 0x23fe540; original cards/ice_age.c:5412. */
int card_karplusan_giant(int player, int card, event_t event)
{
  /* Karplusan Giant	|6|R	0x000000
   * Creature - Giant 3/3
   * Tap an untapped snow land you control: ~ gets +1/+1 until end of turn. */

  int parent_card;
  int result;
  card_instance_t *instance;
  target_definition_t td;

  base_target_definition(player, card, &td, 1);
  td.required_subtype = 0x2003;
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.illegal_state = TARGET_STATE_TAPPED;
  if ((uint32_t)(event - EVENT_POW_BOOST) < 2)
  {
    td.illegal_abilities = 0;
    result = target_available(player, card, &td);
    if (result < 1)
    {

      return 0;
    }
    return generic_shade_amt_can_pump(player, card, 1, 1, 0, 0, 0, 0, 0, 0, result);
  }
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      result = generic_activated_ability(player, card, event, 6, 0, 0, 0, 0, 0, 0, 0, &td,
                                         "Select an untapped snow land you control.");
      if (event != EVENT_ACTIVATE)
      {
        return result;
      }
      if (spell_fizzled != 1)
      {

        instance = get_card_instance(player, card);
        tap_card(instance->targets[0].player, instance->targets[0].card);
        instance->number_of_targets = '\0';
        return result;
      }
      return result;
    }
  }
  else if (event != EVENT_RESOLVE_ACTIVATION)
  {
    if (event != EVENT_CLEANUP)
    {
      return 0;
    }
    return generic_activated_ability(player, card, event, 6, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "Select an untapped snow land you control.");
  }

  instance = get_card_instance(player, card);
  result = instance->parent_controller;
  parent_card = instance->parent_card;
  instance = in_play(result, parent_card);
  if (instance != NULL)
  {
    pump_until_eot_merge_previous(result, parent_card, result, parent_card, 1, 1);
  }
  return generic_activated_ability(player, card, event, 6, 0, 0, 0, 0, 0, 0, 0, &td,
                                   "Select an untapped snow land you control.");
}
int card_karplusan_yeti(int player, int card, event_t event)
{
  /* Karplusan Yeti	|3|R|R	0x200d7e3
   * Creature - Yeti 3/3
   * |T: ~ deals damage equal to its power to target creature. That creature deals damage equal to its power to ~. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  // Not fight, since the specific keyword's not used - in particular, it'll still happen even if the Yeti's no longer on the bf
	  get_card_instance(inst->parent_controller, inst->parent_card)->regen_status |= KEYWORD_RECALC_POWER;
	  int mypow = get_power(inst->parent_controller, inst->parent_card);
	  damage_creature(inst->targets[0].player, inst->targets[0].card, mypow, inst->parent_controller, inst->parent_card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		{
		  get_card_instance(inst->targets[0].player, inst->targets[0].card)->regen_status |= KEYWORD_RECALC_POWER;
		  int hispow = get_power(inst->targets[0].player, inst->targets[0].card);
		  damage_creature(inst->parent_controller, inst->parent_card, hispow, inst->targets[0].player, inst->targets[0].card);
		}
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_kelsinko_ranger(int player, int card, event_t event)
{
  /* Kelsinko Ranger	|W	0x200d7e8
   * Creature - Human 1/1
   * |1|W: Target |Sgreen creature gains first strike until end of turn. */

  if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player
	  && card_kelsinko_ranger(player, card, EVENT_CAN_ACTIVATE)
	  && (get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_GREEN))){
	check_abilities_keywords |= KEYWORD_FIRST_STRIKE;
  }

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  pump_ability_until_eot(player, card, inst->targets[0].player, inst->targets[0].card, 0,0, KEYWORD_FIRST_STRIKE,0);
	}

  return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_XW(1,1), 0, &td,
								   get_sleighted_color_text(player, card, "Select target %s creature.", COLOR_GREEN));
}

int card_kjeldoran_dead(int player, int card, event_t event){
	/* Kjeldoran Dead	|B	0x2002E15
	 * Creature - Skeleton 3/1
	 * When ~ enters the battlefield, sacrifice a creature.
	 * |B: Regenerate ~. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		if( ! new_can_sacrifice(player, card, player, &test) ){
			ai_modifier-=1000;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		player_sacrifices_a_permanent(player, card, player, TYPE_CREATURE, SAC_CAUSED | SAC_NO_CANCEL);
	}

	return regeneration(player, card, event, MANACOST_B(1));
}

/* ManalinkEh 2022: 0x23fed20; original cards/ice_age.c:5542. */
int card_kjeldoran_elite_guard(int player, int card, event_t event)
{
  /* Kjeldoran Elite Guard	|3|W	0x000000	[heart_wolf]
   * Creature - Human Soldier 2/2
   * |T: Target creature gets +2/+2 until end of turn. When that creature leaves the battlefield this turn, sacrifice ~. Activate this ability only during combat. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_CAN_ACTIVATE && current_phase - 0x15U > 8)
    return 0;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      result = pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                                      instance->targets[0].player, instance->targets[0].card, 2, 2,
                                      0, 0);
      if (result != -1)
      {
        instance = get_card_instance(player, result);
        instance->targets[3].player = 0x220;
        instance->targets[3].card = 3;
      }
    }
  }
  return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}

/* ManalinkEh 2022: 0x23ede70; original cards/ice_age.c:5576. */
static int kjeldoran_frostbeast_legacy(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (!end_of_combat_trigger(player, card, event, 2))
  {
    return 0;
  }
  instance = get_card_instance(player, card);
  kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  kill_card(player, card, KILL_EXILE);
  return 0;
}

/* ManalinkEh 2022: 0x23eb8a0; original cards/ice_age.c:5587. */
static void attach_kjeldoran_frostbeast_legacy(int player, int card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, kjeldoran_frostbeast_legacy, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x23feed0; original cards/ice_age.c:5592. */
int card_kjeldoran_frostbeast(int player, int card, event_t event)
{
  /* Kjeldoran Frostbeast	|3|G|W	0x000000
   * Creature - Elemental Beast 2/4
   * At end of combat, destroy all creatures blocking or blocked by ~. */

  card_instance_t *instance;

  if (event == EVENT_CHANGE_TYPE)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      if (!is_humiliated(player, card, AL_TRIG))
      {
        instance = get_card_instance(player, card);
        instance->destroys_if_blocked = instance->destroys_if_blocked | 0xc;
      }
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {

      instance = get_card_instance(player, card);
      if ((current_turn == player) && ((instance->state & 4) != 0))
      {
        for_each_creature_blocking_me(current_turn, card, attach_kjeldoran_frostbeast_legacy,
                                      current_turn, card);
      }
      if ((1 - current_turn == player) && ((instance->state & 8) != 0))
      {
        for_each_creature_blocked_by_me(player, card, attach_kjeldoran_frostbeast_legacy, player,
                                        card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23ff010; original cards/ice_age.c:5615. */
int card_kjeldoran_guard(int player, int card, event_t event)
{
  /* Kjeldoran Guard	|1|W	0x000000	[heart_wolf]
   * Creature - Human Soldier 1/1
   * |T: Target creature gets +1/+1 until end of turn. When that creature leaves the battlefield this turn, sacrifice ~. Activate this ability only during combat and only if defending player controls no snow lands. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE && event != EVENT_ACTIVATE && event != EVENT_RESOLVE_ACTIVATION &&
      event != EVENT_CLEANUP)
    return 0;
  default_target_definition(player, card, &td, 2);
  if (event == EVENT_CAN_ACTIVATE)
  {
    if (current_phase - 0x15U > 8 || control_a_snow_land(1 - current_turn, 0))
      return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE)
    return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    result =
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card, 1, 1, 0, 0);
    if (result != -1)
    {
      instance = get_card_instance(player, result);
      instance->targets[3].player = 0x220;
      instance->targets[3].card = 3;
    }
  }
  return 0;
}

int card_kjeldoran_knight(int player, int card, event_t event)
{
  /* Kjeldoran Knight	|W|W	0x200d806
   * Creature - Human Knight 1/1
   * Banding
   * |1|W: ~ gets +1/+0 until end of turn.
   * |W|W: ~ gets +0/+2 until end of turn. */

  if (event == EVENT_POW_BOOST)
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_XW(1,1),	1,0);
  if (event == EVENT_TOU_BOOST)
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_W(2),	0,2);

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !can_use_activated_abilities(player, card))
		return 0;

	  enum
	  {
		CHOICE_POW = 1,
		CHOICE_TGH
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"+1/+0", 1, 1, DLG_MANA(MANACOST_XW(1,1)),
						"+0/+2", 1, 1, DLG_MANA(MANACOST_W(2)));

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		switch (choice)
		  {
			case CHOICE_POW:
			  return generic_shade_merge_pt(player, card, event, 0, MANACOST_XW(1,1),	1,0);
			case CHOICE_TGH:
			  return generic_shade_merge_pt(player, card, event, 0, MANACOST_W(2),	0,2);
		  }
	}

  return 0;
}

static int effect_royal_guard(int player, int card, event_t event)
{

  if (get_card_instance(player, card)->targets[0].player > -1)
  {
    card_instance_t *instance = get_card_instance(player, card);
    card_instance_t *damage = combat_damage_being_dealt(event);

    if (damage && damage->damage_target_player == instance->targets[1].player &&
        damage->damage_target_card == -1 &&
        !check_special_flags(damage->damage_source_player, damage->damage_source_card,
                             SF_ATTACKING_PWALKER) &&
        (damage->targets[3].player & TYPE_CREATURE) && // probably redundant to status check
        is_attacking(damage->damage_source_player, damage->damage_source_card) &&
        is_unblocked(damage->damage_source_player, damage->damage_source_card))
    {
      damage->damage_target_player = instance->targets[0].player;
      damage->damage_target_card = instance->targets[0].card;
    }

    other_leaves_play(player, card, instance->targets[0].player, instance->targets[0].card, event,
                      0, 0);
    if (event == EVENT_CLEANUP || event == EVENT_LEAVES_PLAY_ABILITY)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }

  return 0;
}

// Called from lose_life(); it's too long to inline there.
int card_kjeldoran_royal_guard(int player, int card, event_t event){
	/* Kjeldoran Royal Guard	|3|W|W	0x2002E1A
	 * Creature - Human Soldier 2/5
	 * |T: All combat damage that would be dealt to you by unblocked creatures this turn is dealt to ~ instead. */

	if( event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);
		int legacy = create_legacy_effect(instance->parent_controller, instance->parent_card, &effect_royal_guard);
		card_instance_t *leg = get_card_instance(player, legacy);
		leg->targets[0].player = instance->parent_controller;
		leg->targets[0].card = instance->parent_card;
		leg->targets[1].player = instance->parent_controller;
		leg->number_of_targets = 1;
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, NULL, NULL);
}

int card_krovikan_elementalist(int player, int card, event_t event)
{
	/* Krovikan Elementalist	|B|B	0x2009cab
	 * Creature - Human Wizard 1/1
	 * |2|R: Target creature gets +1/+0 until end of turn.
	 * |U|U: Target creature you control gains flying until end of turn. Sacrifice it at the beginning of the next end step. */

  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION)
	{
	  if (event == EVENT_CAN_ACTIVATE && !can_use_activated_abilities(player, card))
		return 0;

	  target_definition_t td_pump;
	  default_target_definition(player, card, &td_pump, TYPE_CREATURE);
	  td_pump.preferred_controller = player;

	  target_definition_t td_fly;
	  default_target_definition(player, card, &td_fly, TYPE_CREATURE);
	  td_fly.preferred_controller = player;
	  td_fly.allowed_controller = player;
	  if (player == AI)
		td_fly.illegal_abilities |= KEYWORD_FLYING;

	  int ai_fly = current_phase < PHASE_DECLARE_BLOCKERS ? 2 : -1;

	  enum
	  {
		CHOICE_PUMP = 1,
		CHOICE_FLY
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"+1/+0", 1, 3,						DLG_MANA(MANACOST_XR(2,1)),	DLG_TARGET(&td_pump, "TARGET_CREATURE"),
						"Flying and sacrifice", 1, ai_fly,	DLG_MANA(MANACOST_U(2)),	DLG_TARGET(&td_fly, "ASHNODS_BATTLEGEAR"));	// target creature you control

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  switch (choice)
			{
			  case CHOICE_PUMP:
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 1,0);
				break;

			  case CHOICE_FLY:
				;int legacy = pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
													 0,0, KEYWORD_FLYING, SP_KEYWORD_DIE_AT_EOT);
				card_instance_t* leg = get_card_instance(player, legacy);
				leg->targets[3].card = KILL_SACRIFICE;
				break;
			}
		}
	}

  return 0;
}

int card_krovikan_fetish(int player, int card, event_t event)
{
  /* Krovikan Fetish	|2|B	0x200d757
   * Enchantment - Aura
   * Enchant creature
   * When ~ enters the battlefield, draw a card at the beginning of the next turn's upkeep.
   * Enchanted creature gets +1/+1. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		cantrip(player, card, 1);

	return generic_aura(player, card, event, player, 1, 1, 0, 0, 0, 0, 0);
}

/* Orcish Cannoneers	|1|R|R	=>unlimited.c:card_orcish_artillery
 * Creature - Orc Warrior 1/3
 * |T: ~ deals 2 damage to target creature or player and 3 damage to you. */

/* Orcish Conscripts	|R	0x000000
 * Creature - Orc 2/2
 * ~ can't attack unless at least two other creatures attack.
 * ~ can't block unless at least two other creatures block. */

/* ManalinkEh 2022: 0x23ffa00; original cards/ice_age.c:5842. */
int card_krovikan_sorcerer(int player, int card, event_t event)
{
  /* Krovikan Sorcerer	|2|U	0x000000
   * Creature - Human Wizard 1/1
   * |T, Discard a non|Sblack card: Draw a card.
   * |T, Discard a |Sblack card: Draw two cards, then discard one of them. */

  enum
  {
    CHOICE_DISCARD_NON_BLACK = 1,
    CHOICE_DISCARD_BLACK = 2
  };

  int i;
  int choice;
  int32_t drawn_iid;
  card_instance_t *instance;
  int drawn_card;
  const char *color_prompt;
  int selected_or_mode;
  unsigned int drawn_count;
  test_definition_t *discard_test;
  test_definition_t this_test;
  test_definition_t this_test2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {

      color_prompt = get_sleighted_color_text(player, card, "Select a non %s ", 1);
      discard_test = &this_test;
      new_default_test_definition(discard_test, 0xff, color_prompt);
      this_test.color = get_sleighted_color_test(player, card, 2);
      this_test.color_flag = 1;
      this_test.zone = 0x100;
      color_prompt = get_sleighted_color_text(player, card, "Select a %s ", 1);
      new_default_test_definition(&this_test2, 0xff, color_prompt);
      this_test2.color = get_sleighted_color_test(player, card, 2);
      this_test2.color_flag = 0;
      this_test2.zone = 0x100;
      selected_or_mode = check_battlefield_for_special_card(player, card, player, 0, discard_test);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Discard a non-black card",
                           selected_or_mode, 5, "Discard a black card",
                           check_battlefield_for_special_card(player, card, player, 0, &this_test2),
                           10, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = choice;
      if (choice == CHOICE_DISCARD_NON_BLACK)
      {
        if (generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                      NULL) == 0)
        {
          return 0;
        }
      }
      else
      {
        if (choice != CHOICE_DISCARD_BLACK)
        {
          return 0;
        }
        if (generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                      NULL) == 0)
        {
          return 0;
        }

        discard_test = &this_test2;
      }
      selected_or_mode = new_select_a_card(player, player, 4, 0, 3, -1, discard_test);
      if (selected_or_mode == -1)
      {
        untap_card_no_event(player, card);
        spell_fizzled = 1;
        return 0;
      }
      discard_card(player, selected_or_mode);
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
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  selected_or_mode = instance->info_slot;
  if (selected_or_mode == 1)
  {
    draw_cards(player, 1);
    selected_or_mode = instance->info_slot;
  }
  if (selected_or_mode != 2)
  {
    return 0;
  }
  drawn_count = 0xffffffff;
  int dtt[2][100];
  memset(dtt, 0, sizeof(dtt));
  i = 2;
  do
  {

    drawn_card = draw_a_card(player);
    if ((-1 < drawn_card) && (instance = in_hand(player, drawn_card), instance != NULL))
    {
      drawn_count = drawn_count & (int)(drawn_count ^ 0xffffffff) >> 0x1f;
      instance = get_card_instance(player, drawn_card);
      drawn_iid = instance->internal_card_id;
      dtt[1][drawn_count] = drawn_card;
      dtt[0][drawn_count] = drawn_iid;
      drawn_count = drawn_count + 1;
    }
    i = i + -1;
  } while (i != 0);
  new_default_test_definition(&this_test, 0xff, "Select a card to discard.");
  if ((int)drawn_count < 0)
  {
    return 0;
  }

  selected_or_mode =
      select_card_from_zone(player, player, dtt[0], drawn_count, 1, 3, -1, &this_test);
  if (selected_or_mode == -1)
  {
    return 0;
  }
  selected_or_mode = dtt[1][selected_or_mode];
  discard_card(player, selected_or_mode);
  return 0;
}

/* Order of the White Shield	|W|W	=>fallen_empires.c:card_order_of_leitbur
 * Creature - Human Knight 2/1
 * Protection from |Sblack
 * |W: ~ gains first strike until end of turn.
 * |W|W: ~ gets +1/+0 until end of turn. */

/* Pale Bears	|2|G	=>vanilla
 * Creature - Bear 2/2
 * |H2Islandwalk */

static int krovikan_vampire_legacy(int player, int card, event_t event)
{
  if (get_card_instance(player, card)->damage_target_player > -1)
  {

    card_instance_t *instance = get_card_instance(player, card);

    int killme = 0;

    other_leaves_play(player, card, instance->damage_source_player, instance->damage_source_card,
                      event, 0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
      killme = 1;

    if (event == EVENT_CARDCONTROLLED &&
        affect_me(instance->damage_source_player, instance->damage_source_card))
      killme = 1;

    if (killme)
      kill_card(instance->damage_target_player, instance->damage_target_card, KILL_SACRIFICE);
  }

  return 0;
}

int card_krovikan_vampire(int player, int card, event_t event){
/* Krovikan Vampire	|3|B|B	0x2009cb0
 * Creature - Vampire 3/3
 * At the beginning of each end step, if a creature dealt damage by ~ this turn died, put that card onto the battlefield under your control. Sacrifice it when you lose control of ~. */

	if( event == EVENT_DEAL_DAMAGE ){
		card_instance_t *instance = get_card_instance(player, card);
		card_instance_t *damage = get_card_instance( affected_card_controller, affected_card);
		if( damage->internal_card_id == damage_card && damage->info_slot > 0 ){
			if( damage->damage_source_card == card && damage->damage_source_player == player &&
				damage->damage_target_card != -1 && ! is_token(damage->damage_target_player, damage->damage_target_card) && is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE)
			  ){
				int pos = instance->info_slot;
				if( pos < 10 ){
					instance->targets[pos].player = damage->damage_target_player;
					instance->targets[pos].card = damage->damage_target_card;
					instance->info_slot++;
				}
			}
		}
	}

	if( event == EVENT_GRAVEYARD_FROM_PLAY ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! is_token(affected_card_controller, affected_card) && is_this_dying(affected_card_controller, affected_card) ){
			int i;
			for(i=0; i<instance->info_slot; i++){
				if( instance->targets[i].player == affected_card_controller && instance->targets[i].card == affected_card ){
					SET_BYTE0(instance->targets[i].player) = get_owner(affected_card_controller, affected_card);
					SET_BYTE1(instance->targets[i].player) = 1;
					instance->targets[i].card = get_original_internal_card_id(affected_card_controller, affected_card);
					break;
				}
			}
		}
	}

	if( eot_trigger(player, card, event) ){
		card_instance_t *instance = get_card_instance(player, card);
		int ids[2][10];
		int idc = 0;
		int i;
		for(i=0; i<instance->info_slot; i++){
			if( instance->targets[i].player > -1 ){
				if( BYTE1(instance->targets[i].player) ){
					ids[0][idc] = BYTE0(instance->targets[i].player);
					ids[1][idc] = cards_data[instance->targets[i].card].id;
					idc++;
				}
				instance->targets[i].player = instance->targets[i].card = -1;
			}
		}
		instance->info_slot = 0;
		if( idc ){
			for(i=0; i<idc; i++){
				int result = seek_grave_for_id_to_reanimate(player, card, ids[0][i], ids[1][i], REANIMATEXTRA_LEAVE_IN_GRAVEYARD);
				if (result != -1){
					reanimate_permanent_with_effect(player, card, ids[0][i], result, REANIMATE_DEFAULT, &krovikan_vampire_legacy);
				}
			}
		}
	}

	return 0;
}

int card_land_cap(int player, int card, event_t event)
{
  /* Land Cap	""	0x200d79d
   * Land
   * ~ doesn't untap during your untap step if it has a depletion counter on it.
   * At the beginning of your upkeep, remove a depletion counter from ~.
   * |T: Add |W or |U to your mana pool. Put a depletion counter on ~. */

  bool upkeep_check = trigger_condition == TRIGGER_UPKEEP;
  if (event == EVENT_UNTAP)
  {
    if (card != affected_card)
      return mana_producer(player, card, event);
    if (player == affected_card_controller)
    {
      if (count_counters(player, card, COUNTER_DEPLETION) > 0 &&
          !is_humiliated(player, card, AL_STATIC))
        does_not_untap(player, card, event);
      upkeep_check = trigger_condition == TRIGGER_UPKEEP;
      if (!upkeep_check)
        return mana_producer(player, card, event);
    }
    else
      upkeep_check = false;
  }
  if (upkeep_check && card == affected_card && affected_card_controller == player &&
      count_counters(player, card, COUNTER_DEPLETION) > 0)
    upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (!is_humiliated(player, card, AL_TRIG))
      remove_counters(player, card, player, card, COUNTER_DEPLETION, 1);
    return mana_producer(player, card, event);
  }
  int result = mana_producer(player, card, event);
  if (event == EVENT_ACTIVATE && spell_fizzled != 1)
    add_counters(player, card, player, card, COUNTER_DEPLETION, 1);
  return result;
}

int card_lapis_lazuli_talisman(int player, int card, event_t event)
{
  /* Lapis Lazuli Talisman	|2	0x200d888
   * Artifact
   * Whenever a player casts a |Sblue spell, you may pay |3. If you do, untap target permanent. */
  return talisman(player, card, event, COLOR_BLUE);
}

/* Plains	""	=>magic.exe:card_plains
 * Basic Land - |H2Plains */

int card_lava_burst(int player, int card, event_t event){
	/* Lava Burst	|X|R	0x200b4c5
	 * Sorcery
	 * ~ deals X damage to any target. If ~ would deal damage to a creature, that damage can't be prevented or dealt instead to another permanent or player. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER );
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( instance->targets[0].card != -1 && is_what(instance->targets[0].player, instance->targets[0].card, TYPE_CREATURE) ){
			int legacy = create_legacy_effect(player, card, &my_damage_cannot_be_prevented);
			get_card_instance(player, legacy)->targets[0].player = player;
			get_card_instance(player, legacy)->targets[0].card = card;
		}
		damage_creature( instance->targets[0].player, instance->targets[0].card, instance->info_slot, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL+GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}
/* ManalinkEh 2022: 0x24005a0; original cards/ice_age.c:6112. */
int card_legions_of_lim_dul(int player, int card, event_t event)
{
  /* Legions of Lim-Dul	|1|B|B	0x000000
   * Creature - Zombie 2/3
   * Snow |H2swampwalk */

  int result;
  subtype_t subt;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (((result == 0) && (attacking_card_controller == player)) && (attacking_card == card))
    {
      subt = get_hacked_subtype(player, card, 0x5009);
      result = count_snow_landtype(1 - current_turn, subt);
      if (result != 0)
      {
        event_result = 1;
      }
    }
  }
  return 0;
}
int card_leshracs_rite(int player, int card, event_t event)
{
  /* Leshrac's Rite	|B	0x200d75c
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has |H2swampwalk. */
  return generic_aura(player, card, event, player, 0,0, get_hacked_walk(player, card, KEYWORD_SWAMPWALK),0, 0, 0, 0);
}

/* ManalinkEh 2022: 0x24006a0; original cards/ice_age.c:6136. */
int card_leshracs_sigil(int player, int card, event_t event)
{
  /* Leshrac's Sigil	|B|B	0x000000
   * Enchantment
   * Whenever an opponent casts a |Sgreen spell, you may pay |B|B. If you do, look at that player's hand and choose a card from it. The player discards that card.
   * |B|B: Return ~ to its owner's hand. */

  int result;
  card_instance_t *instance;
  ec_definition_t ec;
  test_definition_t test;

  if ((((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      (reason_for_trigger_controller == player))
  {
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 8);
    sp_ability_trigger(1 - player, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    result = charge_mana_while_resolving(player, card, EVENT_SPA, player, COLOR_GREEN, 2);
    if (result != 0)
    {
      default_ec_definition(1 - player, player, &ec);
      default_test_definition(&test, 0xff);
      new_effect_coercion(&ec, &test);
    }
    return global_enchantment(player, card, event);
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0, 0, 2, 0, 0, 0, 0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return global_enchantment(player, card, event);
  instance = get_card_instance(player, card);
  bounce_permanent(instance->parent_controller, instance->parent_card);
  return generic_activated_ability(player, card, event, 0, 0, 2, 0, 0, 0, 0, 0, NULL, NULL);
}

/* Power Sink	|X|U	=>unlimited.c:card_power_sink
 * Instant
 * Counter target spell unless its controller pays |X. If he or she doesn't, that player taps all lands with mana abilities he or she controls and empties his or her mana pool. */

int card_lhurgoyf(int player, int card, event_t event)
{
  /* Lhurgoyf	|2|G|G	0x200ccdf
   * Creature - Lhurgoyf 100/101
   * ~'s power is equal to the number of creature cards in all graveyards and its toughness is equal to that number plus 1. */

  int result;
  if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = count_graveyard_by_type(2, 2);
    set_characteristic_pt(result + (unsigned int)(event != EVENT_POWER));
  }
  return 0;
}

/* Pygmy Allosaurus	|2|G	=>vanilla
 * Creature - Lizard 2/2
 * |H2Swampwalk */

int card_lightning_blow(int player, int card, event_t event)
{
  /* Lightning Blow	|1|W	0x200d793
   * Instant
   * Target creature gains first strike until end of turn.
   * Draw a card at the beginning of the next turn's upkeep. */
  return vanilla_instant_pump(player, card, event, ANYBODY, player, 0,0, VANILLA_PUMP_CANTRIP|KEYWORD_FIRST_STRIKE,0);
}

/* ManalinkEh 2022: 0x2400970; original cards/ice_age.c:6196. */
int card_lim_duls_cohort(int player, int card, event_t event)
{
  /* Lim-Dul's Cohort	|1|B|B	0x000000
   * Creature - Zombie 2/3
   * Whenever ~ blocks or becomes blocked by a creature, that creature can't be regenerated this turn. */

  int blocker_index_or_humiliated;
  card_instance_t *instance;
  int opponent;

  if ((event == EVENT_DECLARE_BLOCKERS) &&
      (blocker_index_or_humiliated = is_humiliated(player, card, AL_TRIG),
       blocker_index_or_humiliated == 0))
  {
    if (current_turn == player)
    {

      opponent = 1 - player;
      blocker_index_or_humiliated = active_cards_count[opponent] + -1;
      if (-1 < blocker_index_or_humiliated)
      {
        do
        {
          instance = in_play(opponent, blocker_index_or_humiliated);

          if (((instance != NULL) && (is_what(opponent, blocker_index_or_humiliated, 2) != 0)) &&
              (instance = get_card_instance(opponent, blocker_index_or_humiliated),
               (unsigned int)instance->blocking == card))
          {
            cannot_regenerate_until_eot(player, card, opponent, blocker_index_or_humiliated);
          }
          blocker_index_or_humiliated = blocker_index_or_humiliated + -1;
        } while (blocker_index_or_humiliated != -1);
      }
    }
    else
    {

      instance = get_card_instance(player, card);
      if (instance->blocking != 0xff)
      {
        cannot_regenerate_until_eot(player, card, 1 - player, (unsigned int)instance->blocking);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2400a90; original cards/ice_age.c:6224. */
int card_lim_duls_hex(int player, int card, event_t event)
{
  /* Lim-Dul's Hex	|1|B	0x000000
   * Enchantment
   * At the beginning of your upkeep, for each player, ~ deals 1 damage to that player unless he or she pays |B or |3. */

  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
    return global_enchantment(player, card, event);
  {
    int generic_available = has_mana(current_turn, 0, 3);
    int black_available = has_mana(current_turn, 1, 1);
    bool damage = !generic_available && !black_available;
    if (!damage)
    {
      int choice;
      if (current_turn == 0)
      {
        int black = has_mana(0, 1, 1);
        int generic = has_mana(current_turn, 0, 3);
        choice = dialog_impl(current_turn, card, EVENT_CAN_CAST, DLG_NO_STORAGE, "Pay 3", generic, 1,
                             "Pay B", black, 1, 0, 0, 0, 0);
      }
      else
        choice = black_available ? 2 : 1;
      if (choice == 0)
        damage = true;
      else
      {
        ldoubleclicked = 0;
        charge_mana_multi(current_turn, choice == 1 ? 3 : 0, choice == 2, 0, 0, 0, 0);
        damage = spell_fizzled == 1;
      }
    }
    if (damage)
      damage_player(current_turn, 1, player, card);
  }
  {
    int generic_available = has_mana(1 - current_turn, 0, 3);
    int black_available = has_mana(1 - current_turn, 1, 1);
    bool damage = !generic_available && !black_available;
    if (!damage)
    {
      int choice;
      if (current_turn == 1)
      {
        int black = has_mana(0, 1, 1);
        int generic = has_mana(1 - current_turn, 0, 3);
        choice = dialog_impl(1 - current_turn, card, EVENT_CAN_CAST, DLG_NO_STORAGE, "Pay 3", generic,
                             1, "Pay B", black, 1, 0, 0, 0, 0);
      }
      else
        choice = black_available ? 2 : 1;
      if (choice == 0)
        damage = true;
      else
      {
        ldoubleclicked = 0;
        charge_mana_multi(1 - current_turn, choice == 1 ? 3 : 0, choice == 2, 0, 0, 0, 0);
        damage = spell_fizzled == 1;
      }
    }
    if (damage)
      damage_player(1 - current_turn, 1, player, card);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2400f20; original cards/ice_age.c:6283. */
int card_lost_order_of_jarkeld(int player, int card, event_t event)
{
  /* Lost Order of Jarkeld	|2|W|W	0x000000
   * Creature - Human Knight 101/101
   * As ~ enters the battlefield, choose an opponent.
   * ~'s power and toughness are each equal to 1 plus the number of creatures the chosen player controls. */

  card_instance_t *instance;
  int result;

  if (card != -1)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      instance->targets[0].player = 1 - player;
    }
    instance = get_card_instance(player, card);
    if (-1 < instance->targets[0].player)
    {

      instance = get_card_instance(player, card);
      if ((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
          (player == affected_card_controller))
      {
        result = count_subtype(instance->targets[0].player, 2, -1);
        set_characteristic_pt(result + 1);
      }
    }
  }
  return 0;
}

int card_maddening_wind(int player, int card, event_t event)
{
  /* Maddening Wind	|2|G	0x200d775
   * Enchantment - Aura
   * Enchant creature
   * Cumulative upkeep |G
   * At the beginning of the upkeep of enchanted creature's controller, ~ deals 2 damage to that player. */

	cumulative_upkeep(player, card, event, MANACOST_G(1));
	damage_enchanted_permanent_controller_at_upkeep(player, card, event, 2);

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

/* ManalinkEh 2022: 0x23ed8d0; original cards/ice_age.c:6331. */
static int effect_tap_when_lose_control(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (event == EVENT_CARDCONTROLLED)
  {
    instance = get_card_instance(player, card);
    if (((instance != NULL) && (instance->damage_target_card == affected_card)) &&
        (instance->damage_target_player == affected_card_controller))
    {
      tap_card((int)instance->damage_target_player, instance->damage_target_card);
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

/* Reclamation	|2|G|W	0x000000
 * Enchantment
 * |SBlack creatures can't attack unless their controller sacrifices a land for each |Sblack creature he or she controls that's attacking. */

int card_magus_of_the_unseen(int player, int card, event_t event){
	/* Magus of the Unseen	|1|U	0x2002E24
	 * Creature - Human Wizard 1/1
	 * |1|U, |T: Untap target artifact an opponent controls and gain control of it until end of turn. It gains haste until end of turn. When you lose control of the artifact, tap it. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			untap_card(instance->targets[0].player, instance->targets[0].card);
			alternate_legacy_text(1, player, gain_control_until_eot_haste(player, card, instance->targets[0].player, instance->targets[0].card));
			alternate_legacy_text(2, player, create_targetted_legacy_effect(player, card, effect_tap_when_lose_control, instance->targets[0].player, instance->targets[0].card));
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_XU(1, 1), 0,
									&td, "Select target artifact your opponent controls.");
}

/* Regeneration	|1|G	=>unlimited.c:card_regeneration
 * Enchantment - Aura
 * Enchant creature
 * |G: Regenerate enchanted creature. */

/* River Delta	""	=>card_land_cap
 * Land
 * ~ doesn't untap during your untap step if it has a depletion counter on it.
 * At the beginning of your upkeep, remove a depletion counter from ~.
 * |T: Add |U or |B to your mana pool. Put a depletion counter on ~. */

int card_malachite_talisman(int player, int card, event_t event)
{
  /* Malachite Talisman	|2	0x200d88d
   * Artifact
   * Whenever a player casts a |Sgreen spell, you may pay |3. If you do, untap target permanent. */
  return talisman(player, card, event, COLOR_GREEN);
}

/* Sabretooth Tiger	|2|R	=>vanilla
 * Creature - Cat 2/1
 * First strike */

int card_marton_stromgald(int player, int card, event_t event)
{
  /* Marton Stromgald	|2|R|R	0x2002E1F
   * Legendary Creature - Human Knight 1/1
   * Whenever ~ attacks, other attacking creatures get +1/+1 until end of turn for each attacking creature other than ~.
   * Whenever ~ blocks, other blocking creatures get +1/+1 until end of turn for each blocking creature other than ~. */

  int i;
  int count;
  int index_or_result;
  int attacking_or_creature;
  card_instance_t *instance;
  int blocking_or_creature;
  int other_attackers;
  check_legend_rule(player, card, event);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if ((event == EVENT_DAT_ABILITY) &&
      (index_or_result = active_cards_count[player] + -1, -1 < index_or_result))
  {
    other_attackers = 0;
    for (; index_or_result >= 0; --index_or_result)
      if ((instance = in_play(player, index_or_result), instance != NULL) &&
          is_what(player, index_or_result, 2))
      {
        attacking_or_creature = is_attacking(player, index_or_result);
        if (card != index_or_result && attacking_or_creature != 0)
          ++other_attackers;
      }
    i = active_cards_count[player] + -1;
    if (-1 < i)
    {
      do
      {
        instance = in_play(player, i);
        if (((instance != NULL) &&
             (attacking_or_creature = is_what(player, i, 2), attacking_or_creature != 0)) &&
            ((attacking_or_creature = is_attacking(player, i),
              card != i && (attacking_or_creature != 0))))
        {
          pump_until_eot(player, card, player, i, other_attackers, other_attackers);
        }
        i = i + -1;
      } while (i != -1);
    }
  }
  index_or_result = is_humiliated(player, card, AL_TRIG);
  if (((index_or_result == 0) && (current_turn != player)) &&
      (index_or_result = blocking(player, card, event), index_or_result != 0))
  {
    count = count_blockers(player, event);
    i = 0;
    if (0 < active_cards_count[player])
    {
      do
      {
        instance = in_play(player, i);
        if ((((instance != NULL) && (blocking_or_creature = is_what(player, i, 2), card != i)) &&
             (blocking_or_creature != 0)) &&
            (blocking_or_creature = blocking(player, i, event), blocking_or_creature != 0))
        {
          pump_until_eot(player, card, player, i, count + -1, count + -1);
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
    }
  }
  return 0;
}

/* Scaled Wurm	|7|G	=>vanilla
 * Creature - Wurm 7/6 */

/* Sea Spirit	|4|U	=>unlimited.c:card_wall_of_water
 * Creature - Elemental Spirit 2/3
 * |U: ~ gets +1/+0 until end of turn. */

/* Seraph	|6|W	=>card_krovikan_vampire
 * Creature - Angel 4/4
 * Flying
 * Whenever a creature dealt damage by ~ this turn dies, put that card onto the battlefield under your control at the beginning of the next end step. Sacrifice the creature when you lose control of ~. */

int card_merieke_ri_berit(int player, int card, event_t event){
	/* Merieke Ri Berit	|W|U|B	0x2002E29
	 * Legendary Creature - Human 1/1
	 * ~ doesn't untap during your untap step.
	 * |T: Gain control of target creature for as long as you control ~. When ~ leaves the battlefield or becomes untapped, destroy that creature. It can't be regenerated. */

	check_legend_rule(player, card, event);

	does_not_untap(player, card, event);

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	if (IS_AI(player)){
		td.allowed_controller = 1 - player;
	}

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		card_instance_t* instance = get_card_instance(player, card);
		gain_control_until_source_is_in_play_and_tapped(instance->parent_controller, instance->parent_card,
														instance->targets[0].player, instance->targets[0].card,
														GCUS_CONTROLLED | GCUS_BURY_IF_TAPPED_OR_LEAVE_PLAY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

/* Shatter	|1|R	=>unlimited.c:card_shatter
 * Instant
 * Destroy target artifact. */

/* Shield Bearer	|1|W	=>vanilla
 * Creature - Human Soldier 0/3
 * Banding */

int card_mesmeric_trance(int player, int card, event_t event)
{
  /* Mesmeric Trance	|1|U|U	0x200d8ba
   * Enchantment
   * Cumulative upkeep |1
   * |U, Discard a card: Draw a card. */

  cumulative_upkeep(player, card, event, MANACOST_X(1));

  if (event == EVENT_RESOLVE_ACTIVATION)
	draw_a_card(player);

  if (event == EVENT_SHOULD_AI_PLAY || event == EVENT_CAN_CAST)
	return global_enchantment(player, card, event);

  return generic_activated_ability(player, card, event, GAA_DISCARD, MANACOST_U(1), 0, NULL, NULL);
}

int card_meteor_shower(int player, int card, event_t event){
	/* Meteor Shower	|X|X|R	0x2009cb5
	 * Sorcery
	 * ~ deals X plus 1 damage divided as you choose among any number of targets. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		int amount = charge_mana_for_double_x(player, COLOR_COLORLESS);
		if( spell_fizzled != 1 && amount > -1 ){
			amount/=2;
			amount++;
			instance->info_slot = amount;
			int trgs = 0;
			while( trgs < amount ){
					char buffer[500];
					scnprintf(buffer, 500, "Select target creature, player, or planeswalker(%d of %d damage)", trgs+1, amount);
					if( new_pick_target(&td, buffer, trgs, GS_LITERAL_PROMPT) ){
						trgs++;
					}
					else{
						break;
					}
			}
			if( trgs != amount ){
				spell_fizzled = 1;
			}
		}
	}

	if(event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

/* Silver Erne	|3|U	=>vanilla
 * Creature - Bird 2/2
 * Flying, trample */

int card_mind_ravel(int player, int card, event_t event)
{
  /* Mind Ravel	|2|B	0x200d7ed
   * Sorcery
   * Target player discards a card.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  discard(get_card_instance(player, card)->targets[0].player, 0, player);
		  cantrip(player, card, 1);
		}

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_mind_warp(int player, int card, event_t event)
{
  /* Mind Warp	|X|3|B	0x200d865
   * Sorcery
   * Look at target player's hand and choose X cards from it. That player discards those cards. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_CAN_CAST)
	return generic_spell(player, card, event, GS_CAN_TARGET|GS_X_SPELL, &td, "TARGET_PLAYER", 1, NULL);

  if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
	  if (is_token(player, card))
		x_value = 0;
	  else
		{
		  charge_mana(player, COLOR_COLORLESS, -1);
		  if (cancel == 1)
			return 0;
		}
	  generic_spell(player, card, event, GS_CAN_TARGET|GS_X_SPELL, &td, "TARGET_PLAYER", 1, NULL);
	}

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  ec_definition_t coerce;
		  default_ec_definition(inst->targets[0].player, player, &coerce);
		  coerce.qty = inst->info_slot;
		  new_effect_coercion(&coerce, NULL);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return 0;
}

/* Sleight of Mind	|U	=>magic.exe:card_sleight_of_mind
 * Instant
 * Change the text of target spell or permanent by replacing all instances of one color word with another. */

/* ManalinkEh 2022: 0x2401dc0; original cards/ice_age.c:6611. */
int card_mind_whip(int player, int card, event_t event)
{
  /* Mind Whip	|2|B|B	0x000000
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of the upkeep of enchanted creature's controller, that player may pay |3. If he or she doesn't, ~ deals 2 damage to that player and you tap that creature. */

  card_instance_t *instance = in_play(player, card);
  if (instance &&
      (instance = get_card_instance(player, card), instance->damage_target_player != -1))
  {
    instance = get_card_instance(player, card);
    upkeep_trigger_ability(player, card, event, instance->damage_target_player);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY && instance->damage_target_player == current_turn)
    {
      bool paid = false;
      if (has_mana_multi(instance->damage_target_player, 3, 0, 0, 0, 0, 0))
      {
        int priority = is_tapped(instance->damage_target_player, instance->damage_target_card);
        priority = priority || life[instance->damage_target_player] < 10 ? 0 : 1;
        int choice = do_dialog(instance->damage_target_player, player, card, -1, -1,
                               " Pay 3 or get 2 damage\n Pass", priority);
        if (choice == 0)
        {
          charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER,
                                      instance->damage_target_player, COLOR_ANY, 3);
          paid = spell_fizzled != 1;
        }
      }
      if (!paid)
      {
        damage_player(instance->damage_target_player, 2, player, card);
        tap_card(instance->damage_target_player, instance->damage_target_card);
      }
    }
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  return vanilla_aura(player, card, event, 1 - player);
}

int card_minion_of_leshrac(int player, int card, event_t event){
	/* Minion of Leshrac	|4|B|B|B	0x2002E2E
	 * Creature - Demon Minion 5/5
	 * Protection from |Sblack
	 * At the beginning of your upkeep, ~ deals 5 damage to you unless you sacrifice a creature other than ~. If ~ deals damage to you this way, tap it.
	 * |T: Destroy target creature or land. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		state_untargettable(player, card, 1);
		if( ! player_sacrifices_a_permanent(player, card, player, TYPE_CREATURE, 0) ){
			damage_player(player, 5, player, card);
			tap_card(player, card);
		}
		state_untargettable(player, card, 0);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE | TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			kill_card( instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target creture or land.");
}

int card_minion_of_tevesh_szat(int player, int card, event_t event){
	/* Minion of Tevesh Szat	|4|B|B|B	0x2002E33
	 * Creature - Demon Minion 4/4
	 * At the beginning of your upkeep, ~ deals 2 damage to you unless you pay |B|B.
	 * |T: Target creature gets +3/-2 until end of turn. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int dmg = 2;
		if( has_mana(player, COLOR_BLACK, 2) ){
			int choice = do_dialog(player, player, card, -1, -1, " Pay upkeep\n Pass", 1);
			if( choice == 0 ){
				charge_mana(player, COLOR_BLACK, 2);
				if( spell_fizzled != 1 ){
					dmg-=2;
				}
			}
		}

		if( dmg > 0 ){
			damage_player(player, 2, player, card);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
							3, -2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

/* ManalinkEh 2022: 0x2402410; original cards/ice_age.c:6736. */
int card_mistfolk(int player, int card, event_t event)
{
  /* Mistfolk	|U|U	0x000000
   * Creature - Illusion 1/2
   * |U: Counter target spell that targets ~. */

  int result;
  card_instance_t *instance;
  unsigned int target_index;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x400, 0, 0, 1, 0, 0, 0, 0,
                                       NULL, NULL);
    if (result != 0)
    {
      instance = get_card_instance(card_on_stack_controller, card_on_stack);
      if (instance->number_of_targets != 0)
      {
        target_index = 0;
        do
        {
          if ((instance->targets[target_index].player == player) &&
              (instance->targets[target_index].card == card))
          {
            return 99;
          }
          target_index = target_index + 1;
        } while (target_index != instance->number_of_targets);
      }
    }
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_CLEANUP)
    return generic_activated_ability(player, card, event, 0x400, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
    return 0;
  instance = get_card_instance(player, card);
  result = counterspell_validate(player, card, NULL, 0);
  if (result != 0)
  {
    real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
  }
  return generic_activated_ability(player, card, event, 0x400, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
}

/* Snow-Covered Forest	""	0x000000:0x479ED0:magic.exe:card_forest
 * Basic Snow Land - |H2Forest */

/* Snow-Covered Island	""	0x000000:0x479EB0:magic.exe:card_island
 * Basic Snow Land - |H2Island */

/* Snow-Covered Mountain	""	0x000000:0x479EF0:magic.exe:card_mountain
 * Basic Snow Land - |H2Mountain */

/* Snow-Covered Plains	""	0x000000:0x479F10:magic.exe:card_plains
 * Basic Snow Land - |H2Plains */

/* Snow-Covered Swamp	""	0x000000:0x479E90:magic.exe:card_swamp
 * Basic Snow Land - |H2Swamp */

/* Snowfall	|2|U	0x000000
 * Enchantment
 * Cumulative upkeep |U
 * Whenever |Han Island is tapped for mana, its controller may add |U to his or her mana pool. If that |H2Island is snow, its controller may add |U|U to his or her mana pool instead. Spend this mana only to pay cumulative upkeep costs. */

/* Soldevi Machinist	|1|U	0x000000
 * Creature - Human Wizard Artificer 1/1
 * |T: Add |2 to your mana pool. Spend this mana only to activate abilities of artifacts. */

int card_mole_worms(int player, int card, event_t event)
{
  /* Mole Worms	|2|B	0x200d81f
   * Creature - Worm 1/1
   * You may choose not to untap ~ during your untap step.
   * |T: Tap target land. It doesn't untap during its controller's untap step for as long as ~ remains tapped. */

  choose_to_untap(player, card, event);

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  card_instance_t* parent = in_play(inst->parent_controller, inst->parent_card);
	  if (parent)
		{
		  parent->targets[1] = inst->targets[0];	// struct copy
		  does_not_untap_until_im_tapped(inst->parent_controller, inst->parent_card,
										 inst->targets[0].player, inst->targets[0].card);
		}
	  tap_card(inst->targets[0].player, inst->targets[0].card);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_LAND");
}

int card_monsoon(int player, int card, event_t event)
{
  /* Monsoon	|2|R|G	0x200d829
   * Enchantment
   * At the beginning of each player's end step, tap all untapped |H1Islands that player controls and ~ deals X damage to the player, where X is the number of |H1Islands tapped this way. */

  if (eot_trigger(player, card, event))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_LAND, "");
	  test.subtype = get_hacked_subtype(player, card, SUBTYPE_ISLAND);
	  int dmg = new_manipulate_all(player, card, current_turn, &test, ACT_TAP);
	  if (dmg > 0)
		damage_player(current_turn, dmg, player, card);
	}

  return global_enchantment(player, card, event);
}

static int fx_mountain_titan(int player, int card, event_t event)
{
  char attached_player;
  int target_card;
  card_instance_t *instance;
  test_definition_t test;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    attached_player = instance->damage_target_player;
    target_card = instance->damage_target_card;
    if ((((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (reason_for_trigger_controller == player))
    {
      default_test_definition(&test, 0xff);
      test.color = get_sleighted_color_test((int)attached_player, target_card, 2);
      sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
    }
    if (event == EVENT_SPA)
    {
      add_counters(player, card, (int)attached_player, target_card, COUNTER_P1_P1, 1);
      return 0;
    }
  }
  if ((event != EVENT_CLEANUP) && (event != EVENT_SHOULD_AI_PLAY))
  {
    return 0;
  }
  kill_card(player, card, KILL_EXILE);
  return 0;
}

int card_mountain_titan(int player, int card, event_t event)
{
  /* Mountain Titan	|2|B|R	0x200d8a1
   * Creature - Giant 2/2
   * |1|R|R: Until end of turn, whenever you cast a |Sblack spell, put a +1/+1 counter on ~. */

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		create_targetted_legacy_effect(player, card, &fx_mountain_titan, inst->parent_controller, inst->parent_card);
	}

  return generic_activated_ability(player, card, event, 0, MANACOST_XR(1,2), 0, NULL, NULL);
}

int card_mudslide(int player, int card, event_t event){
	/* Mudslide	|2|R	0x2009cba
	 * Enchantment
	 * Creatures without flying don't untap during their controllers' untap steps.
	 * At the beginning of each player's upkeep, that player may choose any number of tapped creatures without flying he or she controls and pay |2 for each creature chosen this way. If the player does, untap those creatures. */

	if (event == EVENT_UNTAP && current_phase == PHASE_UNTAP && !is_humiliated(player, card, AL_STATIC)
		&& is_what(affected_card_controller, affected_card, TYPE_CREATURE) && !check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING)){
		card_instance_t* instance = get_card_instance(affected_card_controller, affected_card);
		instance->untap_status &= ~3;
	}

	int upkeeps, p = current_turn, c1;
	for (upkeeps = upkeep_trigger(player, card, event); upkeeps > 0; --upkeeps){
		for (c1 = active_cards_count[p] - 1; c1 >= 0; --c1){
			if (in_play(p, c1) && is_tapped(p, c1) && is_what(p, c1, TYPE_CREATURE) && !check_for_ability(p, c1, KEYWORD_FLYING)){
				if (!has_mana(p, COLOR_COLORLESS, 2)){
					return 0;
				}
				if (do_dialog(p, p, c1, -1, -1, " Untap this creature\n Decline", 0) == 0
					&& charge_mana_while_resolving(player, card, event, p, COLOR_COLORLESS, 2)){
					untap_card(p, c1);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

static int effect_musician(int player, int card, event_t event)
{
  if (trigger_condition == TRIGGER_UPKEEP || event == EVENT_UPKEEP_TRIGGER_ABILITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->damage_target_player < 0 || instance->damage_target_card < 0
		  || !in_play(instance->damage_target_player, instance->damage_target_card))
		kill_card(player, card, KILL_EXILE);
	  else
		basic_upkeep_arbitrary(player, card, instance->damage_target_player, instance->damage_target_card, event,
							   MANACOST_X(count_counters(instance->damage_target_player, instance->damage_target_card, COUNTER_MUSIC)));
	}

  return 0;
}

int card_musician(int player, int card, event_t event)
{
  /* Musician	|2|U	0x2009cbf
   * Creature - Human Wizard 1/3
   * Cumulative upkeep |1
   * |T: Put a music counter on target creature. If it doesn't have "At the beginning of your upkeep, destroy this creature unless you pay |1 for each music counter on it," it gains that ability. */

  card_instance_t *instance;
  int result;
  card_instance_t *legacy;
  int card_00;
  target_definition_t td;

  cumulative_upkeep(player, card, event, 1, 0, 0, 0, 0, 0);
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_MUSIC, 1);
      result = 0;
      do
      {
        card_00 = 0;
        if (0 < active_cards_count[result])
        {
          do
          {
            legacy = in_play(result, card_00);
            if ((((legacy != NULL) && (legacy->internal_card_id == LEGACY_EFFECT_CUSTOM)) &&
                 (legacy->info_slot_fn == effect_musician)) &&
                ((legacy->damage_target_card == instance->targets[0].card &&
                  ((int)legacy->damage_target_player == instance->targets[0].player))))
            {
              return 0;
            }
            card_00 = card_00 + 1;
          } while (card_00 < active_cards_count[result]);
        }
        result = result + 1;
      } while (result != 2);
      create_targetted_legacy_effect(instance->parent_controller, instance->parent_card,
                                     effect_musician, instance->targets[0].player,
                                     instance->targets[0].card);
    }
  }
  return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                   "TARGET_CREATURE");
}

/* Stench of Evil	|2|B|B	0x000000
 * Sorcery
 * Destroy all |H1Plains. For each land destroyed this way, ~ deals 1 damage to that land's controller unless he or she pays |2. */

/* Stone Rain	|2|R	=>unlimited.c:card_ice_storm
 * Sorcery
 * Destroy target land. */

/* ManalinkEh 2022: 0x2402dd0; original cards/ice_age.c:6955. */
int card_mystic_might(int player, int card, event_t event)
{
  /* Mystic Might	|U	0x000000
   * Enchantment - Aura
   * Enchant land you control
   * Cumulative upkeep |1|U
   * Enchanted land has "|T: Target creature gets +2/+2 until end of turn." */

  card_instance_t *instance;
  target_definition_t td;
  cumulative_upkeep(player, card, event, 1, 0, 1, 0, 0, 0);
  if (!is_humiliated(player, card, AL_STATIC) &&
      (instance = get_card_instance(player, card), instance->damage_target_player >= 0))
  {
    instance = get_card_instance(player, card);
    int attached_player = instance->damage_target_player;
    int attached_card = instance->damage_target_card;
    default_target_definition(attached_player, attached_card, &td, 2);
    td.preferred_controller = player;
    if (event == EVENT_CAN_ACTIVATE)
      return granted_generic_activated_ability(player, card, attached_player, attached_card, event,
                                               5, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_CREATURE");
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = 0;
      if (pick_target(&td, "TARGET_CREATURE"))
        return granted_generic_activated_ability(player, card, attached_player, attached_card,
                                                 event, 1, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    }
    else if (event == EVENT_RESOLVE_ACTIVATION &&
             validate_target(attached_player, attached_card, &td, 0))
    {
      card_instance_t *land = get_card_instance(attached_player, attached_card);
      pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                             land->targets[0].player, land->targets[0].card, 2, 2, 0, 0);
    }
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
  td.preferred_controller = player;
  td.allowed_controller = player;
  return targeted_aura_custom_prompt(player, card, event, &td, "Select target land you control.");
}

int card_mystic_remora(int player, int card, event_t event){
	/* Mystic Remora	|U	0x20017EA
	 * Enchantment
	 * Cumulative upkeep |1
	 * Whenever an opponent casts a noncreature spell, you may draw a card unless that player pays |4. */

	if (!is_unlocked(player, card, event, 3)){ return 0; }
	cumulative_upkeep(player, card, event, MANACOST_X(1));
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.type_flag = F1_NO_CREATURE;
		sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		int choice = 0;
		int ai_choice = hand_count[1-player] < 4;
		if (has_mana(1-player, COLOR_COLORLESS, 4) && trigger_cause_controller == HUMAN){
			choice = do_dialog(1-player, player, card, -1, -1, " Let opponent draw\n Pay 4", ai_choice);
		}
		if (choice == 1){
			charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, 1-player, COLOR_COLORLESS, 4);
			if (spell_fizzled != 1){ return 0; }
		}
		draw_a_card(player);
	}
	return global_enchantment(player, card, event);
}

int card_nacre_talisman(int player, int card, event_t event)
{
  /* Nacre Talisman	|2	0x200d892
   * Artifact
   * Whenever a player casts a |Swhite spell, you may pay |3. If you do, untap target permanent. */
  return talisman(player, card, event, COLOR_WHITE);
}

/* ManalinkEh 2022: 0x23ed750; original cards/ice_age.c:7051. */
static int basic_subtype_to_color_part(int subt)
{
  if (subt != 0x5006)
  {
    return (subt == 0x5007) - 1 | 5;
  }
  return 4;
}

/* ManalinkEh 2022: 0x2403360; original cards/ice_age.c:7060. */
int card_naked_singularity(int player, int card, event_t event)
{
  /* Naked Singularity	|5	0x000000
   * Artifact
   * Cumulative upkeep |3
   * If tapped for mana, |H1Plains produce |R, |H1Islands produce |G, |H1Swamps produce |W, |H1Mountains produce |U, and |H1Forests produce |B instead of any other type. */

  int c;
  int subtype_or_color;
  card_instance_t *instance;
  int land_subtype;
  subtype_t hacked_land;
  int p;
  cumulative_upkeep(player, card, event, 3, 0, 0, 0, 0, 0);
  if (event != EVENT_CAST_SPELL)
  {
    if ((event == EVENT_MODIFY_MANA_PROD) || (event == EVENT_RESOLVE_SPELL))
    {
      p = 0;
      do
      {
        subtype_or_color = get_hacked_subtype(player, card, 0x5009);
        if (subtype_or_color == 0x5009)
        {
          subtype_or_color = 1;
        }
        else if (subtype_or_color == 0x5002)
        {
          subtype_or_color = 2;
        }
        else if (subtype_or_color == 0x5001)
        {
          subtype_or_color = 3;
        }
        else
        {
          subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
        }
        mana_color_override[p][subtype_or_color] = 5;
        subtype_or_color = get_hacked_subtype(player, card, 0x5002);
        if (subtype_or_color == 0x5009)
        {
          subtype_or_color = 1;
        }
        else if (subtype_or_color == 0x5002)
        {
          subtype_or_color = 2;
        }
        else if (subtype_or_color == 0x5001)
        {
          subtype_or_color = 3;
        }
        else
        {
          subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
        }
        mana_color_override[p][subtype_or_color] = 3;
        subtype_or_color = get_hacked_subtype(player, card, 0x5001);
        if (subtype_or_color == 0x5009)
        {
          subtype_or_color = 1;
        }
        else if (subtype_or_color == 0x5002)
        {
          subtype_or_color = 2;
        }
        else if (subtype_or_color == 0x5001)
        {
          subtype_or_color = 3;
        }
        else
        {
          subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
        }
        mana_color_override[p][subtype_or_color] = 1;
        subtype_or_color = get_hacked_subtype(player, card, 0x5006);
        if (subtype_or_color == 0x5009)
        {
          subtype_or_color = 1;
        }
        else if (subtype_or_color == 0x5002)
        {
          subtype_or_color = 2;
        }
        else if (subtype_or_color == 0x5001)
        {
          subtype_or_color = 3;
        }
        else
        {
          subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
        }
        mana_color_override[p][subtype_or_color] = 2;
        subtype_or_color = get_hacked_subtype(player, card, 0x5007);
        if (subtype_or_color == 0x5009)
        {
          subtype_or_color = 1;
        }
        else if (subtype_or_color == 0x5002)
        {
          subtype_or_color = 2;
        }
        else if (subtype_or_color == 0x5001)
        {
          subtype_or_color = 3;
        }
        else
        {
          subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
        }
        mana_color_override[p][subtype_or_color] = 4;
        if (0 < active_cards_count[p])
        {
          c = 0;
          do
          {
            instance = in_play(p, c);
            if ((instance != NULL) &&
                ((((land_subtype = has_subtype(p, c, SUBTYPE_FOREST),
                    land_subtype != 0 ||
                        (land_subtype = has_subtype(p, c, SUBTYPE_ISLAND), land_subtype != 0)) ||
                   (land_subtype = has_subtype(p, c, SUBTYPE_MOUNTAIN), land_subtype != 0)) ||
                  ((land_subtype = has_subtype(p, c, SUBTYPE_PLAINS),
                    land_subtype != 0 ||
                        (land_subtype = has_subtype(p, c, SUBTYPE_SWAMP), land_subtype != 0))))))
            {
              set_special_flags2(p, c, 0x40);
            }
            c = c + 1;
          } while (c < active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
    }
    return 0;
  }
  hacked_land = get_hacked_subtype(player, card, 0x5009);
  if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
  {
    hacked_land = get_hacked_subtype(player, card, 0x5002);
    if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
    {
      hacked_land = get_hacked_subtype(player, card, 0x5001);
      if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
      {
        hacked_land = get_hacked_subtype(player, card, 0x5006);
        if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
        {
          hacked_land = get_hacked_subtype(player, card, 0x5007);
          if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
          {
            return 0;
          }
        }
      }
    }
  }
  set_special_flags2(affected_card_controller, affected_card, 0x40);
  return 0;
}

/* Sulfurous Springs	""	=>functions.c:generic_painland
 * Land
 * |T: Add |1 to your mana pool.
 * |T: Add |B or |R to your mana pool. ~ deals 1 damage to you. */

/* Swamp	""	=>magic.exe:card_swamp
 * Basic Land - |H2Swamp */

/* Swords to Plowshares	|W	=>unlimited.c:card_swords_to_plowshares
 * Instant
 * Exile target creature. Its controller gains life equal to its power. */

int card_natures_lore(int player, int card, event_t event){
	/* Nature's Lore	|1|G	0x2009698
	 * Sorcery
	 * Search your library for |Ha Forest card and put that card onto the battlefield. Then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card.", SUBTYPE_FOREST));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

static int effect_necropotence(int player, int card, event_t event)
{
  if (event == EVENT_MAX_HAND_SIZE && current_turn == player)
	{
	  card_instance_t *instance = get_card_instance(player, card);
	  add_card_to_hand(player, instance->targets[0].card);
	  kill_card(player, card, KILL_EXILE);
	}
  return 0;
}

int card_necropotence(int player, int card, event_t event){
	/* Necropotence	|B|B|B	0x20011C8
	 * Enchantment
	 * Skip your draw step.
	 * Whenever you discard a card, exile that card from your graveyard.
	 * Pay 1 life: Exile the top card of your library face down. Put that card into your hand at the beginning of your next end step. */

	skip_your_draw_step(player, event);

	if (discard_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, DISCARD_STILL_IN_HAND)){
		kill_card(trigger_cause_controller, trigger_cause, KILL_EXILE);
		g_prevent_normal_card_to_graveyard_effect = 1;	// prevent normal card-to-graveyard effect
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if(event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t *instance = get_card_instance(player, card);
		int *deck = deck_ptr[player];
		if( deck[0] != -1 ){
			int legacy_card = create_legacy_effect(instance->parent_controller, instance->parent_card, &effect_necropotence );
			card_instance_t *legacy = get_card_instance(player, legacy_card);
			legacy->targets[0].card = deck[0];
			remove_card_from_deck( player, 0 );
		}
	}

	return generic_activated_ability(player, card, event, 0, MANACOST0, 1, NULL, NULL);
}

/* Timberline Ridge	""	=>card_land_cap
 * Land
 * ~ doesn't untap during your untap step if it has a depletion counter on it.
 * At the beginning of your upkeep, remove a depletion counter from ~.
 * |T: Add |R or |G to your mana pool. Put a depletion counter on ~. */

static const char* target_only_permanents_unless_in_hand(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  // If e.g. the player targeted himself with a lose-life spell like Soul Feast, it'll still be on the battlefield.  Don't let him sac it!
  return in_hand(player, card) || is_what(player, card, TYPE_PERMANENT) ? NULL : gs_target_error_type;	// ",type"
}

void oath_of_lim_dul_lifeloss_trigger(int player, int card, int amount)
{
  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_HAND | TARGET_ZONE_IN_PLAY;
  td.allowed_controller = player;
  td.preferred_controller = player;
  td.illegal_abilities = 0;
  td.allow_cancel = 0;
  td.special = TARGET_SPECIAL_NOT_ME | TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = target_predicate_extra(target_only_permanents_unless_in_hand);

  target_t tgts[amount + 1];
  int i, num_tgts = 0;
  card_instance_t *instance = get_card_instance(player, card);

  for (i = 0; can_target(&td) && i < amount; ++i)
    if (select_target(player, card, &td, "Select a permanent to sacrifice or card to discard.",
                           &tgts[i]))
    {
      instance->number_of_targets = 0;
      state_untargettable(tgts[i].player, tgts[i].card, 1);
      ++num_tgts;
    }

  for (i = 0; i < num_tgts; ++i)
    state_untargettable(tgts[i].player, tgts[i].card, 0);

  for (i = 0; i < num_tgts; ++i)
    if (in_hand(tgts[i].player, tgts[i].card))
      discard_card(tgts[i].player, tgts[i].card);
    else
      kill_card(tgts[i].player, tgts[i].card, KILL_SACRIFICE);
}

/* Tor Giant	|3|R	=>vanilla
 * Creature - Giant 3/3 */

int card_oath_of_lim_dul(int player, int card, event_t event){
	/* Oath of Lim-Dul	|3|B	0x2002E38
	 * Enchantment
	 * Whenever you lose life, for each 1 life you lost, sacrifice a permanent other than ~ unless you discard a card.
	 * |B|B: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
		draw_cards(get_card_instance(player, card)->parent_controller, 1);
	}

	return generic_activated_ability(player, card, event, 0, MANACOST_B(2), 0, NULL, NULL);
}

int card_onyx_talisman(int player, int card, event_t event)
{
  /* Onyx Talisman	|2	0x200d897
   * Artifact
   * Whenever a player casts a |Sblack spell, you may pay |3. If you do, untap target permanent. */
  return talisman(player, card, event, COLOR_BLACK);
}

/* Underground River	""	=>functions.c:generic_painland
 * Land
 * |T: Add |1 to your mana pool.
 * |T: Add |U or |B to your mana pool. ~ deals 1 damage to you. */

/* ManalinkEh 2022: 0x23ee1d0; original cards/ice_age.c:7317. */
static int orcish_farmer_effect(int player, int card, event_t event)
{
  int basic_land_type_to_become;
  card_instance_t *instance;
  if (event == EVENT_CHANGE_TYPE)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      basic_land_type_to_become = get_hacked_subtype(player, card, 0x5009);
      event_result = land_into_new_basic_land_type(event_result, basic_land_type_to_become);
    }
  }
  else if (event == EVENT_BEGIN_TURN)
  {
    instance = get_card_instance(player, card);
    if (instance->damage_target_player == current_turn)
    {
      instance = get_card_instance(player, card);
      if (-1 < instance->targets[0].player)
      {
        kill_card(player, card, KILL_EXILE);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2404320; original cards/ice_age.c:7336. */
int card_orcish_farmer(int player, int card, event_t event)
{
  /* Orcish Farmer	|1|R|R	0x000000
   * Creature - Orc 2/2
   * |T: Target land becomes |Ha Swamp until its controller's next untap step. */

  int32_t source_card;
  card_instance_t *instance;
  int valid;
  card_instance_t *legacy;
  target_definition_t td;

  if (((event != EVENT_ACTIVATE) && (1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION))) &&
      (event != EVENT_CLEANUP))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  if ((ai_is_speculating == 1 && player == 1) ||
      (ai_is_speculating != 1 && player == 1 && ((unsigned char)trace_mode & 2) == 0))
  {
    td.required_subtype = get_hacked_subtype(1, card, 0x5009);
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
  }
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (valid = valid_target(&td), valid != 0))
  {
    legacy = get_card_instance(
        player, create_targetted_legacy_effect(instance->parent_controller, instance->parent_card,
                                               orcish_farmer_effect, instance->targets[0].player,
                                               instance->targets[0].card));
    source_card = instance->parent_card;
    legacy->targets[0].player = instance->parent_controller;
    legacy->targets[0].card = source_card;
    legacy->number_of_targets = '\x01';
  }
  return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td, "TARGET_LAND");
}

/* Veldt	""	=>card_land_cap
 * Land
 * ~ doesn't untap during your untap step if it has a depletion counter on it.
 * At the beginning of your upkeep, remove a depletion counter from ~.
 * |T: Add |G or |W to your mana pool. Put a depletion counter on ~. */

/* ManalinkEh 2022: 0x2405060; original cards/ice_age.c:7493. */
int card_orcish_librarian(int player, int card, event_t event)
{
  /* Orcish Librarian	|1|R	0x000000
   * Creature - Orc 1/1
   * |R, |T: Look at the top eight cards of your library. Exile four of them at random, then put the rest on top of your library in any order. */

  char buffer[100];
  if (event == EVENT_RESOLVE_ACTIVATION && count_deck(player) != 0)
  {
    int deck_size = count_deck(player);
    int amount = deck_size > 8 ? 8 : deck_size;
    if (deck_size > 7)
    {
      show_deck(player, deck_ptr[player], amount, "Here\'s the top eight cards of your deck.", 0,
                gs_done_008b40e0);
      int rnd_ex = amount > 4 ? 4 : amount;
      int next_position = amount - (rnd_ex + 1);
      int count_g = 0;
      int cards = amount;
      do
      {
        int rnd = next_position;
        if (count_g < next_position)
          rnd = internal_rand(cards);
        ++count_g;
        rfg_card_in_deck(player, rnd);
        --cards;
        --next_position;
      } while (count_g < 4 && cards > 0);
      rearrange_top_x(player, player, amount - rnd_ex);
    }
    else
    {
      int pos = scnprintf(buffer, 100, "Here\'s the top ");
      const char *fmt = NULL;
      switch (amount)
      {
        case 1: fmt = "one card"; break;
        case 2: fmt = "two cards"; break;
        case 3: fmt = "three cards"; break;
        case 4: fmt = "four cards"; break;
        case 5: fmt = "five cards"; break;
        case 6: fmt = "six cards"; break;
        case 7: fmt = "seven cards"; break;
      }
      if (fmt)
      {
        int length = scnprintf(buffer + pos, 100 - pos, fmt);
        scnprintf(buffer + pos + length, 100 - (pos + length), " of your deck.");
      }
      else
        scnprintf(buffer + pos, 100 - pos, " of your deck.");
      show_deck(player, deck_ptr[player], amount, buffer, 0, gs_done_008b40e0);
      if (deck_size > 0)
      {
        if (deck_size < 5)
          for (int count = 0; count < amount; ++count)
            rfg_card_in_deck(player, 0);
        else
        {
          int remain = amount;
          do
          {
            int next = remain - 1;
            int position = internal_rand(remain);
            rfg_card_in_deck(player, position);
            remain = next;
          } while (remain != amount - 4);
          rearrange_top_x(player, player, remain);
        }
      }
    }
  }
  return generic_activated_ability(player, card, event, 1, 0, 0, 0, 0, 1, 0, 0, NULL, NULL);
}

int card_orcish_lumberjack(int player, int card, event_t event){
	/* Orcish Lumberjack	|R	0x2002E3D
	 * Creature - Orc 1/1
	 * |T, Sacrifice |Ha Forest: Add three mana in any combination of |R and/or |G to your mana pool. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	default_test_definition(&test, TYPE_LAND);
	test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
	scnprintf(test.message, 100, "%s", get_hacked_land_text(player, card, "Select a %s to sacrifice", SUBTYPE_FOREST));

	if( event == EVENT_CAN_ACTIVATE && can_produce_mana(player, card) && ! is_tapped(player, card) && ! is_sick(player, card) ){
		return new_can_sacrifice_as_cost(player, card, &test);
	}

	if( event == EVENT_ACTIVATE){
		ai_modifier -= 36;
		if( new_sacrifice(player, card, player, SAC_AS_COST, &test) ){
			add_state(player, card, STATE_TAPPED);
			produce_mana_tapped_any_combination_of_colors(player, card, COLOR_TEST_RED|COLOR_TEST_GREEN, 3, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	return 0;
}

/* ManalinkEh 2022: 0x2405590; original cards/ice_age.c:7585. */
int card_orcish_squatters(int player, int card, event_t event)
{
  /* Orcish Squatters	|4|R	0x000000
   * Creature - Orc 2/3
   * Whenever ~ attacks and isn't blocked, you may gain control of target land defending player controls for as long as you control ~. If you do, ~ assigns no combat damage this turn. */

  int c;
  int power_or_eligible;
  card_instance_t *instance;
  unsigned int argument7;
  int opponent_or_choice;
  int unblocked_power;
  target_definition_t td;

  if (event == EVENT_DECLARE_BLOCKERS)
  {

    unblocked_power = 0;
    c = active_cards_count[current_turn] + -1;
    if (-1 < c)
    {
      do
      {
        instance = in_play(current_turn, c);
        if ((((instance != NULL) &&
              (power_or_eligible = is_what(current_turn, c, 2), power_or_eligible != 0)) &&
             (power_or_eligible = is_attacking(current_turn, c), power_or_eligible != 0)) &&
            ((power_or_eligible = is_unblocked(current_turn, c),
              power_or_eligible != 0 &&
                  (power_or_eligible = check_special_flags(current_turn, c, 0x40),
                   power_or_eligible == 0))))
        {
          power_or_eligible = get_power(current_turn, c);
          unblocked_power = unblocked_power + power_or_eligible;
        }
        c = c + -1;
      } while (c != -1);
    }
    opponent_or_choice = is_attacking(player, card);
    if (((opponent_or_choice != 0) &&
         (opponent_or_choice = is_unblocked(player, card), opponent_or_choice != 0)) &&
        (opponent_or_choice = is_humiliated(player, card, AL_TRIG), opponent_or_choice == 0))
    {

      default_target_definition(player, card, &td, 1);
      opponent_or_choice = 1 - player;
      td.allowed_controller = opponent_or_choice;
      td.preferred_controller = opponent_or_choice;
      instance = get_card_instance(player, card);
      if ((((ai_is_speculating == 1) ||
            ((argument7 = 1,
              player == 1 && (argument7 = 1, ((unsigned char)trace_mode & 2) == 0)))) &&
           (argument7 = 1, current_turn == player)) &&
          (unblocked_power < life[opponent_or_choice]))
      {
        opponent_or_choice = count_subtype(opponent_or_choice, 1, -1);
        argument7 = (unsigned int)(6 < opponent_or_choice);
      }
      opponent_or_choice = do_dialog(player, player, card, -1, -1, " Steal land\n Pass", argument7);
      if (((opponent_or_choice == 0) &&
           (opponent_or_choice = can_target(&td), opponent_or_choice != 0)) &&
          (opponent_or_choice = pick_target(&td, "TARGET_LAND"), opponent_or_choice != 0))
      {
        gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[0].player,
                                                        instance->targets[0].card,
                                                        GCUS_SOURCE_IN_PLAY | GCUS_CONTROLLED);
        negate_combat_damage_this_turn(player, card, player, card, 2);
      }
      instance->number_of_targets = '\0';
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2405820; original cards/ice_age.c:7630. */
int card_order_of_the_sacred_torch(int player, int card, event_t event)
{
  /* Order of the Sacred Torch	|1|W|W	0x000000
   * Creature - Human Knight 2/2
   * |T, Pay 1 life: Counter target |Sblack spell. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    counterspell_target_definition(player, card, &td, 0xff);
    td.required_color = get_sleighted_color_test(player, card, 2);
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    counterspell_target_definition(player, card, &td, 0xff);
    td.required_color = get_sleighted_color_test(player, card, 2);
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (result = counterspell_validate(player, card, &td, 0), result != 0))
    {
      real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
    }
  }
  return generic_activated_ability(player, card, event, 0x401, 0, 0, 0, 0, 0, 0, 1, &td, NULL);
}
int card_panic(int player, int card, event_t event)
{
  /* Panic	|R	0x200d87e
   * Instant
   * Cast ~ only during combat before blockers are declared.
   * Target creature can't block this turn.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_CAN_CAST && !(current_phase >= PHASE_DECLARE_ATTACKERS && current_phase < PHASE_DECLARE_BLOCKERS))
	return 0;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  alternate_legacy_text(1, player, pump_ability_until_eot(player, card,
																  inst->targets[0].player, inst->targets[0].card,
																  0,0, 0,SP_KEYWORD_CANNOT_BLOCK));
		  alternate_legacy_text(2, player, cantrip(player, card, 1));
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_pentagram_of_the_ages(int player, int card, event_t event){
	/* Pentagram of the Ages	|4	0x2002E42
	 * Artifact
	 * |4, |T: The next time a source of your choice would deal damage to you this turn, prevent that damage. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = damage_card;
	td.required_type = 0;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			int amount = target->info_slot;
			prevent_damage(instance->targets[0].player, instance->targets[0].card, amount);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_LITERAL_PROMPT,
									MANACOST_X(4), 0, &td, "Select a damage card that will damage you.");
}

/* Wall of Pine Needles	|3|G	=>unlimited.c:card_wall_of_brambles
 * Creature - Plant Wall 3/3
 * Defender
 * |G: Regenerate ~. */

/* Wall of Shields	|3	=>vanilla
 * Artifact Creature - Wall 0/4
 * Defender
 * Banding */

int card_pestilence_rats(int player, int card, event_t event)
{
  /* Pestilence Rats	|2|B	0x2009cc4
   * Creature - Rat 100/3
   * ~'s power is equal to the number of other Rats on the battlefield. */

  int newval;
  test_definition_t this_test;
  if (((event == EVENT_POWER) && (card == affected_card)) && (player == affected_card_controller))
  {
    default_test_definition(&this_test, 0x47);
    this_test.subtype = 0x9a;
    this_test.not_me = 1;
    newval = check_battlefield_for_special_card(player, card, 2, 4, &this_test);
    set_characteristic_pt(newval);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2405d90; original cards/ice_age.c:7742. */
int card_phantasmal_mount(int player, int card, event_t event)
{
  /* Phantasmal Mount	|1|U	0x000000
   * Creature - Illusion Horse 1/1
   * Flying
   * |T: Target creature you control with toughness 2 or less gets +1/+1 and gains flying until end of turn. When ~ leaves the battlefield this turn, sacrifice that creature. When the creature leaves the battlefield this turn, sacrifice ~. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.toughness_requirement = 0x2002;
    td.allowed_controller = player;
    td.preferred_controller = player;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.toughness_requirement = 0x2002;
    td.allowed_controller = player;
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0)) &&
        (result = pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                                         instance->targets[0].player, instance->targets[0].card, 1,
                                         1, 0x20, 0),
         result != -1))
    {
      instance = get_card_instance(player, result);
      instance->targets[3].player = 0x228;
      instance->targets[3].card = 3;
    }
  }
  result =
      generic_activated_ability(player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                "Select target creature you control with toughness 2 or less.");
  return result;
}

int card_pit_trap(int player, int card, event_t event){
	/* Pit Trap	|2	0x2002E47
	 * Artifact
	 * |2, |T, Sacrifice ~: Destroy target attacking creature without flying. It can't be regenerated. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_ATTACKING;
	td.illegal_abilities |= KEYWORD_FLYING;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET | GAA_SACRIFICE_ME | GAA_LITERAL_PROMPT,
									MANACOST_X(2), 0, &td, "Select target attacking creature without flying.");
}

static int can_sac_land_thunk(int player, int card, int number_of_age_counters)
{
	test_definition_t this_test;
	default_test_definition(&this_test, TYPE_LAND);
	this_test.qty = number_of_age_counters;

	return new_can_sacrifice(player, card, player, &this_test);
}

static int sac_land_thunk(int player, int card, int number_of_age_counters)
{
  return impose_sacrifice(player, card, player, number_of_age_counters, TYPE_LAND,MATCH, 0,0, 0,0, 0,0, -1,0);
}

/* Wild Growth	|G	=>unlimited.c:card_wild_growth
 * Enchantment - Aura
 * Enchant land
 * Whenever enchanted land is tapped for mana, its controller adds |G to his or her mana pool. */

/* Wind Spirit	|4|U	=>gatecrash.c:card_ripscale_predators
 * Creature - Elemental Spirit 3/2
 * Flying
 * Menace */

int card_polar_kraken(int player, int card, event_t event)
{
	/* Polar Kraken	|8|U|U|U	0x2002E4C
	 * Creature - Kraken 11/11
	 * Trample
	 * ~ enters the battlefield tapped.
	 * Cumulative upkeep-Sacrifice a land. */

  comes_into_play_tapped(player, card, event);

  cumulative_upkeep_general(player, card, event, can_sac_land_thunk, sac_land_thunk);

  return 0;
}

/* Winter's Chill	|X|U	0x000000
 * Instant
 * Cast ~ only during combat before blockers are declared.
 * X can't be greater than the number of snow lands you control.
 * Choose X target attacking creatures. For each of those creatures, its controller may pay |1 or |2. If that player doesn't, destroy that creature at end of combat. If that player pays only |1, prevent all combat damage that would be dealt to and dealt by that creature this combat. */

int card_portent(int player, int card, event_t event){
	/* Portent	|U	0x2009cc9
	 * Sorcery
	 * Look at the top three cards of target player's library, then put them back in any order. You may have that player shuffle his or her library.
	 * Draw a card at the beginning of the next turn's upkeep. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			rearrange_top_x(instance->targets[0].player, player, 3);
			int choice = do_dialog(player, player, card, -1, -1, " Shuffle this library\n Pass", 0);
			if( choice == 0 ){
				shuffle(instance->targets[0].player);
			}
			cantrip(player, card, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_pox(int player, int card, event_t event){
	/* Pox	|B|B|B	0x200130D
	 * Sorcery
	 * Each player loses a third of his or her life, then discards a third of the cards in his or her hand, then sacrifices a third of the creatures he or she controls, then sacrifices a third of the lands he or she controls. Round up each time. */

  if (event == EVENT_RESOLVE_SPELL)
	{
		test_definition_t test;

	  APNAP(p, { lose_life(p, (life[p] + 2) / 3); });
	  APNAP(p, { new_multidiscard(p, (hand_count[p] + 2) / 3, 0, player); });
	  APNAP(p,
			{
				int count = (count_subtype(p, TYPE_CREATURE, -1) + 2) / 3;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
				test.qty = count;
				new_sacrifice(player, card, p, SAC_NO_CANCEL | SAC_CAUSED, &test);
			});
	  APNAP(p,
			{
				int count = (count_subtype(p, TYPE_LAND, -1) + 2) / 3;
				new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
				test.qty = count;
				new_sacrifice(player, card, p, SAC_NO_CANCEL | SAC_CAUSED, &test);
			});

	  kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* ManalinkEh 2022: 0x2406500; original cards/ice_age.c:7903. */
int card_prismatic_ward(int player, int card, event_t event)
{
  /* Prismatic Ward	|1|W	0x000000	[Chromatic Armor]
   * Enchantment - Aura
   * Enchant creature
   * As ~ enters the battlefield, choose a color.
   * Prevent all damage that would be dealt to enchanted creature by sources of the chosen color. */

  card_instance_t *instance;
  char attached_player;
  card_instance_t *aura_instance;
  int result;
  card_instance_t *damage;
  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    result = choose_a_color_and_show_legacy(player, card, 1 - player, 1);
    instance->info_slot = 1 << ((unsigned char)result & 0x1f);
  }
  instance = in_play(player, card);
  if (instance)
  {
    aura_instance = get_card_instance(player, card);
    if (aura_instance->damage_target_player >= 0 &&
        (aura_instance = get_card_instance(player, card), event == EVENT_DEAL_DAMAGE))
    {
      attached_player = aura_instance->damage_target_player;
      result = aura_instance->damage_target_card;

      damage = get_card_instance(affected_card_controller, affected_card);
      if (((damage->internal_card_id == damage_card) &&
           (((damage->damage_target_card == result &&
              (attached_player == damage->damage_target_player)) &&
             (result = check_state(affected_card_controller, affected_card, 0x100000),
              result == 0)))) &&
          (aura_instance->info_slot != -1))
      {

        prevent_damage(affected_card_controller, affected_card, damage->info_slot);
      }
    }
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  return vanilla_aura(player, card, event, player);
}

int card_pyknite(int player, int card, event_t event)
{
  /* Pyknite	|2|G	0x200d784
   * Creature - Ouphe 1/1
   * When ~ enters the battlefield, draw a card at the beginning of the next turn's upkeep. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
		cantrip(player, card, 1);

	return 0;
}

int card_pyroblast(int player, int card, event_t event)
{
  /* Pyroblast	|R	0x200d86f
   * Instant
   * Choose one -
   * * Counter target spell if it's |Sblue.
   * * Destroy target permanent if it's |Sblue. */
  return ia_blast(player, card, event, COLOR_BLUE);
}

int card_pyroclasm(int player, int card, event_t event){
	/* Pyroclasm	|1|R	0x200cd98
	 * Sorcery
	 * ~ deals 2 damage to each creature. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		new_damage_all(player, card, ANYBODY, 2, NDA_ALL_CREATURES, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* Yavimaya Gnats	|2|G	=>unlimited.c:card_wall_of_brambles
 * Creature - Insect 0/1
 * Flying
 * |G: Regenerate ~. */

int card_rally(int player, int card, event_t event)
{
  /* Rally	|W|W	0x200d7b6
   * Instant
   * Blocking creatures get +1/+1 until end of turn. */

  if (event == EVENT_RESOLVE_SPELL)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "");
	  test.state = STATE_BLOCKING;
	  pump_creatures_until_eot(player, card, ANYBODY, 0, 1, 1, 0,0, &test);
	  kill_card(player, card, KILL_DESTROY);
	}

  return basic_spell(player, card, event);
}

int card_ray_of_command(int player, int card, event_t event){
/* Ray of Command	|3|U
 * Instant
 * Untap target creature an opponent controls and gain control of it until end of turn. That creature gains haste until end of turn.
 When you lose control of the creature, tap it. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = 1-player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( instance->targets[0].player != player ){
				untap_card(instance->targets[0].player, instance->targets[0].card);
				alternate_legacy_text(1, player, gain_control_until_eot_haste(player, card, instance->targets[0].player, instance->targets[0].card));
				alternate_legacy_text(2, player, create_targetted_legacy_effect(player, card, effect_tap_when_lose_control, instance->targets[0].player, instance->targets[0].card));
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature to untap & gain control.", 1, NULL);
}

/* Zuran Spellcaster	|2|U	=>unlimited.c:card_prodigal_sorcerer
 * Creature - Human Wizard 1/1
 * |T: ~ deals 1 damage to target creature or player. */

int card_ray_of_erasure(int player, int card, event_t event)
{
  /* Ray of Erasure	|U	0x200d82e
   * Instant
   * Target player puts the top card of his or her library into his or her graveyard.
   * Draw a card at the beginning of the next turn's upkeep. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  mill(get_card_instance(player, card)->targets[0].player, 1);
		  cantrip(player, card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
static void add_venomous_breath_effect(int player, int card, int t_player, int t_card);
static int kjeldoran_frostbeast_legacy(int player, int card, event_t event);
static int venomous_breath_effect(int player, int card, event_t event);
static int orcish_farmer_effect(int player, int card, event_t event);

/* ManalinkEh 2022: 0x2406c20; original cards/ice_age.c:8060. */
int card_reality_twist(int player, int card, event_t event)
{
  /* Reality Twist	|U|U|U	0x000000
   * Enchantment
   * Cumulative upkeep |1|U|U
   * If tapped for mana, |H1Plains produce |R, |H1Swamps produce |G, |H1Mountains produce |W, and |H1Forests produce |B instead of any other type. */

  int c;
  int p;
  int subtype_or_color;
  card_instance_t *instance;
  subtype_t hacked_land;
  cumulative_upkeep(player, card, event, 1, 0, 2, 0, 0, 0);
  if (event == EVENT_CAST_SPELL)
  {
    hacked_land = get_hacked_subtype(player, card, 0x5009);
    if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
    {
      hacked_land = get_hacked_subtype(player, card, 0x5001);
      if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
      {
        hacked_land = get_hacked_subtype(player, card, 0x5006);
        if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
        {
          hacked_land = get_hacked_subtype(player, card, 0x5007);
          if (has_subtype(affected_card_controller, affected_card, hacked_land) == 0)
            return global_enchantment(player, card, event);
        }
      }
    }
    set_special_flags2(affected_card_controller, affected_card, 0x40);
  }
  else if ((event == EVENT_MODIFY_MANA_PROD) || (event == EVENT_RESOLVE_SPELL))
  {
    p = 0;
    do
    {
      subtype_or_color = get_hacked_subtype(player, card, 0x5009);
      if (subtype_or_color == 0x5009)
      {
        subtype_or_color = 1;
      }
      else if (subtype_or_color == 0x5002)
      {
        subtype_or_color = 2;
      }
      else if (subtype_or_color == 0x5001)
      {
        subtype_or_color = 3;
      }
      else
      {
        subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
      }
      mana_color_override[p][subtype_or_color] = 3;
      subtype_or_color = get_hacked_subtype(player, card, 0x5001);
      if (subtype_or_color == 0x5009)
      {
        subtype_or_color = 1;
      }
      else if (subtype_or_color == 0x5002)
      {
        subtype_or_color = 2;
      }
      else if (subtype_or_color == 0x5001)
      {
        subtype_or_color = 3;
      }
      else
      {
        subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
      }
      mana_color_override[p][subtype_or_color] = 1;
      subtype_or_color = get_hacked_subtype(player, card, 0x5006);
      if (subtype_or_color == 0x5009)
      {
        subtype_or_color = 1;
      }
      else if (subtype_or_color == 0x5002)
      {
        subtype_or_color = 2;
      }
      else if (subtype_or_color == 0x5001)
      {
        subtype_or_color = 3;
      }
      else
      {
        subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
      }
      mana_color_override[p][subtype_or_color] = 5;
      subtype_or_color = get_hacked_subtype(player, card, 0x5007);
      if (subtype_or_color == 0x5009)
      {
        subtype_or_color = 1;
      }
      else if (subtype_or_color == 0x5002)
      {
        subtype_or_color = 2;
      }
      else if (subtype_or_color == 0x5001)
      {
        subtype_or_color = 3;
      }
      else
      {
        subtype_or_color = basic_subtype_to_color_part(subtype_or_color);
      }
      mana_color_override[p][subtype_or_color] = 4;
      if (0 < active_cards_count[p])
      {
        c = 0;
        do
        {
          instance = in_play(p, c);
          if (instance != NULL)
          {
            subtype_t forest = get_hacked_subtype(player, card, 0x5001);
            if (has_subtype(p, c, forest) ||
                has_subtype(p, c, get_hacked_subtype(player, card, 0x5006)) ||
                has_subtype(p, c, get_hacked_subtype(player, card, 0x5007)) ||
                has_subtype(p, c, get_hacked_subtype(player, card, 0x5009)))
              set_special_flags2(p, c, 0x40);
          }
          c = c + 1;
        } while (c < active_cards_count[p]);
      }
      p = p + 1;
    } while (p != 2);
  }
  return global_enchantment(player, card, event);
}

int card_red_scarab(int player, int card, event_t event)
{
  /* Red Scarab	|W	0x200b5ce
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature can't be blocked by |Sred creatures.
	 * Enchanted creature gets +2/+2 as long as an opponent controls a |Sred permanent. */

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS || event == EVENT_BLOCK_LEGALITY)
    return scarab(player, card, event, COLOR_TEST_RED);
  return 0;
}

/* ManalinkEh 2022: 0x2407170; original cards/ice_age.c:8124. */
int card_rime_dryad(int player, int card, event_t event)
{
  /* Rime Dryad	|G	0x000000
   * Creature - Dryad 1/2
   * Snow |H2forestwalk */

  int result;
  subtype_t subt;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if (((result == 0) && (attacking_card_controller == player)) && (attacking_card == card))
    {
      subt = get_hacked_subtype(player, card, 0x5001);
      result = count_snow_landtype(1 - current_turn, subt);
      if (result != 0)
      {
        event_result = 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x24071f0; original cards/ice_age.c:8139. */
int card_ritual_of_subdual(int player, int card, event_t event)
{
  /* Ritual of Subdual	|4|G|G	0x000000
   * Enchantment
   * Cumulative upkeep |2
   * If a land is tapped for mana, it produces colorless mana instead of any other type. */

  int p;

  cumulative_upkeep(player, card, event, 2, 0, 0, 0, 0, 0);
  if (event == EVENT_CAST_SPELL)
  {
    if (is_what(affected_card_controller, affected_card, 1) != 0)
    {
      set_special_flags2(affected_card_controller, affected_card, 0x40);
    }
  }
  else if ((event == EVENT_COUNT_MANA) || (event == EVENT_RESOLVE_SPELL))
  {
    for (p = 0; p < 2; ++p)
    {
      int initial_count = active_cards_count[p];
      for (int color = 0; color < 7; ++color)
        mana_color_override[p][color] = 0;
      if (initial_count > 0)
        for (int land = 0; land < active_cards_count[p]; ++land)
          if (in_play(p, land) && is_what(p, land, 1))
          {
            set_special_flags2(p, land, 0x40);
          }
    }
  }
  return global_enchantment(player, card, event);
}

int card_runed_arch(int player, int card, event_t event)
{
	/* Runed Arch	|3	0x2009cce
	 * Artifact
	 * ~ enters the battlefield tapped.
	 * |X, |T, Sacrifice ~: X target creatures with power 2 or less can't be blocked this turn. */

  comes_into_play_tapped(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;
  td.power_requirement = 2 | TARGET_PT_LESSER_OR_EQUAL;

  if (event == EVENT_CAN_ACTIVATE)
	{
	  if (player == AI || ai_is_speculating == 1)
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET|GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST_X(1), 0, &td, "TARGET_CREATURE");
	  else
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(0), 0, NULL, NULL);
	}

  if (event == EVENT_ACTIVATE)
	{
	  int old_max_x_value = max_x_value;
	  max_x_value = target_available(player, card, &td);
	  if (max_x_value == 0 || charge_mana_for_activated_ability(player, card, MANACOST_X(-1)))
		{
		  max_x_value = old_max_x_value;
		  int num_tgts = x_value;

		  if (pick_up_to_n_targets(&td, "TAWNOS_WAND", num_tgts) == num_tgts)	// "Select target creature with power 2 or less."
			{
			  tap_card(player, card);
			  kill_card(player, card, KILL_SACRIFICE);
			}
		}
	  else
		max_x_value = old_max_x_value;
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int i;
	  for (i = 0; i < instance->number_of_targets; ++i)
		if (validate_target(player, card, &td, i))
		  pump_ability_until_eot(player, card, instance->targets[i].player, instance->targets[i].card, 0, 0, 0, SP_KEYWORD_UNBLOCKABLE);
	}

  return 0;
}

int card_sacred_boon(int player, int card, event_t event)
{
  /* Sacred Boon	|1|W	0x200d84c
   * Instant
   * Prevent the next 3 damage that would be dealt to target creature this turn. At the beginning of the next end step, put a +0/+1 counter on that creature for each 1 damage prevented this way. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.preferred_controller = player;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  card_instance_t* legacy = get_card_instance(player, create_targetted_legacy_effect(player, card, &effect_scars_of_the_veteran,
																							 inst->targets[0].player, inst->targets[0].card));
		  legacy->targets[1].card = legacy->counter_toughness = 3;
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x24077f0; original cards/ice_age.c:8272. */
int card_seizures(int player, int card, event_t event)
{
  /* Seizures	|1|B	0x000000
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature becomes tapped, ~ deals 3 damage to that creature's controller unless that player pays |3. */

  card_instance_t *instance;
  int result;

  instance = get_card_instance(player, card);

  if (((instance->damage_target_player < '\0') ||
       (result = is_humiliated(player, card, AL_TRIG), result != 0)) ||
      (instance = get_card_instance(player, card), event != EVENT_TAP_CARD))
  {
    if (event == EVENT_CAN_CAST)
      return disabling_aura(player, card, event);
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
        return disabling_aura(player, card, event);
    }
    else if ((event == EVENT_RESOLVE_SPELL) ||
             ((event == EVENT_CAN_CHANGE_TARGET || (event == EVENT_CHANGE_TARGET))))
      return disabling_aura(player, card, event);
  }
  else if (((instance->damage_target_card == affected_card) &&
            (instance->damage_target_player == affected_card_controller)) &&
           ((result = has_mana((int)instance->damage_target_player, 0, 3),
             result == 0 || (charge_mana_while_resolving(player, card, EVENT_TAP_CARD,
                                                         (int)instance->damage_target_player,
                                                         COLOR_COLORLESS, 3),
                             spell_fizzled == 1))))
  {
    damage_player((int)instance->damage_target_player, 3, player, card);
  }
  if (((event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA) &&
      (((((2 < (uint32_t)(event - EVENT_POWER) && (event != EVENT_RESOLVE_MOVING_AURA)) &&
          (event != EVENT_CHANGE_TYPE)) &&
         ((event != EVENT_SET_COLOR && (event != EVENT_GET_SELECTED_CARD)))) &&
        (event != EVENT_STATIC_EFFECTS))))
  {
    return 0;
  }
  return disabling_aura(player, card, event);
}

int card_shambling_strider(int player, int card, event_t event)
{
  /* Shambling Strider	|4|G|G	0x200d815
   * Creature - Yeti 5/5
   * |R|G: ~ gets +1/-1 until end of turn. */
  return generic_shade_merge_pt(player, card, event, 0, MANACOST_RG(1,1), 1,-1);
}

int card_shield_of_the_ages(int player, int card, event_t event){
	/* Shield of the Ages	|2	0x2002E51
	 * Artifact
	 * |2: Prevent the next 1 damage that would be dealt to you this turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;
	td.required_type = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_LITERAL_PROMPT, MANACOST_X(2), 0,
									&td, "Select a damage card that will damage you.");
}

/* ManalinkEh 2022: 0x2407b80; original cards/ice_age.c:8349. */
int card_shyft(int player, int card, event_t event)
{
  /* Shyft	|4|U	0x000000	[Dream Coat]
   * Creature - Shapeshifter 4/2
   * At the beginning of your upkeep, you may have ~ become the color or colors of your choice. */

  int result;
  card_instance_t *instance;
  unsigned int chosen_colors;

  result = is_humiliated(player, card, AL_TRIG);
  if (result == 0)
  {

    result = duh_mode(player);
    upkeep_trigger_ability_mode(player, card, event, player,
                                (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
    instance = get_card_instance(player, card);
    if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
    {

      result = get_deck_color(player, 1 - player);
      chosen_colors = 0;
      if (player == 1)
      {
        chosen_colors = 1 << ((unsigned char)result & 0x1f);
      }
      else
      {
        while (result = dialog_impl(player, card, EVENT_CAN_CAST, DLG_RANDOM, DLG_NO_STORAGE, "Black",
                                    (unsigned int)((chosen_colors & 2) == 0), 1, "Blue",
                                    (unsigned int)((chosen_colors & 4) == 0), 1, "Green",
                                    (unsigned int)((chosen_colors & 8) == 0), 1, "Red",
                                    (unsigned int)((chosen_colors & 0x10) == 0), 1, "White",
                                    (unsigned int)((chosen_colors & 0x20) == 0), 1, 0, 0, 0, 0),
               result != 0)
        {

          chosen_colors = chosen_colors | 1 << ((unsigned char)result & 0x1f);
        }
      }
      instance = get_card_instance(player, card);
      instance->targets[1].card = chosen_colors;
    }
    else
    {
      result = instance->targets[1].card;
      if ((event == EVENT_SET_COLOR) && (-1 < result))
      {
        if ((card == affected_card) && (player == affected_card_controller))
        {
          event_result = result;
        }
      }
      else if (event == EVENT_CLEANUP)
      {
        instance = get_card_instance(player, card);
        instance->targets[2].player = 0;
      }
    }
  }
  return 0;
}

int card_sibilant_spirit(int player, int card, event_t event)
{
  /* Sibilant Spirit	|5|U	0x200d761
   * Creature - Spirit 5/6
   * Flying
   * Whenever ~ attacks, defending player may draw a card. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
    draw_some_cards_if_you_want(player, card, 1-current_turn, 1);
  return 0;
}

int card_skeleton_ship(int player, int card, event_t event)
{
  /* Skeleton Ship	|3|U|B	0x2009cd3
   * Legendary Creature - Skeleton 0/3
   * When you control no |H1Islands, sacrifice ~.
   * |T: Put a -1/-1 counter on target creature. */

  if (event == EVENT_ABILITIES)
  {
    if (card == affected_card && player == affected_card_controller &&
        !check_battlefield_for_subtype(player, TYPE_LAND, 0x5002))
      kill_card(player, card, KILL_SACRIFICE);
  }
  else if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                   COUNTER_M1_M1, 1);
    return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                       "TARGET_CREATURE");
  }
  return 0;
}

int card_skull_catapult(int player, int card, event_t event){
	/* Skull Catapult	|4	0x2009cd8
	 * Artifact
	 * |1, |T, Sacrifice a creature: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.allow_cancel = 0;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 2);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE | GAA_CAN_TARGET, MANACOST_X(1), 0,
									&td, "TARGET_ANY");
}

int card_snow_devil(int player, int card, event_t event)
{
  /* Snow Devil	|1|U	0x200d77a
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has flying.
   * Enchanted creature has first strike as long as it's blocking and you control a snow land. */

	card_instance_t* instance = in_play(player, card);
	if (event == EVENT_ABILITIES && instance){
		if (affect_me(instance->damage_target_player, instance->damage_target_card)){
			card_instance_t* attached = in_play(instance->damage_target_player, instance->damage_target_card);
			if (attached && (attached->state & STATE_BLOCKING) && control_a_snow_land(player, 0)){
				event_result |= KEYWORD_FIRST_STRIKE;
			}
		}
	}else if (event == EVENT_PHASE_CHANGED && current_turn == 1-player
		&& (unsigned int)(current_phase - PHASE_AFTER_BLOCKING) < 6 && instance){
		card_instance_t* attached = in_play(instance->damage_target_player, instance->damage_target_card);
		if (attached){ attached->regen_status |= KEYWORD_RECALC_ABILITIES; }
	}
	return generic_aura(player, card, event, player, 0, 0, KEYWORD_FLYING, 0, 0, 0, 0);
}

int card_snow_fortress(int player, int card, event_t event)
{
  /* Snow Fortress	|5	0x200d838
   * Artifact Creature - Wall 0/4
   * Defender
   * |1: ~ gets +1/+0 until end of turn.
   * |1: ~ gets +0/+1 until end of turn.
   * |3: ~ deals 1 damage to target creature without flying that's attacking you. */

  if (event == EVENT_POW_BOOST)
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1),	1,0);
  if (event == EVENT_TOU_BOOST)
	return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1),	0,1);

  if (IS_ACTIVATING(event))
	{
	  if (event == EVENT_CAN_ACTIVATE && !can_use_activated_abilities(player, card))
		return 0;

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.required_state = TARGET_STATE_ATTACKING;
	  td.illegal_abilities |= KEYWORD_FLYING;

	  enum
	  {
		CHOICE_POW = 1,
		CHOICE_TGH,
		CHOICE_DMG
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"+1/+0",	1, 1, DLG_MANA(MANACOST_X(1)),
						"+0/+1",	1, 1, DLG_MANA(MANACOST_X(1)),
						"Damage",	1, 1, DLG_MANA(MANACOST_X(3)), DLG_LITERAL_TARGET(&td, "Select target creature without flying that's attacking you."));

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_RESOLVE_ACTIVATION)
		switch (choice)
		  {
			case CHOICE_POW:
			  return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1),	1,0);
			case CHOICE_TGH:
			  return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1),	0,1);
			case CHOICE_DMG:
			  if (valid_target(&td))
				damage_target0(player, card, 1);
			  break;
		  }
	}

  return 0;
}

int card_snow_hound(int player, int card, event_t event)
{
  /* Snow Hound	|2|W	0x200d824
   * Creature - Hound 1/1
   * |1, |T: Return ~ and target |Sgreen or |Sblue creature you control to their owner's hand. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.allowed_controller = td.preferred_controller = player;
  td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE|COLOR_TEST_GREEN);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (in_play(inst->parent_controller, inst->parent_card))
		bounce_permanent(inst->parent_controller, inst->parent_card);
	  bounce_permanent(inst->targets[0].player, inst->targets[0].card);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_LITERAL_PROMPT, MANACOST_X(1), 0, &td,
								   get_sleighted_color_text2(player, card, "Select target %s or %s creature you control.", COLOR_GREEN, COLOR_BLUE));
}

int card_snowblind(int player, int card, event_t event)
{
  /* Snowblind	|3|G	0x200d7de
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets -X/-Y. If that creature is attacking, X is the number of snow lands defending player controls. Otherwise, X is the number of snow lands its controller controls. Y is equal to X or to enchanted creature's toughness minus 1, whichever is smaller. */

  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && ! is_humiliated(player, card, AL_STATIC) )
	{
	  card_instance_t* inst = in_play(player, card);
	  if (inst && affect_me(inst->damage_target_player, inst->damage_target_card))
		{
		  card_instance_t* aff = get_card_instance(inst->damage_target_player, inst->damage_target_card);
		  int old_event_result = event_result;
		  event_result -= count_snow_permanents((aff->state & STATE_ATTACKING) ? 1-current_turn : inst->damage_target_player, TYPE_LAND, 0);
		  if (event == EVENT_TOUGHNESS && event_result < 1 && old_event_result >= 1)
			event_result = 1;
		}
	}

  if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

  return vanilla_aura(player, card, event, 1-player);
}

/* ManalinkEh 2022: 0x2408ac0; original cards/ice_age.c:8620. */
int card_soldevi_golem(int player, int card, event_t event)
{
  /* Soldevi Golem	|4	0x000000
   * Artifact Creature - Golem 5/3
   * ~ doesn't untap during your untap step.
   * At the beginning of your upkeep, you may untap target tapped creature an opponent controls. If you do, untap ~. */

  resolve_trigger_t trigger_mode;
  card_instance_t *instance;
  int result;
  unsigned int toughness;
  target_definition_t td;
  does_not_untap(player, card, event);
  if (!((((current_turn != player) || (trigger_condition != TRIGGER_UPKEEP)) || (card != affected_card)) ||
        (player != affected_card_controller)))
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_TAPPED;
    trigger_mode = RESOLVE_TRIGGER_0;
    if (can_target(&td) && (player != 1 || check_state(1, card, 0x10)))
      trigger_mode = (duh_mode(player) != 0) + RESOLVE_TRIGGER_OPTIONAL;
    upkeep_trigger_ability_mode(player, card, event, player, trigger_mode);
  }
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.allowed_controller = 1 - player;
  td.required_state = TARGET_STATE_TAPPED;
  td.preferred_controller = td.allowed_controller;
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    toughness = get_toughness(player, card);
    td.power_requirement = toughness | 0x2000;
  }
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "Select target tapped creature an opponent controls.", 0, 0x80);
    if (result != 0)
    {
      untap_card(instance->targets[0].player, instance->targets[0].card);
      untap_card(player, card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

int card_soldevi_simulacrum(int player, int card, event_t event)
{
	/* Soldevi Simulacrum	|4	0x2002E56
	 * Artifact Creature - Soldier 2/4
	 * Cumulative upkeep |1
	 * |1: ~ gets +1/+0 until end of turn. */

  cumulative_upkeep(player, card, event, MANACOST_X(1));

  return generic_shade_merge_pt(player, card, event, 0, MANACOST_X(1), 1,0);
}

int card_songs_of_the_damned(int player, int card, event_t event){
	/* Songs of the Damned	|B	0x2002E5B
	 * Instant
	 * Add |B to your mana pool for each creature card in your graveyard. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		int amount = count_graveyard_by_type(player, TYPE_CREATURE);
		if( amount <= 1 ){
			ai_modifier-=25;
		}
		else{
			ai_modifier += ((amount-1)*5);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		produce_mana(player, COLOR_BLACK, count_graveyard_by_type(player, TYPE_CREATURE));
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int soul_burn_legacy(int player, int card, event_t event);

int card_soul_barrier(int player, int card, event_t event){
	/* Soul Barrier	|2|U	0x2002E60
	 * Enchantment
	 * Whenever an opponent casts a creature spell, ~ deals 2 damage to that player unless he or she pays |2. */

	sp_ability_trigger(1-player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		int choice = 0;
		if (has_mana(1-player, COLOR_COLORLESS, 2)){
			choice = do_dialog(1-player, player, card, -1, -1, " Pay 2\n Take 2 damage", life[player] >= 8);
		}
		if (choice == 0){
			charge_mana(1-player, COLOR_COLORLESS, 2);
			choice = spell_fizzled;
		}
		if (choice == 1){ damage_player(1-player, 2, player, card); }
	}
	return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x23ecde0; original cards/ice_age.c:8730. */
int soul_burn_legacy(int player, int card, event_t event)
{
  int target_card;
  int player_00;
  card_instance_t *instance;
  card_instance_t *damage;
  int result;
  unsigned int life_gain;
  unsigned int target_limit;
  unsigned int damage_amount;

  instance = get_card_instance(player, card);
  if (event == EVENT_DEAL_DAMAGE)
  {
    damage = get_card_instance(affected_card_controller, affected_card);
    damage_amount = damage->info_slot;
    if ((damage->internal_card_id == damage_card) && (damage->display_pic_csv_id == 0x840))
    {
      result = damage->damage_target_card;
      target_card = instance->targets[0].card;
      if ((result == target_card) &&
          ((player_00 = instance->targets[0].player,
            damage->damage_target_player == player_00 && (0 < (int)damage_amount))))
      {
        if (instance->targets[1].player < 0)
        {
          instance->targets[1].player = 0;
        }
        if (result == -1)
        {
          target_limit = life[player_00];
        }
        else
        {
          result = is_what(player_00, target_card, 2);
          if (result == 0)
          {
            target_limit = count_counters(instance->targets[0].player, instance->targets[0].card,
                                          COUNTER_LOYALTY);
            damage_amount = damage->info_slot;
          }
          else
          {
            target_limit =
                get_toughness((int)damage->damage_target_player, damage->damage_target_card);
            damage_amount = damage->info_slot;
          }
        }
        life_gain = instance->targets[1].player;
        if ((int)damage_amount < (int)life_gain)
        {
          life_gain = damage_amount;
        }
        target_limit = (int)(target_limit ^ 0xffffffff) >> 0x1f & target_limit;
        if ((int)target_limit < (int)life_gain)
        {
          life_gain = target_limit;
        }
        instance->targets[1].player = life_gain;
      }
    }
    if (damage_amount == 0)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  result = instance->targets[1].player;
  if ((((0 < result) && (trigger_condition == TRIGGER_DEAL_DAMAGE)) && (card == affected_card)) &&
      ((player == affected_card_controller && (reason_for_trigger_controller == player))))
  {
    if (event == EVENT_TRIGGER)
    {
      event_result = event_result | 2;
    }
    else if (event == EVENT_RESOLVE_TRIGGER)
    {
      gain_life(player, result);
      kill_card(player, card, KILL_EXILE);
    }
    else if (event == EVENT_END_TRIGGER)
    {
      instance->targets[1].player = 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2408f80; original cards/ice_age.c:8775. */
int card_soul_burn(int player, int card, event_t event)
{
  /* Soul Burn	|X|2|B	0x000000
   * Sorcery
   * Spend only |Sblack and/or |Sred mana on X.
   * ~ deals X damage to any target. You gain life equal to the damage dealt, but not more than the amount of |B spent on X, the player's life total before the damage was dealt, the planeswalker's loyalty before the damage was dealt, or the creature's toughness. */

  card_instance_t *instance;
  int result;
  card_instance_t *legacy;
  bool paid;
  target_definition_t td;

  if (event != EVENT_CAN_CAST)
  {
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        default_target_definition(player, card, &td, 0x102);
        td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
        instance = get_card_instance(player, card);
        if ((card == affected_card) && (player == affected_card_controller))
        {
          set_special_flags2(player, card, 0x20000);
          result = check_special_flags2(player, card, -0x80000000);
          if (result == 0)
          {
            instance->info_slot = 0;
          }
          result = played_for_free(player, card);
          if ((result == 0) && (result = is_token(player, card), result == 0))
          {
            charge_mana_multi(player, 0, -1, 0, 0, -1, 0);
            paid = spell_fizzled != 1;
            instance->targets[1].player = mana_paid[1];
            if (paid)
            {
              set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
            }
          }
          return generic_spell(player, card, EVENT_CAST_SPELL, 1, &td, "TARGET_ANY", 1, NULL);
        }
      }
      return 0;
    }
    if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
    {
      default_target_definition(player, card, &td, 0x102);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      instance = get_card_instance(player, card);
      if ((event != EVENT_CAN_CHANGE_TARGET) && (event != EVENT_CHANGE_TARGET))
      {
        if (event != EVENT_RESOLVE_SPELL)
        {
          return 0;
        }
        result = valid_target(&td);
        if ((result != 0) && (0 < instance->info_slot))
        {
          result = create_legacy_effect(player, card, soul_burn_legacy);
          legacy = get_card_instance(player, result);
          legacy->targets[0].player = instance->targets[0].player;
          legacy->targets[0].card = instance->targets[0].card;
          legacy->targets[1].player = instance->targets[1].player;
          damage_target0(player, card, instance->info_slot);
        }
        kill_card(player, card, KILL_DESTROY);
        return 0;
      }
      return generic_spell(player, card, event, 5, &td, "TARGET_ANY", 1, NULL);
    }
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
  }
  default_target_definition(player, card, &td, 0x102);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  get_card_instance(player, card);
  return generic_spell(player, card, event, 5, &td, "TARGET_ANY", 1, NULL);
}

int card_spoils_of_evil(int player, int card, event_t event){
	/* Spoils of Evil	|2|B	0x2002E65
	 * Instant
	 * For each artifact or creature card in target opponent's graveyard, add |1 to your mana pool and you gain 1 life. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) && player == AI ){
		int amount = count_graveyard_by_type(player, TYPE_CREATURE | TYPE_ARTIFACT);
		if( amount && life[player] < 6 ){
			ai_modifier += 50;
		}
		if( amount <= 3 ){
			ai_modifier-=25;
		}
		else{
			ai_modifier += ((amount-3)*5);
		}
	}

	if( event == EVENT_RESOLVE_SPELL){
		int amount = count_graveyard_by_type(1-player, TYPE_CREATURE | TYPE_ARTIFACT);
		produce_mana(player, COLOR_COLORLESS, amount);
		gain_life(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_spoils_of_war(int player, int card, event_t event)
{
  /* Spoils of War	|X|B	0x2002E6A
   * Sorcery
   * X is the number of artifact and/or creature cards in an opponent's graveyard as you cast ~.
   * Distribute X +1/+1 counters among any number of target creatures. */

  int i;
  target_t *target;
  int t_player;
  target_t *selected_target;
  card_instance_t *instance;
  int remaining;
  int counters_or_cost;
  int target_or_number;
  unsigned int updated_cost;
  int mode_or_index;
  unsigned int cost_or_index;
  int valid_count;
  unsigned int target_limit;
  int validated[10];
  target_definition_t td;

  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 2;
    get_card_instance(player, card);
    counters_or_cost = count_graveyard_by_type(1 - player, 0x42);
    if (0 < counters_or_cost)
    {
      counters_or_cost =
          get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, counters_or_cost);
      if (has_mana_multi(player, counters_or_cost, 1, 0, 0, 0, 0) != 0)
      {
        return generic_spell(player, card, EVENT_CAN_CAST, 5, &td, NULL, 1, NULL);
      }
    }
  }
  else if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      td.allow_cancel = 2;
      instance = get_card_instance(player, card);
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->info_slot = 0;
        instance->number_of_targets = '\0';
        counters_or_cost = count_graveyard_by_type(1 - player, 0x42);
        mode_or_index = true_get_updated_casting_cost(player, card, -1, EVENT_CAST_SPELL, -1);
        updated_cost = counters_or_cost + mode_or_index;
        cost_or_index = (int)(updated_cost ^ 0xffffffff) >> 0x1f & updated_cost;
        charge_mana(player, COLOR_COLORLESS, cost_or_index);
        if (spell_fizzled != 1)
        {
          target_limit = cost_or_index;
          if (10 < (int)cost_or_index)
          {
            target_limit = 10;
          }
          i = 0;
          if (0 < (int)updated_cost)
          {
            do
            {
              if (new_pick_target(&td, "TARGET_CREATURE", i, 0) == 0)
                break;
              selected_target = instance->targets + i;
              target = instance->targets + i;
              i = i + 1;
              state_untargettable(target->player, selected_target->card, 1);
            } while (i < (int)target_limit);
          }
          if (instance->number_of_targets == '\0')
          {
            spell_fizzled = 1;
            return 0;
          }
          i = 0;
          do
          {
            selected_target = instance->targets + i;
            target = instance->targets + i;
            i = i + 1;
            state_untargettable(target->player, selected_target->card, 0);
          } while (i < (int)(unsigned int)instance->number_of_targets);
          instance->info_slot = cost_or_index;
        }
      }
    }
  }
  else if (((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET)) ||
           (event == EVENT_CHANGE_TARGET))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    td.allow_cancel = 2;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_SPELL)
    {
      counters_or_cost = instance->info_slot;
      if (instance->number_of_targets != '\0')
      {
        valid_count = 0;
        mode_or_index = 0;
        do
        {
          remaining = validate_target(player, card, &td, mode_or_index);
          if (remaining != 0)
          {
            valid_count = valid_count + 1;
          }
          validated[mode_or_index] = (unsigned int)(remaining != 0);
          mode_or_index = mode_or_index + 1;
        } while (mode_or_index < (int)(unsigned int)instance->number_of_targets);
        if ((valid_count != 0) &&
            (mode_or_index = do_dialog(player, player, card, -1, -1, " Auto mode\n Manual mode", 0),
             counters_or_cost != 0))
        {
          cost_or_index = 0;
          do
          {
            remaining = counters_or_cost;
            if (validated[cost_or_index] != 0)
            {
              target_or_number = instance->targets[cost_or_index].card;
              t_player = instance->targets[cost_or_index].player;
              if (mode_or_index == 1)
              {
                if (do_dialog(player, t_player, target_or_number, -1, -1,
                              " Add counters to this creature\n Pass", 0) == 0)
                {
                  add_state(instance->targets[cost_or_index].player,
                            instance->targets[cost_or_index].card, 0x200000);
                  target_or_number = choose_a_number(
                      player, "How many +1/+1 counter to this creature?", counters_or_cost);
                  if ((counters_or_cost < target_or_number) || (target_or_number < 1))
                  {
                    remaining = 0;
                  }
                  else
                  {
                    remaining = counters_or_cost - target_or_number;
                    counters_or_cost = target_or_number;
                  }
                  remove_state(instance->targets[cost_or_index].player,
                               instance->targets[cost_or_index].card, 0x200000);
                  add_counters(player, card, instance->targets[cost_or_index].player,
                               instance->targets[cost_or_index].card, COUNTER_P1_P1,
                               counters_or_cost);
                }
              }
              else
              {
                add_counters(player, card, t_player, target_or_number, COUNTER_P1_P1, 1);
                remaining = counters_or_cost + -1;
              }
            }
            cost_or_index = cost_or_index + 1;
            if (instance->number_of_targets == cost_or_index)
            {
              cost_or_index = 0;
            }
            counters_or_cost = remaining;
          } while (remaining != 0);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_staff_of_the_ages(int player, int card, event_t event){
	/* Staff of the Ages	|3	0x200c541
	 * Artifact
	 * Creatures with landwalk abilities can be blocked as though they didn't have those abilities. */

	landwalk_disabling_card(player, card, event, PB_LANDWALK_DISABLED_MASK);
	return 0;
}

int card_stampede(int player, int card, event_t event){
	/* Stampede	|1|G|G	0x2002E6F
	 * Instant
	 * Attacking creatures get +1/+0 and gain trample until end of turn. */

	if( event==EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "");
		this_test.state = STATE_ATTACKING;
		pump_creatures_until_eot(player, card, current_turn, 0, 1, 0, KEYWORD_TRAMPLE, 0, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_stone_spirit(int player, int card, event_t event)
{
  /* Stone Spirit	|4|R	0x200d766
   * Creature - Elemental Spirit 4/3
   * ~ can't be blocked by creatures with flying. */

  if (event == EVENT_BLOCK_LEGALITY && attacking_card == card && attacking_card_controller == player
	  && check_for_ability(affected_card_controller, affected_card, KEYWORD_FLYING)
	  && !is_humiliated(player, card, AL_STATIC))
	event_result = 1;

  return 0;
}

int card_stonehands(int player, int card, event_t event)
{
  /* Stonehands	|2|R	0x200d76b
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +0/+2.
   * |R: Enchanted creature gets +1/+0 until end of turn. */

  if (event == EVENT_TOUGHNESS)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (affect_me(inst->damage_target_player, inst->damage_target_card))
		event_result += 2;
	}

  return card_firebreathing(player, card, event);
}

int card_storm_spirit(int player, int card, event_t event)
{
  /* Storm Spirit	|3|G|W|U	0x200d7a2
   * Creature - Elemental Spirit 3/3
   * Flying
   * |T: ~ deals 2 damage to target creature. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	damage_target0(player, card, 2);

  return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_stormbind(int player, int card, event_t event){
	/* Stormbind	|1|R|G	0x200223f
	 * Enchantment
	 * |2, Discard a card at random: ~ deals 2 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DISCARD, MANACOST_X(2), 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
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

/* ManalinkEh 2022: 0x240a380; original cards/ice_age.c:9164. */
int card_stromgald_cabal(int player, int card, event_t event)
{
  /* Stromgald Cabal	|1|B|B	0x000000
   * Creature - Human Knight 2/2
   * |T, Pay 1 life: Counter target |Swhite spell. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    counterspell_target_definition(player, card, &td, 0xff);
    td.required_color = get_sleighted_color_test(player, card, 0x20);
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    counterspell_target_definition(player, card, &td, 0xff);
    td.required_color = get_sleighted_color_test(player, card, 0x20);
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (result = counterspell_validate(player, card, &td, 0), result != 0))
    {
      real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
    }
  }
  return generic_activated_ability(player, card, event, 0x401, 0, 0, 0, 0, 0, 0, 1, &td, NULL);
}

int card_stunted_growth(int player, int card, event_t event){
	/* Stunted Growth	|3|G|G	0x2002E74
	 * Sorcery
	 * Target player chooses three cards from his or her hand and puts them on top of his or her library in any order. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			card_instance_t* instance = get_card_instance(player, card);

			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to put on top.");
			test.qty = MIN(3, hand_count[instance->targets[0].player]);
			new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, TUTOR_DECK, 1, AI_MIN_VALUE, &test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x240a6c0; original cards/ice_age.c:9221. */
int card_sunstone(int player, int card, event_t event)
{
  /* Sunstone	|3	0x000000
   * Artifact
   * |2, Sacrifice a snow land: Prevent all combat damage that would be dealt this turn. */

  int c;
  int value;
  card_instance_t *instance;
  int unblocked_power;
  test_definition_t test2;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    new_default_test_definition(&test2, 1, "Select a snow land to sacrifice.");
    test2.subtype = 0x2003;
    if (event == EVENT_ACTIVATE)
    {

      if ((1 - player == current_turn) && (c = active_cards_count[current_turn] + -1, -1 < c))
      {
        unblocked_power = 0;
        do
        {
          instance = in_play(current_turn, c);
          if ((((instance != NULL) && (value = is_what(current_turn, c, 2), value != 0)) &&
               (value = is_attacking(current_turn, c), value != 0)) &&
              (value = is_unblocked(current_turn, c), value != 0))
          {
            value = get_power(current_turn, c);
            unblocked_power = unblocked_power + value;
          }
          if ((4 < unblocked_power) ||
              (value = ai_modifier + -2, life[current_turn] <= unblocked_power))
          {
            value = ai_modifier + 0x40;
          }
          c = c + -1;
          ai_modifier = value;
        } while (c != -1);
      }
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
      }
      return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_NONE, 2, 0,
                                                              0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE,
                                                              &test2, SARV_0);
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    new_default_test_definition(&test2, 1, "Select a snow land to sacrifice.");
  }
  test2.subtype = 0x2003;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {

    instance = get_card_instance(player, card);
    fog_effect(instance->parent_controller, instance->parent_card);
  }
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
  }
  return generic_activated_ability_with_sacrifice_as_cost(
      player, card, event, GAA_NONE, 2, 0, 0, 0, 0, 0, 0, NULL, NULL, TYPE_NONE, &test2, SARV_0);
}

int card_tarpan(int player, int card, event_t event)
{
  /* Tarpan	|G	0x200d7d9
   * Creature - Horse 1/1
   * When ~ dies, you gain 1 life. */

  if (this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY))
	gain_life(player, 1);
  return 0;
}

int card_thermokarst(int player, int card, event_t event){
	/* Thermokarst	|1|G|G	0x2002E01
	 * Sorcery
	 * Destroy target land. If that land was a snow land, you gain 1 life. */

  return ia_land_destructor(player, card, event, 1);
}

/* ManalinkEh 2022: 0x240a9f0; original cards/ice_age.c:9293. */
int card_thoughtleech(int player, int card, event_t event)
{
  /* Thoughtleech	|G|G	0x000000
   * Enchantment
   * Whenever |Han Island an opponent controls becomes tapped, you may gain 1 life. */

  int result;
  card_instance_t *instance;
  subtype_t subtype;

  if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (affected_card_controller == player)) && (player == 1))
    {
      result = count_subtype(0, 0x47, 0x5002);
      ai_modifier = ai_modifier + result * 5;
    }
  }
  else if ((event == EVENT_TAP_CARD) && (1 - player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        subtype = get_hacked_subtype(player, card, 0x5002);
        result = has_subtype(affected_card_controller, affected_card, subtype);
        if (result != 0)
        {
          result = do_dialog(player, player, card, -1, -1, " Gain 1 life\n Decline", 0);
          if (result == 0)
          {
            gain_life(player, 1);
          }
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

int card_thunder_wall(int player, int card, event_t event)
{
  /* Thunder Wall	|1|U|U	0x200d73e
   * Creature - Wall 0/2
   * Defender
   * Flying
   * |U: ~ gets +1/+1 until end of turn. */
  return generic_shade_merge_pt(player, card, event, 0, MANACOST_U(1), 1,1);
}

int card_time_bomb(int player, int card, event_t event)
{
	/* Time Bomb	|4	0x2002E79
	 * Artifact
	 * At the beginning of your upkeep, put a time counter on ~.
	 * |1, |T, Sacrifice ~: ~ deals damage equal to the number of time counters on it to each creature and each player. */

	upkeep_trigger_ability(player, card, event, player);

    card_instance_t *instance = get_card_instance(player, card);

	if (event == EVENT_UPKEEP_TRIGGER_ABILITY){
		add_counters(player, card, player, card, COUNTER_TIME, 1);
	}

    if( event == EVENT_CAN_ACTIVATE ){
		 return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
    }

	if( event == EVENT_ACTIVATE ){
	     instance->info_slot = count_counters(player, card, COUNTER_TIME);
		 if( instance->info_slot < 3 ){
			     ai_modifier -= 48;
		 }

	}
    if (event == EVENT_RESOLVE_ACTIVATION){
		new_damage_all(player, card, ANYBODY, instance->info_slot, NDA_PLAYER_TOO, NULL);
    }

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME, MANACOST_X(1), 0, NULL, NULL);
}

int card_tinder_wall(int player, int card, event_t event){
	/* Tinder Wall	|G	0x20018EE
	 * Creature - Plant Wall 0/3
	 * Defender
	 * Sacrifice ~: Add |R|R to your mana pool.
	 * |R, Sacrifice ~: ~ deals 2 damage to target creature it's blocking. */

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_COUNT_MANA && affect_me(player, card) && can_sacrifice_this_as_cost(player, card) ){
		     declare_mana_available(player, COLOR_RED, 2);
	}

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL) ){
			return can_produce_mana(player, card);
		}
		if( ! paying_mana() && current_phase == PHASE_AFTER_BLOCKING &&
				current_turn != player && instance->blocking < 255 && would_validate_arbitrary_target(&td, 1-player, instance->blocking )  ){
			return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_R(1), 0, NULL, NULL);
	    }
	}

	if( event == EVENT_ACTIVATE ){
		int choice = 0;
		if( ! paying_mana() && current_phase == PHASE_AFTER_BLOCKING &&
				current_turn != player && instance->blocking < 255 && would_validate_arbitrary_target(&td, 1-player, instance->blocking )  ){
			if( generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) )
			  {
				choice = do_dialog(player, player, card, -1, -1, " Generate Mana\n Damage creature it's blocking\n Cancel", 1);
			}
		}
		if( choice == 0){
			produce_mana(player, COLOR_RED, 2);
			kill_card(player, card, KILL_SACRIFICE);
		}
		if( choice == 1 ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_R(1)) && pick_target(&td, "TARGET_CREATURE") ){
				instance->targets[0].player = 1-player;
				instance->targets[0].card = instance->blocking;
				instance->info_slot = 1;
				instance->number_of_targets = 1;
			    kill_card(player, card, KILL_SACRIFICE);
			}
		}
        if( choice == 2 ){
			 spell_fizzled = 1;
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot > 0 && valid_target(&td) ){
			damage_creature(instance->targets[0].player, instance->targets[0].card, 2, player, card );
		}
	}

	return 0;
}

/* ManalinkEh 2022: 0x240b1b0; original cards/ice_age.c:9425. */
int card_total_war(int player, int card, event_t event)
{
  /* Total War	|3|R	0x000000
   * Enchantment
   * Whenever a player attacks with one or more creatures, destroy all untapped non-Wall creatures that player controls that didn't attack, except for creatures the player hasn't controlled continuously since the beginning of the turn. */

  int i;
  card_instance_t *instance;
  int triggered;
  card_instance_t *creature;
  int record_index_or_eligible;
  target_t *stored_creature;

  instance = in_play(player, card);
  if (((instance != NULL) && (triggered = is_humiliated(player, card, AL_TRIG), triggered == 0)) &&
      (triggered = declare_attackers_trigger(player, card, event, 0, current_turn, -1),
       triggered != 0))
  {

    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    i = active_cards_count[current_turn] + -1;
    if (-1 < i)
    {
      do
      {
        creature = in_play(current_turn, i);
        if (((creature != NULL) && (record_index_or_eligible = is_what(current_turn, i, 2),
                                    record_index_or_eligible != 0)) &&
            ((record_index_or_eligible = can_attack(current_turn, i),
              record_index_or_eligible != 0 &&
                  ((record_index_or_eligible = has_subtype(current_turn, i, SUBTYPE_WALL),
                    record_index_or_eligible == 0 &&
                        (record_index_or_eligible = instance->info_slot,
                         record_index_or_eligible < 0x13))))))
        {
          do
          {
            if (instance->targets[record_index_or_eligible].player == -1)
            {
              instance->targets[record_index_or_eligible].player = i;
              break;
            }
            if (instance->targets[record_index_or_eligible].card == -1)
            {
              instance->targets[record_index_or_eligible].card = i;
              break;
            }
            record_index_or_eligible = record_index_or_eligible + 1;
            instance->info_slot = record_index_or_eligible;
          } while (record_index_or_eligible != 0x13);
        }
        i = i + -1;
      } while (i != -1);
    }
    stored_creature = instance->targets;
    do
    {
      if (stored_creature->player == -1)
        break;
      if (check_state(current_turn, stored_creature->player, 0x40) == 0)
      {
        kill_card(current_turn, stored_creature->player, KILL_DESTROY);
      }
      stored_creature->player = -1;
      if (stored_creature->card == -1)
        break;
      if (check_state(current_turn, stored_creature->card, 0x40) == 0)
      {
        kill_card(current_turn, stored_creature->card, KILL_DESTROY);
      }
      stored_creature->card = -1;
      stored_creature = stored_creature + 1;
    } while (stored_creature != (target_t *)&instance->parent_controller);
    instance->info_slot = 0;
  }
  return global_enchantment(player, card, event);
}

int card_touch_of_death(int player, int card, event_t event)
{
	/* Touch of Death	|2|B	0x200d7ca
	 * Sorcery
	 * ~ deals 1 damage to target player or planeswalker. You gain 1 life.
	 * Draw a card at the beginning of the next turn's upkeep. */
  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  damage_target0(player, card, 1);
		  gain_life(player, 1);
		  cantrip(player, card, 1);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER_OR_PLANESWALKER", 1, NULL);
}

/* ManalinkEh 2022: 0x23ed670; original cards/ice_age.c:9511. */
static int effect_touch_of_vitae(int player, int card, event_t event)
{
  int result;
  card_instance_t *instance;

  if (event == EVENT_CLEANUP)
  {
    kill_card(player, card, KILL_EXILE);
    return 0;
  }
  result = attachment_granting_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 0, 0,
                                                 NULL, NULL);
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return result;
  }
  instance = get_card_instance(player, card);
  untap_card((int)instance->damage_target_player, instance->damage_target_card);
  return result;
}

/* ManalinkEh 2022: 0x240b560; original cards/ice_age.c:9529. */
int card_touch_of_vitae(int player, int card, event_t event)
{
  /* Touch of Vitae	|2|G	0x000000
   * Instant
   * Until end of turn, target creature gains haste and "|0: Untap this creature. Activate this ability only once."
   * Draw a card at the beginning of the next turn's upkeep. */

  int result;
  card_instance_t *instance;
  int card_00;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      instance = get_card_instance(player, card);
      pump_ability_until_eot(player, card, instance->targets[0].player,
                             instance->targets[0].card, 0, 0, 0, 4);
      result = instance->targets[0].player;
      if (result == player)
      {
        create_targetted_legacy_activate(player, card, effect_touch_of_vitae, player,
                                         instance->targets[0].card);
      }
      else
      {

        card_00 = add_card_to_hand(result, instance->internal_card_id);
        result = instance->targets[0].player;
        hand_count[result] = hand_count[result] + -1;
        create_targetted_legacy_activate(result, card_00, effect_touch_of_vitae, result,
                                         instance->targets[0].card);
        obliterate_card(instance->targets[0].player, card_00);
      }
      result = cantrip(player, card, 1);
      alternate_legacy_text(2, player, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td,
                         "Select target creature it gains haste and |0: Untap this creature once.",
                         1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_trailblazer(int player, int card, event_t event)
{
  /* Trailblazer	|2|G|G	0x200d7c5
   * Instant
   * Target creature can't be blocked this turn. */
  return vanilla_instant_pump(player, card, event, ANYBODY, player, 0,0, 0,SP_KEYWORD_UNBLOCKABLE);
}

int card_updraft(int player, int card, event_t event)
{
  /* Updraft	|1|U	0x200d798
   * Instant
   * Target creature gains flying until end of turn.
   * Draw a card at the beginning of the next turn's upkeep. */
  return vanilla_instant_pump(player, card, event, ANYBODY, player, 0,0, VANILLA_PUMP_CANTRIP|KEYWORD_FLYING,0);
}

int card_urzas_bauble(int player, int card, event_t event){
	/* Urza's Bauble	|0	0x20024F6
	 * Artifact
	 * |T, Sacrifice ~: Look at a card at random in target player's hand. You draw a card at the beginning of the next turn's upkeep. */

	if( !IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.zone = TARGET_ZONE_PLAYERS;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			if( hand_count[opp] > 0){
				int r =  internal_rand( hand_count[opp] );
				int cards_array[ 1 ];
				int i=0;
				int hand_index = 0;
				for(i=0;i<active_cards_count[opp]; i++){
					if( in_hand(opp, i) ){
						int id = get_card_instance(opp, i)->internal_card_id;
						if( id > -1 ){
							if( hand_index == r ){
								cards_array[0] = id;
							}
							hand_index++;
						}
					}
				}
				show_deck( player, cards_array, 1, "Urza's Bauble revealed...", 0, gs_done_008b40e0 );
			}
			cantrip(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

/* ManalinkEh 2022: 0x23edef0; original cards/ice_age.c:9636. */
static int venomous_breath_effect(int player, int card, event_t event)
{
  char attached_player;
  int target_card;
  card_instance_t *instance;
  instance = get_card_instance(player, card);
  if (-1 < instance->damage_target_player)
  {
    instance = get_card_instance(player, card);
    attached_player = instance->damage_target_player;
    target_card = instance->damage_target_card;
    if (end_of_combat_trigger(player, card, event, 2))
    {
      kill_card((int)attached_player, target_card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x23eb8d0; original cards/ice_age.c:9650. */
static void add_venomous_breath_effect(int player, int card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, venomous_breath_effect, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x240ba20; original cards/ice_age.c:9654. */
int card_venomous_breath(int player, int card, event_t event)
{
  /* Venomous Breath	|3|G	0x000000
   * Instant
   * Choose target creature. At this turn's next end of combat, destroy all creatures that blocked or were blocked by it this turn. */

  card_instance_t *instance;
  target_definition_t td;
  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      instance = get_card_instance(player, card);
      for_each_creature_blocked_by_me(instance->targets[0].player, instance->targets[0].card,
                                      add_venomous_breath_effect, player, card);
      for_each_creature_blocking_me(instance->targets[0].player, instance->targets[0].card,
                                    add_venomous_breath_effect, player, card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

int card_vertigo(int player, int card, event_t event)
{
  /* Vertigo	|R	0x200d85b
   * Instant
   * ~ deals 2 damage to target creature with flying. That creature loses flying until end of turn. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_abilities = KEYWORD_FLYING;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  negate_ability_until_eot(player, card, inst->targets[0].player, inst->targets[0].card, KEYWORD_FLYING);
		  damage_target0(player, card, 2);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_FLYING", 1, NULL);
}

int card_vexing_arcanix(int player, int card, event_t event){
	/* Vexing Arcanix	|4	0x2005039
	 * Artifact
	 * |3, |T: Target player names a card, then reveals the top card of his or her library.
	 * If it's the named card, the player puts it into his or her hand.
	 * Otherwise, the player puts it into his or her graveyard and ~ deals 2 damage to him or her. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int t_player = instance->targets[0].player;
			int card_id = -1;
			if( IS_AI(t_player) ){
				card_id = ai_name_a_random_csvid(player, card, t_player, TYPE_ANY, NULL);
			}
			else{
				test_definition_t test;
				new_default_test_definition(&test, TYPE_ANY, "Choose a card name.");
				card_id = new_card_from_list(t_player, 3, &test);
			}
			int iid = deck_ptr[t_player][0];
			int match = 0;
			if( iid > -1 ){
				reveal_card_iid(player, card, iid);

				if( cards_data[iid].id == card_id ){
					add_card_to_hand(t_player, iid);
					remove_card_from_deck(t_player, 0);
					match = 1;
				}
			}
			if( ! match ){
				mill(t_player, 1);
				damage_player(t_player, 2, player, instance->parent_card);
			}
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, "TARGET_PLAYER");
}

int card_vibrating_sphere(int player, int card, event_t event){
	/* Vibrating Sphere	|4	0x2009cdd
	 * Artifact
	 * As long as it's your turn, creatures you control get +2/+0.
	 * As long as it's not your turn, creatures you control get -0/-2. */

	if( ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_POWER && affected_card_controller == player && current_turn == player ){
			event_result+=2;
		}
		if( event == EVENT_TOUGHNESS && affected_card_controller == player && current_turn != player ){
			event_result-=2;
		}
	}
	return 0;
}

static int fx_walking_wall(int player, int card, event_t event)
{
  if (event == EVENT_ABILITIES || event == EVENT_POWER || event == EVENT_TOUGHNESS)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (!affect_me(inst->damage_target_player, inst->damage_target_card))
		return 0;
	  if (event == EVENT_ABILITIES)
		add_status(affected_card_controller, affected_card, STATUS_WALL_CAN_ATTACK);
	  else if (event == EVENT_POWER)
		event_result += inst->counter_power;
	  else if (event == EVENT_TOUGHNESS)
		event_result += inst->counter_toughness;
	}

  if (event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY)
	kill_card(player, card, KILL_EXILE);

  return 0;
}

int card_walking_wall(int player, int card, event_t event)
{
  /* Walking Wall	|4	0x200d879
   * Artifact Creature - Wall 0/6
   * Defender
   * |3: ~ gets +3/-1 until end of turn and can attack this turn as though it didn't have defender. Activate this ability only once each turn. */
  // Used externally
  if (event == EVENT_POW_BOOST && card_walking_wall(player, card, EVENT_CAN_ACTIVATE))
	return 3;

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  inst = get_card_instance(player, create_targetted_legacy_effect(player, card, fx_walking_wall, inst->parent_controller, inst->parent_card));
	  inst->counter_power = 3;
	  inst->counter_toughness = -1;
	}

  return generic_activated_ability(player, card, event, GAA_ONCE_PER_TURN, MANACOST_X(3), 0, NULL, NULL);
}

int card_wall_of_lava(int player, int card, event_t event)
{
  /* Wall of Lava	|1|R|R	0x200d743
   * Creature - Wall 1/3
   * Defender
   * |R: ~ gets +1/+1 until end of turn. */
  return generic_shade_merge_pt(player, card, event, 0, MANACOST_R(1), 1,1);
}

int card_war_chariot(int player, int card, event_t event)
{
  /* War Chariot	|3	0x200d81a
   * Artifact
   * |3, |T: Target creature gains trample until end of turn. */
  return vanilla_creature_pumper(player, card, event, MANACOST_X(3), GAA_UNTAPPED, 0,0, KEYWORD_TRAMPLE,0, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_warning(int player, int card, event_t event)
{
  /* Warning	|W	0x200d801
   * Instant
   * Prevent all combat damage that would be dealt by target attacking creature this turn. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE);
  td.required_state = TARGET_STATE_ATTACKING;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  negate_combat_damage_this_turn(player, card, inst->targets[0].player, inst->targets[0].card, 0);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ATTACKING_CREATURE", 1, NULL);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_whalebone_glider(int player, int card, event_t event){
	/* Whalebone Glider	|2	0x2009cf6
	 * Artifact
	 * |2, |T: Target creature with power 3 or less gains flying until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.preferred_controller = player;
	td.power_requirement = 3 | TARGET_PT_LESSER_OR_EQUAL;
	return vanilla_creature_pumper(player, card, event, MANACOST_X(2), GAA_UNTAPPED+GAA_NONSICK, 0, 0, KEYWORD_FLYING, 0, &td);
}

int card_white_scarab(int player, int card, event_t event)
{
  /* White Scarab	|W	0x200b5d3
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature can't be blocked by |Swhite creatures.
	 * Enchanted creature gets +2/+2 as long as an opponent controls a |Swhite permanent. */

  if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS || event == EVENT_BLOCK_LEGALITY)
    return scarab(player, card, event, COLOR_TEST_WHITE);
  return 0;
}

/* ManalinkEh 2022: 0x240c520; original cards/ice_age.c:9899. */
int card_whiteout(int player, int card, event_t event)
{
  /* Whiteout	|1|G	0x000000
   * Instant
   * All creatures lose flying until end of turn.
   * Sacrifice a snow land: Return ~ from your graveyard to your hand. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    negate_ability_until_eot(player, card, 2, -1, KEYWORD_FLYING);
    kill_card(player, card, KILL_DESTROY);
  }
  else if (event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS)
  {
    test_definition_t test2;
    new_default_test_definition(&test2, TYPE_LAND, "Select a snow land to sacrifice.");
    test2.subtype = 0x2003;
    if (event == EVENT_GRAVEYARD_ABILITY)
    {
      if (new_can_sacrifice(player, card, player, &test2) && spell_fizzled != 1)
        return 4;
    }
    else if (new_sacrifice(player, card, player, SAC_AS_COST, &test2) && spell_fizzled != 1)
      return 1;
  }
  return basic_spell(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_wiitigo(int player, int card, event_t event)
{
  /* Wiitigo	|3|G|G|G	0x2009cfb
   * Creature - Yeti 0/0
   * ~ enters the battlefield with six +1/+1 counters on it.
   * At the beginning of your upkeep, put a +1/+1 counter on ~ if it has blocked or been blocked since your last upkeep. Otherwise, remove a +1/+1 counter from it. */

  card_instance_t *instance;
  int blocker_index;
  card_instance_t *blocker;
  int opponent;

  instance = get_card_instance(player, card);
  upkeep_trigger_ability(player, card, event, player);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, 6);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    if (instance->targets[1].player != 0x42)
    {
      remove_counters(player, card, player, card, COUNTER_P1_P1, 1);
      return 0;
    }
    add_counters(player, card, player, card, COUNTER_P1_P1, 1);
    return 0;
  }
  if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (current_turn == player)
    {
      if (is_attacking(player, card) != 0)
      {
        opponent = 1 - player;
        blocker_index = active_cards_count[opponent] + -1;
        if (-1 < blocker_index)
        {
          do
          {
            blocker = in_play(opponent, blocker_index);
            if (((blocker != NULL) && (is_what(opponent, blocker_index, 2) != 0)) &&
                (blocker = get_card_instance(opponent, blocker_index),
                 (unsigned int)blocker->blocking == card))
            {
              instance->targets[1].player = 0x42;
              return 0;
            }
            blocker_index = blocker_index + -1;
          } while (blocker_index != -1);
        }
      }
    }
    else if (instance->blocking != 0xff)
    {
      instance->targets[1].player = 0x42;
    }
  }
  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_wings_of_aesthir(int player, int card, event_t event)
{
  /* Wings of Aesthir	|W|U	0x200d77f
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+0 and has flying and first strike. */
  return generic_aura(player, card, event, player, 1,0, KEYWORD_FIRST_STRIKE|KEYWORD_FLYING,0, 0,0,0);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_withering_wisps(int player, int card, event_t event)
{
  /* Withering Wisps	|1|B|B	0x200d851
   * Enchantment
   * At the beginning of the end step, if no creatures are on the battlefield, sacrifice ~.
   * |B: ~ deals 1 damage to each creature and each player. Activate this ability no more times each turn than the number of snow |H1Swamps you control. */

  int rval = pestilence_impl(player, card, event, 1, COLOR_BLACK);

  if (event == EVENT_CAN_ACTIVATE && rval)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  if (inst->info_slot == count_snow_landtype(player, get_hacked_subtype(player, card, SUBTYPE_SWAMP)))
		return 0;
	}

  if (event == EVENT_ACTIVATE && cancel != 1)
	{
	  card_instance_t* inst = get_card_instance(player, card);
	  inst->info_slot = MAX(1, inst->info_slot + 1);
	}

  if (event == EVENT_CLEANUP)
	get_card_instance(player, card)->info_slot = 0;

  return rval;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_woolly_mammuth(int player, int card, event_t event){
	/* Woolly Mammoths	|1|G|G	0x2002E7E
	 * Creature - Elephant 3/2
	 * ~ has trample as long as you control a snow land. */

	if( event == EVENT_ABILITIES && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) && control_a_snow_land(player, 0) ){
		event_result |= KEYWORD_TRAMPLE;
	}

	return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int chaos_moon_legacy(int player, int card, event_t event);

int card_woolly_spider(int player, int card, event_t event){
	/* Woolly Spider	|1|G|G	0x2002E83
	 * Creature - Spider 2/3
	 * Reach
	 * Whenever ~ blocks a creature with flying, ~ gets +0/+2 until end of turn. */

	if( current_turn != player && current_phase == PHASE_DECLARE_BLOCKERS && blocking(player, card, event) &&
		! is_humiliated(player, card, AL_TRIG) )
	{
		card_instance_t *instance = get_card_instance( player, card );
		if( get_abilities(1-player, instance->blocking, EVENT_ABILITIES, -1) & KEYWORD_FLYING ){
			pump_until_eot(player, card, player, card, 0, 2);
		}
	}
	return 0;
}

int card_word_of_blasting(int player, int card, event_t event)
{
  /* Word of Blasting	|1|R	0x200d83d
   * Instant
   * Destroy target Wall. It can't be regenerated. ~ deals damage equal to that Wall's converted mana cost to the Wall's controller. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_PERMANENT);
  td.required_subtype = SUBTYPE_WALL;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		{
		  card_instance_t* inst = get_card_instance(player, card);
		  int cmc = get_cmc(inst->targets[0].player, inst->targets[0].card);
		  kill_card(inst->targets[0].player, inst->targets[0].card, KILL_BURY);
		  damage_player(inst->targets[0].player, cmc, player, card);
		}
	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_WALL", 1, NULL);
}

int card_word_of_undoing(int player, int card, event_t event){
	/* Word of Undoing	|U	0x200bd76
	 * Instant
	 * Return target creature and all |Swhite Auras you own attached to it to their owners' hands. */

	if (!IS_GS_EVENT(player, card, event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL  ){
		if( valid_target(&td) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "");
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
			this_test.owner = player;

			manipulate_auras_enchanting_target(player, card, instance->targets[0].player, instance->targets[0].card, &this_test, ACT_BOUNCE);
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
}

int card_wrath_of_marit_lage(int player, int card, event_t event)
{
	/* Wrath of Marit Lage	|3|U|U	0x200ce4c
	 * Enchantment
	 * When ~ enters the battlefield, tap all |Sred creatures.
	 * |SRed creatures don't untap during their controllers' untap steps. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY )
	{
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		new_manipulate_all(player, card, ANYBODY, &test, ACT_TAP);
	}

	if( event == EVENT_UNTAP && in_play(player, card) && is_what(affected_card_controller, affected_card, TYPE_CREATURE) &&
		(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_RED)) )
		get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;

	return global_enchantment(player, card, event);
}

int card_zuran_enchanter(int player, int card, event_t event)
{
  /* Zuran Enchanter	|1|U	0x200d810
   * Creature - Human Wizard 1/1
   * |2|B, |T: Target player discards a card. Activate this ability only during your turn. */

  if (!IS_GAA_EVENT(event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;

  if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
	discard(get_card_instance(player, card)->targets[0].player, 0, player);

  return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_IN_YOUR_TURN, MANACOST_XB(2,1), 0, &td, "TARGET_PLAYER");
}

int card_zuran_orb(int player, int card, event_t event){
	/* Zuran Orb	|0	0x20057b4
	 * Artifact
	 * Sacrifice a land: You gain 2 life. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 2);
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, 0, MANACOST0, 0, NULL, NULL, TYPE_LAND, 0, 0);
}

int card_zurs_weirding(int player, int card, event_t event){
	/* Zur's Weirding	|3|U	0x2009d00
	 * Enchantment
	 * Players play with their hands revealed.
	 * If a player would draw a card, he or she reveals it instead. Then any other player may pay 2 life. If a player does, put that card into its owner's graveyard. Otherwise, that player draws a card. */

	if (event == EVENT_STATIC_EFFECTS){
		player_bits[0] |= PB_HAND_REVEALED;
		player_bits[1] |= PB_HAND_REVEALED;
	}

	if( trigger_condition == TRIGGER_REPLACE_CARD_DRAW && affect_me(player, card) && !suppress_draw ){
		if( event == EVENT_TRIGGER){
			event_result |= 2u;
		}
		else if( event == EVENT_RESOLVE_TRIGGER ){
				int *deck = deck_ptr[reason_for_trigger_controller];
				if( deck[0] > -1 ){

					int ai_choice = 1;
					if( get_base_value(-1, deck[0]) > 50 && life[1-reason_for_trigger_controller]-2 >= 6 ){
						ai_choice = 3;
					}

					int can_pay = can_pay_life(1-reason_for_trigger_controller, 2);
					int choice = DIALOG(player, card, EVENT_ACTIVATE,
										DLG_NO_STORAGE, DLG_NO_CANCEL,
										DLG_WHO_CHOOSES(1-reason_for_trigger_controller),
										DLG_FULLCARD_ID(deck[0]),
										DLG_SMALLCARD(player, card),
										"Pay 2 life", can_pay, ai_choice,
										"Allow draw", 1, 2);

					if( choice == 1 ){
						lose_life(1-reason_for_trigger_controller, 2);
						mill(reason_for_trigger_controller, 1);
						suppress_draw = 1;
					}
				}
		}
	}

	return global_enchantment(player, card, event);
}

#ifndef SHANDALAR
/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x23f08c0; original cards/ice_age.c:1020. */
int card_balduvian_shaman(int player, int card, event_t event)
{
  /* Balduvian Shaman	|U	0x000000
   * Creature - Human Cleric Shaman 1/1
   * |T: Change the text of target |Swhite enchantment you control that doesn't have cumulative upkeep by replacing all instances of one color word with another. That enchantment gains "Cumulative upkeep |1." */

  card_instance_t *instance;
  const char *prompt;
  int result;
  target_definition_t td;

  default_target_definition(player, card, &td, 4);
  td.required_color = get_sleighted_color_test(player, card, 0x20);
  td.preferred_controller = player;
  td.allowed_controller = player;
  td.extra = (int32_t)doesnt_have_cumulative_upkeep;
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = valid_target(&td);
    if (result != 0)
    {
      replace_all_instances_of_one_color_word_with_another(
          player, card, instance->targets[0].player, instance->targets[0].card);
      create_targetted_legacy_effect(instance->parent_controller, instance->parent_card,
                                     effect_basha, instance->targets[0].player,
                                     instance->targets[0].card);
    }
  }
  prompt = get_sleighted_color_text(
      player, card, "Select target %s enchantment you control that doesn\'t have cumulative upkeep",
      5);
  result = generic_activated_ability(player, card, event, 7, 0, 0, 0, 0, 0, 0, 0, &td,
                                          (char *)prompt);
  return result;
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 Oath life-loss callback. */

/* ManalinkEh 2022: 0x24040a0; original cards/ice_age.c:7248. */

#endif

#ifndef SHANDALAR
static void effect_breath_dream(int player, int card, event_t event, kill_t kill_mode)
{
  int c, result, turn_player;
  card_instance_t *instance;
  char marked[151];
  test_definition_t this_test;
  turn_player = current_turn;
  memset(marked, 0, sizeof(marked));
  c = 0;
  if (0 < active_cards_count[current_turn])
  {
    do
    {
      instance = in_play(turn_player, c);
      if ((instance != NULL) && (result = is_what(turn_player, c, 2), result != 0))
      {
        marked[c] = '\x01';
      }
      c = c + 1;
    } while (c < active_cards_count[turn_player]);
  }
  default_test_definition(&this_test, 2);
  c = 0;
  this_test.color = get_sleighted_color_test(player, card, 8);
  if (0 < active_cards_count[turn_player])
  {
    do
    {
      if ((((marked[c] != '\0') && (instance = in_play(turn_player, c), instance != NULL)) &&
           ((instance->token_status & 0x80) == 0)) &&
          ((result = new_make_test_in_play(turn_player, c, -1, &this_test),
            result != 0 && (result = cumulative_upkeep_arbitrary(player, card, turn_player, c,
                                                                 event, 1, 0, 0, 0, 0, 0),
                            result != 0))))
      {
        kill_card(turn_player, c, kill_mode);
      }
      c = c + 1;
    } while (c < active_cards_count[turn_player]);
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23fa6e0; original cards/ice_age.c:4194. */
int card_goblin_snowman(int player, int card, event_t event)
{
  /* Goblin Snowman	|3|R	0x000000
   * Creature - Goblin 1/1
   * Whenever ~ blocks, prevent all combat damage that would be dealt to and dealt by it this turn.
   * |T: ~ deals 1 damage to target creature it's blocking. */

  unsigned char blocked_card;
  card_instance_t *instance;
  int target_available;
  int opponent_or_result;
  target_definition_t td;
  opponent_or_result = 1 - player;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (current_turn == opponent_or_result)) &&
        (current_phase < 0x18))
    {
      opponent_or_result = get_power(player, card);
      check_abilities_power = check_abilities_power - opponent_or_result;
      check_abilities_toughness = check_abilities_toughness + 0x3fff;
    }
    default_target_definition(player, card, &td, 2);
    return 0;
  }
  if ((current_turn == opponent_or_result) && (event == EVENT_DECLARE_BLOCKERS))
  {
    opponent_or_result = blocking(player, card, EVENT_DECLARE_BLOCKERS);
    if ((opponent_or_result != 0) &&
        (opponent_or_result = is_humiliated(player, card, AL_TRIG), opponent_or_result == 0))
    {
      maze_of_ith_effect(player, card, player, card);
    }
    default_target_definition(player, card, &td, 2);
  }
  else
  {
    default_target_definition(player, card, &td, 2);
    if (event == EVENT_CAN_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      if (current_phase != 0x18)
      {
        return 0;
      }
      if (current_turn == player)
      {
        return 0;
      }
      if (instance->blocking == 0xff)
      {
        return 0;
      }
      if (would_validate_arbitrary_target(&td, opponent_or_result,
                                          (unsigned int)instance->blocking) == 0)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      if (current_phase != 0x18)
      {
        return 0;
      }
      if (current_turn == player)
      {
        return 0;
      }
      if (instance->blocking == 0xff)
      {
        return 0;
      }
      if (would_validate_arbitrary_target(&td, opponent_or_result,
                                          (unsigned int)instance->blocking) == 0)
      {
        return 0;
      }
      add_state(player, card, 0x10);
      target_available = charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0);
      if ((target_available != 0) &&
          (target_available =
               new_pick_target(&td, "Select target creature Goblin Snowman is blocking.", 0, 0x80),
           target_available != 0))
      {
        blocked_card = instance->blocking;
        instance->targets[0].player = opponent_or_result;
        instance->targets[0].card = (unsigned int)blocked_card;
        instance->info_slot = 1;
        instance->number_of_targets = '\x01';
      }
      if (spell_fizzled != 1)
      {
        return 0;
      }
      remove_state(player, card, 0x10);
      return 0;
    }
  }
  if (((event == EVENT_RESOLVE_ACTIVATION) &&
       (instance = get_card_instance(player, card), 0 < instance->info_slot)) &&
      (opponent_or_result = valid_target(&td), opponent_or_result != 0))
  {
    damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23ee460; original cards/ice_age.c:60. */

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23eb6c0; original cards/ice_age.c:4331. */
static char *target_is_nonsnow(int who_chooses, int player, int card, int targeting_player,
                        int targeting_card)
{
  int result;
  result = has_subtype(player, card, SUBTYPE_SNOW);
  return result ? (char *)gs_target_error_subtype : NULL;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23faff0; original cards/ice_age.c:4337. */
int card_hallowed_ground(int player, int card, event_t event)
{
  /* Hallowed Ground	|1|W	0x000000
   * Enchantment
   * |W|W: Return target nonsnow land you control to its owner's hand. */

  int result;
  target_definition_t td;
  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 1);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)target_is_nonsnow;
    td.allowed_controller = player;
    td.preferred_controller = player;
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    default_target_definition(player, card, &td, 1);
    td.preferred_controller = player;
    td.allowed_controller = player;
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (int32_t)target_is_nonsnow;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      action_on_target(player, card, 0, ACT_BOUNCE);
    }
  }
  result = generic_activated_ability(player, card, event, 6, 0, 0, 0, 0, 0, 2, 0, &td,
                                          "Select target non-snow land you control.");
  return result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23ec740; original cards/ice_age.c:5043. */
static int infinite_hourglass_legacy(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int activation_result;
  int time_counters;
  int source_card_or_count;
  int source_player_or_count;
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_card_or_count = get_id(p, instance->targets[0].card);
        if (source_card_or_count != 0x7c2)
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
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {
      get_card_instance(player, card);
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {
      instance = get_card_instance(player, card);
      if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
      {
        source_card_or_count = instance->targets[0].card;
        source_player_or_count = instance->targets[0].player;
        activation_result =
            granted_generic_activated_ability(source_player_or_count, source_card_or_count, player,
                                              card, event, 0x8000, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
        if (event == EVENT_ACTIVATE)
          return activation_result;
        if ((current_phase == 4) &&
            (time_counters =
                 count_counters(source_player_or_count, source_card_or_count, COUNTER_TIME),
             0 < time_counters))
        {
          if (activation_result == 0)
          {
            return 0;
          }
          if (ai_is_speculating != 1)
          {
            if (player != 1)
            {
              return activation_result;
            }
            if (((unsigned char)trace_mode & 2) != 0)
            {
              return activation_result;
            }
          }
          source_card_or_count =
              count_counters(source_player_or_count, source_card_or_count, COUNTER_TIME);
          if (source_card_or_count < 1)
          {
            return activation_result;
          }
          source_card_or_count = count_subtype(source_player_or_count, 2, -1);
          source_player_or_count = count_subtype(1 - player, 2, -1);
          if (source_player_or_count <= source_card_or_count + 1)
          {
            return activation_result;
          }
          g_activation_event_flags = g_activation_event_flags | 3;
          return activation_result;
        }
      }
      else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
      {
        source_card_or_count = instance->targets[0].card;
        source_player_or_count = instance->targets[0].player;
        activation_result =
            granted_generic_activated_ability(source_player_or_count, source_card_or_count, player,
                                              card, event, 0x8000, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
        if (event != EVENT_RESOLVE_ACTIVATION)
        {
          return activation_result;
        }
        remove_counters(source_player_or_count, source_card_or_count, source_player_or_count,
                        source_card_or_count, COUNTER_TIME, 1);
        return activation_result;
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x23fd2e0; original cards/ice_age.c:5108. */
int card_infinite_hourglass(int player, int card, event_t event)
{
  /* Infinite Hourglass	|4	0x000000
   * Artifact
   * At the beginning of your upkeep, put a time counter on ~.
   * All creatures get +1/+0 for each time counter on ~.
   * |3: Remove a time counter from ~. Any player may activate this ability but only during any upkeep step. */

  int p;
  int c;
  int activation_result;
  int count_or_created;
  int eligible_or_opponent_count;
  card_instance_t *instance;
  int found[2];

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    add_counters(player, card, player, card, COUNTER_TIME, 1);
    boost_creature_type(player, card, EVENT_UPKEEP_TRIGGER_ABILITY, ~SUBTYPE_INVALID,
                        count_counters(player, card, COUNTER_TIME), 0, 0, BCT_INCLUDE_SELF);
  }
  else
  {
    boost_creature_type(player, card, event, ~SUBTYPE_INVALID,
                        count_counters(player, card, COUNTER_TIME), 0, 0, BCT_INCLUDE_SELF);
    p = current_turn;
    if (event == EVENT_STATIC_EFFECTS)
    {
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      found[0] = -1;
      found[1] = -1;
      c = active_cards_count[current_turn] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(p, c);
          if ((((instance != NULL) && (eligible_or_opponent_count = is_what(p, c, 0x80),
                                       eligible_or_opponent_count != 0)) &&
               (instance = get_card_instance(p, c),
                instance->info_slot_fn == infinite_hourglass_legacy)) &&
              ((instance->targets[0].player == player && (instance->targets[0].card == card))))
          {
            found[p] = c;
          }
          c = c + -1;
        } while (c != -1);
      }
      p = 1 - current_turn;
      c = active_cards_count[p] + -1;
      if (-1 < c)
      {
        do
        {
          instance = in_play(p, c);
          if (((instance != NULL) && (eligible_or_opponent_count = is_what(p, c, 0x80),
                                      eligible_or_opponent_count != 0)) &&
              ((instance = get_card_instance(p, c),
                instance->info_slot_fn == infinite_hourglass_legacy &&
                    ((instance->targets[0].player == player &&
                      (instance->targets[0].card == card))))))
          {
            found[p] = c;
          }
          c = c + -1;
        } while (c != -1);
      }
      if (-1 < found[player])
      {
        kill_card(player, found[player], KILL_EXILE);
      }
      p = 1 - player;
      if (found[p] != -1)
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      count_or_created = add_card_to_hand(p, instance->internal_card_id);
      instance = get_card_instance(
          p, create_legacy_activate(p, count_or_created, infinite_hourglass_legacy));
      instance->targets[0].player = player;
      instance->targets[0].card = card;
      instance->number_of_targets = '\x01';
      obliterate_card(p, count_or_created);
      return 0;
    }
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      activation_result =
          generic_activated_ability(player, card, event, 0x8000, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (current_phase != 4)
        {
          return 0;
        }
        count_or_created = count_counters(player, card, COUNTER_TIME);
        if (count_or_created < 1)
        {
          return 0;
        }
        if (activation_result == 0)
        {
          return 0;
        }
        if (ai_is_speculating != 1)
        {
          if (player != 1)
          {
            return activation_result;
          }
          if (((unsigned char)trace_mode & 2) != 0)
          {
            return activation_result;
          }
        }
        count_or_created = count_counters(player, card, COUNTER_TIME);
        if (count_or_created < 1)
        {
          return activation_result;
        }
        count_or_created = count_subtype(player, 2, -1);
        eligible_or_opponent_count = count_subtype(1 - player, 2, -1);
        if (count_or_created + 1 < eligible_or_opponent_count)
        {
          g_activation_event_flags = g_activation_event_flags | 3;
          return activation_result;
        }
        return activation_result;
      }
      return activation_result;
    }
  }
  if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
  {
    return 0;
  }
  activation_result =
      generic_activated_ability(player, card, event, 0x8000, 3, 0, 0, 0, 0, 0, 0, NULL, NULL);
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    remove_counters(player, card, player, card, COUNTER_TIME, 1);
    return activation_result;
  }
  return activation_result;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2403af0; original cards/ice_age.c:7162. */
int card_norritt(int player, int card, event_t event)
{
  /* Norritt	|3|B	0x000000
   * Creature - Imp 1/1
   * |T: Untap target |Sblue creature.
   * |T: Choose target non-Wall creature the active player has controlled continuously since the beginning of the turn. That creature attacks this turn if able. If it doesn't, destroy it at the beginning of the next end step. Activate this ability only before attackers are declared. */

  enum
  {
    CHOICE_UNTAP = 1,
    CHOICE_MUST_ATTACK = 2
  };

  int choice;
  card_instance_t *instance;
  int value;
  const char *prompt;
  int available_or_opponent;
  target_definition_t *untap_target;
  target_definition_t td;
  target_definition_t td2;
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 2);
    td.allowed_controller = current_turn;
    td.required_subtype = 0xd1;
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
    td.illegal_state = TARGET_STATE_SUMMONING_SICK;
    default_target_definition(player, card, &td2, 2);
    td2.required_color = get_sleighted_color_test(player, card, 4);
    if (event == EVENT_CAN_ACTIVATE)
    {
      value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x10000005, 0, 0, 0, 0, 0,
                                        0, 0, &td, NULL);
      if ((value == 0) && (value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0,
                                                             0, 0, 0, 0, 0, 0, &td2, NULL),
                           value == 0))
      {
        return 0;
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      ai_modifier = ai_modifier + (unsigned int)((1 - player == current_turn) ? 0 : -48) + 0x18;
      value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0,
                                        &td2, NULL);
      available_or_opponent = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE,
                                                        0x10000005, 0, 0, 0, 0, 0, 0, 0, &td, NULL);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Untap target blue creature",
                           value, 5, "Target creature must attack", available_or_opponent,
                           ai_modifier, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      instance->info_slot = choice;
      if (choice != CHOICE_UNTAP)
      {
        if (choice != CHOICE_MUST_ATTACK)
        {
          return 0;
        }
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0x10000007, 0, 0, 0, 0, 0, 0, 0,
                                  &td,
                                  "Select target non-wall creature active player has, to attack.");
        return 0;
      }
      prompt = get_sleighted_color_text(player, card, "Select target %s creature.", 2);
      generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, 0, 0, 0, 0, 0, 0, &td2,
                                (char *)prompt);
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
    td.allowed_controller = current_turn;
    td.required_subtype = 0xd1;
    td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
    td.illegal_state = TARGET_STATE_SUMMONING_SICK;
    default_target_definition(player, card, &td2, 2);
    td2.required_color = get_sleighted_color_test(player, card, 4);
  }
  untap_target = &td2;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    value = instance->info_slot;
    if (value == CHOICE_UNTAP)
    {
      if (valid_target(untap_target) != 0)
      {
        untap_card(instance->targets[0].player, instance->targets[0].card);
      }
      value = instance->info_slot;
    }
    if ((value == CHOICE_MUST_ATTACK) && (value = valid_target(&td), value != 0))
    {
      value = get_original_internal_card_id(instance->parent_controller, instance->parent_card);
      available_or_opponent = 1 - player;
      value = add_card_to_hand(available_or_opponent, value);
      create_targetted_legacy_effect(available_or_opponent, value, effect_nettling_imp,
                                     instance->targets[0].player, instance->targets[0].card);
      obliterate_card(available_or_opponent, value);
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x24044c0; original cards/ice_age.c:7368. */
int card_orcish_healer(int player, int card, event_t event)
{
  /* Orcish Healer	|R|R	0x000000
   * Creature - Orc Cleric 1/1
   * |R|R, |T: Target creature can't be regenerated this turn.
   * |B|B|R, |T: Regenerate target |Sblack or |Sgreen creature.
   * |R|G|G, |T: Regenerate target |Sblack or |Sgreen creature. */

  enum
  {
    CHOICE_NO_REGEN = 1,
    CHOICE_BBR = 2,
    CHOICE_RGG = 3,
    CHOICE_SHIELD = 4
  };

  int choice;
  int value;
  card_instance_t *instance;
  int cannot_regenerate_available;
  int shield_available;
  const char *__format_1;
  unsigned int shield_mana_available;
  int green_regenerate_available;
  unsigned int cannot_regenerate_priority;
  unsigned int black_regenerate_available;
  int activation_flags;
  target_definition_t *regenerate_target;
  target_definition_t *shield_target;
  target_definition_t td;
  target_definition_t td2;
  target_definition_t td3;
  char msg[100];

  if (((event == EVENT_ACTIVATE) ||
       ((unsigned int)event - (unsigned int)EVENT_RESOLVE_ACTIVATION < 2U)) ||
      (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    value = 1 - player;
    td.preferred_controller = value;
    if (player == 1)
    {
      td.required_state = TARGET_STATE_DESTROYED;
      default_target_definition(1, card, &td2, 2);
      td2.required_color = get_sleighted_color_test(1, card, 10);
      td2.required_state = TARGET_STATE_DESTROYED;
      td2.preferred_controller = 1;
      td2.special = TARGET_SPECIAL_REGENERATION;
      default_target_definition(1, card, &td3, 2);
      td3.required_color = get_sleighted_color_test(1, card, 10);
      td3.preferred_controller = 1;
      instance = get_card_instance(1, card);
      activation_flags = 0x800007;
    }
    else
    {
      default_target_definition(player, card, &td2, 2);
      td2.required_color = get_sleighted_color_test(player, card, 10);
      td2.required_state = TARGET_STATE_DESTROYED;
      td2.preferred_controller = player;
      default_target_definition(player, card, &td3, 2);
      td3.required_color = get_sleighted_color_test(player, card, 10);
      td3.preferred_controller = player;
      instance = get_card_instance(player, card);
      activation_flags = 7;
    }
    shield_target = &td3;
    regenerate_target = &td2;
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (((land_can_be_played & 0x200U) != 0) &&
          ((value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 2, 0, 0, 1, 0,
                                              0, regenerate_target, NULL),
            value != 0 ||
                (value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 2,
                                                   1, 0, 0, regenerate_target, NULL),
                 value != 0))))
      {
        return 99;
      }
      value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0, 0, 0, 0,
                                        shield_target, NULL);
      if (((value != 0) &&
           ((value = has_mana_multi(player, 0, 2, 0, 0, 1, 0),
             value != 0 || (value = has_mana_multi(player, 0, 0, 0, 2, 1, 0), value != 0)))) ||
          (value = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, activation_flags, 0,
                                             0, 0, 0, 2, 0, 0, &td, NULL),
           value != 0))
      {
        return 1;
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      cannot_regenerate_available = generic_activated_ability(
          player, card, EVENT_CAN_ACTIVATE, activation_flags, 0, 0, 0, 0, 2, 0, 0, &td, NULL);
      green_regenerate_available = 0;
      black_regenerate_available = land_can_be_played & 0x200;
      if (black_regenerate_available != 0)
      {
        black_regenerate_available = generic_activated_ability(
            player, card, EVENT_CAN_ACTIVATE, 5, 0, 2, 0, 0, 1, 0, 0, regenerate_target, NULL);
        green_regenerate_available = generic_activated_ability(
            player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 2, 1, 0, 0, regenerate_target, NULL);
      }
      shield_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0, 0, 0,
                                                   0, 0, 0, shield_target, NULL);
      shield_mana_available = 0;
      if (shield_available != 0)
      {
        shield_available = has_mana_multi(player, 0, 2, 0, 0, 1, 0);
        shield_mana_available = 1;
        if (shield_available == 0)
        {
          shield_available = has_mana_multi(player, 0, 0, 0, 2, 1, 0);
          shield_mana_available = (unsigned int)(shield_available != 0);
        }
      }
      cannot_regenerate_priority = 0;
      if (value == instance->targets[0].player)
      {
        value = can_regenerate(value, instance->targets[0].card);
        cannot_regenerate_priority = (unsigned int)(value != 0);
      }
      choice = dialog_impl(
          player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Creature can't regenerate",
          cannot_regenerate_available, cannot_regenerate_priority, "Regenerate target creature",
          black_regenerate_available, 1, "Regenerate target creature", green_regenerate_available,
          1, "Regeneration shield", shield_mana_available, 1, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      __format_1 =
          get_sleighted_color_text2(player, card, "Select target %s or %s creature.", 1, 3);
      snprintf(msg, 100, "%s", __format_1);
      if (choice == CHOICE_NO_REGEN)
      {
        generic_activated_ability(player, card, EVENT_ACTIVATE, activation_flags, 0, 0, 0, 0, 2, 0,
                                  0, &td,
                                  "Select target creature. It can\'t be regenerated this turn.");
      }
      else
      {
        if (choice == CHOICE_BBR)
        {
          cannot_regenerate_available = 0;
          value = 2;
        }
        else
        {
          if (choice != CHOICE_RGG)
          {
            if (choice != CHOICE_SHIELD)
            {
              return 0;
            }
            if (has_mana_multi(player, 0, 2, 0, 0, 1, 0) == 0)
            {
              cannot_regenerate_available = 2;
              value = 0;
            }
            else
            {
              cannot_regenerate_available = 0;
              value = 2;
            }
            return generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, value, 0,
                                             cannot_regenerate_available, 1, 0, 0, shield_target,
                                             msg);
          }
          cannot_regenerate_available = 2;
          value = 0;
        }
        generic_activated_ability(player, card, EVENT_ACTIVATE, 7, 0, value, 0,
                                  cannot_regenerate_available, 1, 0, 0, regenerate_target, msg);
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      value = instance->info_slot;
      if (value == 1)
      {
        if (valid_target(&td) != 0)
        {
          alternate_legacy_text(
              1, player,
              cannot_regenerate_until_eot(instance->parent_controller, instance->parent_card,
                                          instance->targets[0].player, instance->targets[0].card));
        }
        value = instance->info_slot;
      }
      if ((value - 2U < 3) && ((value = valid_target(regenerate_target),
                                value != 0 || (value = valid_target(shield_target), value != 0))))
      {
        value = regenerate_or_shield(instance->parent_controller, instance->parent_card,
                                     instance->targets[0].player, instance->targets[0].card);
        alternate_legacy_text(2, instance->parent_controller, value);
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2409280; original cards/ice_age.c:8826. */
int card_soul_kiss(int player, int card, event_t event)
{
  /* Soul Kiss	|2|B	0x000000
   * Enchantment - Aura
   * Enchant creature
   * |B, Pay 1 life: Enchanted creature gets +2/+2 until end of turn. Activate this ability no more than three times each turn. */

  unsigned int used_activations;
  card_instance_t *instance;
  int available_pumps_or_priority;
  int mana_available;
  card_instance_t *attached;
  if (event == EVENT_SHOULD_AI_PLAY)
  {
    instance = get_card_instance(player, card);
    available_pumps_or_priority = 0;
    if (instance->damage_target_player == player)
    {
      available_pumps_or_priority =
          ((unsigned int)((player == 1) ? 12 : -12)) * landsofcolor_controlled[player][1];
    }
    ai_modifier = ai_modifier + available_pumps_or_priority;
    get_card_instance(player, card);
    instance = get_card_instance(player, card);
  }
  else
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CHECK_ABILITIES)
    {
      if ((((instance->damage_target_card == affected_card) &&
            (affected_card_controller == instance->damage_target_player)) &&
           (instance->damage_target_player == player)) &&
          (attached = in_play(player, card), attached != NULL))
      {
        used_activations = instance->targets[0x12].player;
        available_pumps_or_priority =
            3 - (used_activations & (int)(used_activations ^ 0xffffffff) >> 0x1f);
        if (life[player] + -1 < available_pumps_or_priority)
        {
          available_pumps_or_priority = life[player] + -1;
        }
        if (0 < available_pumps_or_priority)
        {
          mana_available = has_mana(player, 1, 1);
          if (mana_available < available_pumps_or_priority)
          {
            available_pumps_or_priority = mana_available;
          }
          check_abilities_power = check_abilities_power + available_pumps_or_priority * 2;
          check_abilities_toughness = check_abilities_toughness + available_pumps_or_priority * 2;
        }
      }
      get_card_instance(player, card);
    }
    if (event != EVENT_CHECK_ABILITIES)
      instance = get_card_instance(player, card);
  }
  if (event != EVENT_CHECK_ABILITIES && instance->damage_target_player >= 0)
  {
    if (event == EVENT_CAN_ACTIVATE)
    {
      instance = get_card_instance(player, card);
      if (instance->targets[0x12].player == 3)
      {
        return 0;
      }
      return generic_activated_ability(player, card, event, 0, 0, 1, 0, 0, 0, 0, 1, NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 1, 0, 0, 0, 0, 1, NULL, NULL);
      if (spell_fizzled == 1)
      {
        return 0;
      }
      instance = get_card_instance(player, card);
      instance->targets[0x12].player = instance->targets[0x12].player + 1;
      return 0;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      instance = get_card_instance(player, card);
      attached = in_play((int)instance->damage_target_player, instance->damage_target_card);
      if (attached != NULL)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      (int)instance->damage_target_player,
                                      instance->damage_target_card, 2, 2);
      }
      return generic_activated_ability(player, card, event, 0, 0, 1, 0, 0, 0, 0, 1, NULL, NULL);
    }
    if (event == EVENT_CLEANUP)
    {
      instance = get_card_instance(player, card);
      instance->targets[0x12].player = 0;
      return generic_activated_ability(player, card, event, 0, 0, 1, 0, 0, 0, 0, 1, NULL, NULL);
    }
  }

  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA &&
      (unsigned int)event - (unsigned int)EVENT_POWER > 2U && event != EVENT_RESOLVE_MOVING_AURA &&
      event != EVENT_CHANGE_TYPE && event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD &&
      event != EVENT_STATIC_EFFECTS)
    return 0;
  return vanilla_aura(player, card, event, player);
}

#endif

#ifndef SHANDALAR
/* Recovered October 2022 private inline definitions. See per-helper limits in the recovery evidence. */
/* ManalinkEh 2022: 0x2409690; original cards/ice_age.c:8890. */
int card_spectral_shield(int player, int card, event_t event)
{
  /* Spectral Shield	|1|W|U	0x000000
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +0/+2 and can't be the target of spells. */

  card_instance_t *instance;
  int result;

  attached_gets_sp_kw2(player, card, event, SP_KEYWORD2_GRANTED_PROT_MISC);
  instance = get_card_instance(player, card);
  if ((((event == EVENT_PROT_MISC) && (instance->damage_target_card == affected_card)) &&
       (instance->damage_target_player == affected_card_controller)) &&
      (((unsigned int)event_result >> 24 & 0xff) == '\x05'))
  {
    result = is_being_cast(attacking_card_controller, attacking_card);
    if (result != 0)
    {
      event_result = event_result | 1;
    }
  }
  attached_gets_pt(player, card, event, 0, 2);
  result = aura(player, card, event, NULL, NULL);
  return result;
}
#endif
