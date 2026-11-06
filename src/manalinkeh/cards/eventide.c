#include "manalink.h"
static int fod_1(int,int,event_t);
static int fod_2(int,int,event_t);
static int fod_3(int,int,event_t);

/* Canonical recovered helper declarations. */
int retrace(int player, int card, event_t event, int id);
#include <math.h>

// Functions
static int skulkin(int player, int card, event_t event, int cless, int target_color, int pwr, int tou, int key, int s_key){

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_color = get_sleighted_color_test(player, card, target_color);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, pwr, tou, key, s_key);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(cless), 0, &td, "TARGET_CREATURE");
}

static int mimic(int player, int card, event_t event, int c1, int c2, int pwr, int tou, int key, int s_key){

	if( trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( trigger_cause_controller == player && ! is_what(trigger_cause_controller, trigger_cause, TYPE_LAND) ){
			int clr = get_sleighted_color_test(player, card, c1) | get_sleighted_color_test(player, card, c2);
			int result = get_color(trigger_cause_controller, trigger_cause) & clr;
			if( result == clr && new_specific_spell_played(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, NULL) ){
				set_pt_and_abilities_until_eot(player, card, player, card, pwr, tou, key, s_key, 0);
			}
		}
	}

	return hybrid(player, card, event);
}

static int hatchling(int player, int card, event_t event, int c1, int c2)
{
  int result;
  unsigned int first_color;
  unsigned int second_color;
  unsigned int first_spell_color;
  unsigned int second_spell_color;
  test_definition_t test;
  enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 4);
  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {
      result = count_counters(player, card, COUNTER_M1_M1);
      if ((result != 0) && (trigger_cause_controller == player))
      {
        result = is_what(player, trigger_cause, 1);
        if (result == 0)
        {
          first_color = get_sleighted_color_test(player, card, c1);
          second_color = get_sleighted_color_test(player, card, c2);
          default_test_definition(&test, 0xff);
          test.color = first_color | second_color;
          sp_ability_trigger(player, event, 2, TYPE_NONE, &test, player, card);
        }
      }
    }
  }
  if (event == EVENT_SPA)
  {
    first_color = get_sleighted_color_test(player, card, c1);
    second_color = get_sleighted_color_test(player, card, c2);
    first_spell_color = get_color(trigger_cause_controller, trigger_cause);
    second_spell_color = get_color(trigger_cause_controller, trigger_cause);
    remove_counters(player, card, player, card, COUNTER_M1_M1,
                    (unsigned int)((second_color & second_spell_color) != 0) +
                        (unsigned int)((first_color & first_spell_color) != 0));
  }
  return hybrid(player, card, event);
}

int retrace(int player, int card, event_t event, int id)
{
  int result;
  card_instance_t *instance;
  test_definition_t this_test;
  if (event == EVENT_GRAVEYARD_ABILITY)
  {
    result = has_mana_to_cast_id(player, EVENT_GRAVEYARD_ABILITY, id);
    if (result != 0)
    {
      default_test_definition(&this_test, 1);
      this_test.zone = 0x100;
      result = check_battlefield_for_special_card(player, card, player, 0, &this_test);
      if (result != 0)
      {
        instance = get_card_instance(player, card);
        result =
            can_legally_play_iid_now(player, instance->internal_card_id, EVENT_GRAVEYARD_ABILITY);
        if (result != 0)
        {
          return 2;
        }
      }
    }
  }
  else if ((event == EVENT_PAY_FLASHBACK_COSTS) &&
           (charge_mana_from_id(player, -1, EVENT_PAY_FLASHBACK_COSTS, id), spell_fizzled != 1))
  {
    new_default_test_definition(&this_test, 1, "Select a land card to discard.");
    result = new_select_a_card(player, player, 4, 0, 3, -1, &this_test);
    if (result == -1)
    {
      spell_fizzled = 1;
      return 0;
    }
    discard_card(player, result);
    return 1;
  }
  return 0;
}

static int count_chroma(int player, int card, int clr1, int clr2, int must_be_permanent)
{
  card_data_t* cd;
  if (player != -1)
	{
	  /* Rule 202.1b Some objects have no mana cost. This normally includes all land cards, any other cards that have no mana symbols where their mana cost
	   * would appear, tokens (unless the effect that creates them specifies otherwise), and nontraditional _Magic_ cards. Having no mana cost represents an
	   * unpayable cost (see rule 117.6). Note that lands are played without paying any costs (see rule 305, "Lands"). */
	  if (is_token(player, card))
		return 0;

	  cd = get_card_data(player, card);
	}
  else
	cd = &cards_data[card];

  // Rules 202.1b again.  Lands are very common, so optimize for them.
  if (cd->type & TYPE_LAND)
	return 0;

  // Permanents only - in Manalink, instants and sorceries and such are on the battlefield while they resolve.
  if (!(cd->type & TYPE_PERMANENT) && must_be_permanent)
	return 0;

  card_ptr_t* cp = cards_ptr[cd->id];

  int result = 0;

  if (clr1 < COLOR_BLACK || clr1 > COLOR_WHITE)
	clr1 = 0;

  if (clr2 < COLOR_BLACK || clr2 > COLOR_WHITE)
	clr2 = 0;

  /* Checking card_ptr_t::req_black etc. and cards_data[].color can't easily distinguish between multicolor cards and hybrid cards when looking for more than
   * one color, even before considering the really weird cases like Bant Sureblade.  We can reliably tell that a card is not hybrid by checking if it has
   * "Modifies Mana Cost" set (it's stored as card_data_t::new_field & 1), but other things set that too.  We can't even reliably tell whether a card is
   * multicolored just from looking at card_data_t::color; consider, for example, Transguild Courier. */

  if (!(cd->new_field & 1))
	{
	  // req_black through req_white are correct and complete, so this is quick and easy way.

	  int clrs = (1<<clr1) | (1<<clr2);

	  if (clrs & COLOR_TEST_BLACK)
		result += cp->req.req_black;

	  if (clrs & COLOR_TEST_BLUE)
		result += cp->req.req_blue;

	  if (clrs & COLOR_TEST_GREEN)
		result += cp->req.req_green;

	  if (clrs & COLOR_TEST_RED)
		result += cp->req.req_red;

	  if (clrs & COLOR_TEST_WHITE)
		result += cp->req.req_white;

	  return result;
	}

  /* Currently-appearing mana symbols in mana costs are:
   *
   * |B |U |G |R |W - normal.
   * |0 through |16 - generic.
   * |PB |PU |PG |PR |PW - phyrexian.
   * |BR |BG |RG |RW |GW |GU |WU |WB |UB |UR - hybrid.
   * |2B |2U |2G |2R |2W - monocolor hybrid.
   * |/ - split card.  Doesn't appear on permanents.
   *
   * So we can count by looking for the corresponding letters B U G R W.
   *
   * Since hybrid mana symbols are only counted once for two-color devotion even if both halves match, we skip the character after a match - it's always either
   * a '|' for the start of a new mana symbol, or the second half of a hybrid mana symbol. */

  static const char* clrs = "\tBUGRW";

  const char* p;
  for (p = cp->mana_cost_text; *p; ++p)
	if (*p == clrs[clr1] || *p == clrs[clr2])
	  {
		++result;		// Matched
		++p;			// Skip the next character
		if (*p == 0)	// Original match was the last character in the string
		  break;
	  }

  return result;
}

//This count only colored mana symbols for permanents, for other uses, go fo "count_chroma".
int chroma(int player, int card, int clr1, int clr2){
	int result = 0;
	if( card != -1 ){
		result = count_chroma(player, card, clr1, clr2, 1);
	}
	else{
		int i;
		for(i=0; i<2; i++){
			if(i==player || player == 2){
				int count = 0;
				while( count < active_cards_count[i] ){
						if( in_play(i, count) && !is_what(i, count, TYPE_EFFECT) ){
							result+=count_chroma(i, count, clr1, clr2, 1);
						}
						count++;
				}
			}
		}
	}
	return result;
}

static void get_mana_from_text(test_definition_t *test, int cless, int black, int blue, int green,
                               int red, int white)
{
  char buffer[100];
  int pos = scnprintf(buffer, 100, "Pay ");
  if (cless)
  {
    pos += scnprintf(buffer + pos, 100 - pos, "%d", cless);
  }
  if (black)
  {
    pos += scnprintf(buffer + pos, 100 - pos, "B");
  }
  if (blue)
  {
    pos += scnprintf(buffer + pos, 100 - pos, "U");
  }
  if (green)
  {
    pos += scnprintf(buffer + pos, 100 - pos, "G");
  }
  if (red)
  {
    pos += scnprintf(buffer + pos, 100 - pos, "R");
  }
  if (white)
  {
    pos += scnprintf(buffer + pos, 100 - pos, "W");
  }
  strcpy(test->message, buffer);
}

static int liege_trigger(int player, int card, event_t event, int trig_mode, int c1, int c2)
{
  if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card) &&
      !is_humiliated(player, card, AL_TRIG) && trigger_cause_controller == player &&
      !is_what(trigger_cause_controller, trigger_cause, TYPE_LAND))
  {
    int c1s = get_sleighted_color_test(player, card, c1);
    int c2s = get_sleighted_color_test(player, card, c2);
    test_definition_t test;
    default_test_definition(&test, TYPE_ANY);
    test.color = c1s | c2s;
    sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
  }
  return 0;
}

// Cards
int card_antler_skulkin(int player, int card, event_t event){
/* CARD_ID_ANTLER_SKULKIN	9733
Antler Skulkin	|5
Artifact Creature - Scarecrow 3/3
|2: Target |Swhite creature gains persist until end of turn.
(When it dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &persist_granted,
											instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST_X(2), 0, &td, "Select target white creature.");
}

int card_archon_of_justice(int player, int card, event_t event){
/* CARD_ID_ARCHON_OF_JUSTICE	9734
Archon of Justice	|3|W|W
Creature - Archon 4/4
Flying
When ~ dies, exile target permanent. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	return 0;
}

int card_ashling_the_extinguisher(int player, int card, event_t event){
/* CARD_ID_ASHLING_THE_EXTINGUISHER	9735
Ashling, the Extinguisher	|2|B|B
Legendary Creature - Elemental Shaman 4/4
Whenever ~ deals combat damage to a player, choose target creature that player controls. He or she sacrifices that creature. */

	check_legend_rule(player, card, event);

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT | DDBM_MUST_BE_COMBAT_DAMAGE)){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );
		td.allowed_controller = 1-player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && can_sacrifice(player, 1-player, 1, TYPE_CREATURE, 0) ){
			if( pick_target(&td, "TARGET_CREATURE") ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_SACRIFICE);
			}
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_balefire_liege(int player, int card, event_t event)
{
  /* Balefire Liege	|2|RW|RW|RW	0x2002389
   * Creature - Spirit Horror 2/4
   * Other |Sred creatures you control get +1/+1.
   * Other |Swhite creatures you control get +1/+1.
   * Whenever you cast a |Sred spell, ~ deals 3 damage to target player or planeswalker.
   * Whenever you cast a |Swhite spell, you gain 3 life. */
  liege_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, COLOR_TEST_RED, COLOR_TEST_WHITE);
  if (event == EVENT_SPA)
  {
    int result = get_color(trigger_cause_controller, trigger_cause);
    if (result & get_sleighted_color_test(player, card, COLOR_TEST_WHITE))
    {
      gain_life(player, 3);
    }
    if (result & get_sleighted_color_test(player, card, COLOR_TEST_RED))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
      td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
      td.allow_cancel = 0;

      if (can_target(&td) && pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER"))
      {
        damage_target0(player, card, 3);
      }
      get_card_instance(player, card)->number_of_targets = 0;
    }
  }

  return liege(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_RED);
}

int card_battlegate_mimic(int player, int card, event_t event){
/* CARD_ID_BATTLEGATE_MIMIC	9739
Battlegate Mimic	|1|RW
Creature - Shapeshifter 2/1
Whenever you cast a spell that's both |Sred and |Swhite, ~ has base power and toughness 4/2 until end of turn and gains first strike until end of turn. */

	return mimic(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_RED, 4, 2, KEYWORD_FIRST_STRIKE, 0);
}

int card_batwing_brume(int player, int card, event_t event){
/* CARD_ID_BATWING_BRUME	9740
Batwing Brume	|1|WB
Instant
Prevent all combat damage that would be dealt this turn if |W was spent to cast ~.
Each player loses 1 life for each attacking creature he or she controls if |B was spent to cast ~. (Do both if |W|B was spent.) */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance(player, card);
		if( has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_WHITE, 1) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int c1 = get_updated_casting_cost(player, card, -1, event, 1);
			test_definition_t test;
			char mana_text[3][100];
			get_mana_from_text(&test, MANACOST_XW(c1, 1));
			strcpy(mana_text[0], test.message);
			get_mana_from_text(&test, MANACOST_XB(c1, 1));
			strcpy(mana_text[1], test.message);
			int can_play_full_effect = 0;
			if( c1 > 0 ){
				if( c1 > 1 ){
					get_mana_from_text(&test, MANACOST_XBW(c1-1, 1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_XBW(c1-1, 1, 1));
				}
				else{
					get_mana_from_text(&test, MANACOST_BW(1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_BW(1, 1));
				}
			}
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							mana_text[0], has_mana_multi(player, MANACOST_XW(c1, 1)), 5,
							mana_text[1], has_mana_multi(player, MANACOST_XB(c1, 1)), 5,
							mana_text[2], can_play_full_effect, 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			charge_mana_multi(player, MANACOST_XBW(
													(c1 > 1 ? (choice != 3 ? c1 : c1-1) : (choice != 3 ? c1 : 0)),
													(choice == 2 || choice == 3),
													(choice == 1 || choice == 3)
													)
								);
		}
		if( spell_fizzled != 1 ){
			instance->info_slot |= 2* (mana_paid[COLOR_WHITE] > 0);
			instance->info_slot |= 4* (mana_paid[COLOR_BLACK] > 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);
		if( (instance->info_slot & 2) ){
			fog_effect(player, card);
		}
		if( (instance->info_slot & 4) ){
			lose_life(current_turn, count_attackers(current_turn));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_beckon_apparition(int player, int card, event_t event){
	/* Beckon Apparition	|WB
	 * Instant
	 * Exile target card from a graveyard. Put a 1/1 |Swhite and |Sblack Spirit creature token with flying onto the battlefield. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &test);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ANY, "Select a card to exile.");
			return generic_spell(player, card, event, GS_GRAVE_RECYCLER_BOTH_GRAVES, NULL, NULL, 1, &this_test);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, instance->targets[0].player, 1);
		if( selected != -1 ){
			rfg_card_from_grave(instance->targets[0].player, selected);

			token_generation_t token;
			default_token_definition(player, card, CARD_ID_SPIRIT, &token);
			token.color_forced = (COLOR_TEST_WHITE | COLOR_TEST_BLACK);
			token.key_plus = KEYWORD_FLYING;
			generate_token(&token);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_belligerent_hatchling(int player, int card, event_t event){
	/* Belligerent Hatchling	|3|RW
	 * Creature - Elemental 6/6
	 * First strike
	 * ~ enters the battlefield with four -1/-1 counters on it.
	 * Whenever you cast a |Sred spell, remove a -1/-1 counter from ~.
	 * Whenever you cast a |Swhite spell, remove a -1/-1 counter from ~. */
	return hatchling(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_RED);
}

int card_bloom_tender(int player, int card, event_t event){
/* CARD_ID_BLOOM_TENDER	9744
Bloom Tender	|1|G
Creature - Elf Druid 1/1
|T: For each color among permanents you control, add one mana of that color to your mana pool. */

	int cols[COLOR_WHITE + 1] = {0};

	if( (event == EVENT_COUNT_MANA && affect_me(player, card)) || event == EVENT_ACTIVATE || (event == EVENT_CAN_ACTIVATE && player == AI)){
		get_card_instance(player, card)->info_slot = 0;
		// Only count colored permanents if they'll actually be needed
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		// Works properly for (possibly laced) lands, despite them using card_data_t::color, due to a special case in get_abilities()
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;

		int i;
		for (i = COLOR_BLACK; i <= COLOR_WHITE; ++i){
			td.required_color = 1 << i;
			if( can_target(&td) ){
				cols[i] = 1;
				get_card_instance(player, card)->info_slot |= 1<<i;
			}
		}
	}

	return mana_producing_creature_multi(player, card, event, 24, 0, cols[COLOR_BLACK], cols[COLOR_BLUE], cols[COLOR_GREEN], cols[COLOR_RED], cols[COLOR_WHITE]);
}

int card_cache_riders(int player, int card, event_t event){
/* CARD_ID_CACHE_RAIDERS	9745
Cache Raiders	|3|U|U
Creature - Merfolk Rogue 4/4
At the beginning of your upkeep, return a permanent you control to its owner's hand. */

	if( current_turn == player && trigger_condition == TRIGGER_UPKEEP ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		td.allow_cancel = 0;
		return bounce_permanent_at_upkeep(player, card, event, &td);
	}
	return 0;
}

int card_card_call_the_skybreaker(int player, int card, event_t event){
	/* Call the Skybreaker	|5|UR|UR
	 * Sorcery
	 * Put a 5/5 |Sblue and |Sred Elemental creature token with flying onto the battlefield.
	 * Retrace */

	hybrid(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY && has_mana_hybrid(player, 2, COLOR_BLUE, COLOR_RED, 5) ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.zone = TARGET_ZONE_HAND;
		if( check_battlefield_for_special_card(player, card, player, 0, &this_test) && can_sorcery_be_played(player, event) ){
			return GA_PLAYABLE_FROM_GRAVE;
		}
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		charge_mana_hybrid(player, card, 2, COLOR_BLUE, COLOR_RED, 5);
		if( spell_fizzled != 1 ){
			char msg[100] = "Select a land card to discard.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, msg);
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				discard_card(player, selected);
				return GAPAID_REMOVE;
			}
			else{
				spell_fizzled = 1;
			}
		}
	}
	if( event == EVENT_CAN_CAST){
		return 1;
	}
	if(event == EVENT_RESOLVE_SPELL){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		token.pow = 5;
		token.tou = 5;
		token.color_forced = COLOR_TEST_BLUE | COLOR_TEST_RED;
		token.key_plus = KEYWORD_FLYING;
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_canker_abomination(int player, int card, event_t event){
/* CARD_ID_CANKER_ABOMINATION	9747
Canker Abomination	|2|BG|BG
Creature - Treefolk Horror 6/6
As ~ enters the battlefield, choose an opponent. ~ enters the battlefield with a -1/-1 counter on it for each creature that player controls. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, count_subtype(1-player, TYPE_CREATURE, -1));
	return hybrid(player, card, event);
}

int card_cankerous_thirst(int player, int card, event_t event){
  /* Cankerous Thirst |3|{B/G}
   * Instant
   * If {B} was spent to cast Cankerous Thirst, you may have target creature get -3/-3 until end of turn.
   * If {G} was spent to cast Cankerous Thirst, you may have target creature get +3/+3 until end of turn. (Do both if {B}{G} was spent.) */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance( player, card );
		int c1 = get_updated_casting_cost(player, card, -1, event, 3);
		if( has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_GREEN, c1) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_M3_M3 = 1,
		CHOICE_P3_P3
	};

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = instance->info_slot = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int c1 = get_updated_casting_cost(player, card, -1, event, 3);
			test_definition_t test;
			char mana_text[3][100];
			get_mana_from_text(&test, MANACOST_XB(c1, 1));
			strcpy(mana_text[0], test.message);
			get_mana_from_text(&test, MANACOST_XG(c1, 1));
			strcpy(mana_text[1], test.message);
			int can_play_full_effect = 0;
			if( c1 > 0 ){
				if( c1 > 1 ){
					get_mana_from_text(&test, MANACOST_XBG(c1-1, 1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_XBG(c1-1, 1, 1));
				}
				else{
					get_mana_from_text(&test, MANACOST_BG(1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_BG(1, 1));
				}
			}
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							mana_text[0], has_mana_multi(player, MANACOST_XB(c1, 1)), 5*can_target(&td),
							mana_text[1], has_mana_multi(player, MANACOST_XG(c1, 1)), 5*can_target(&td1),
							mana_text[2], can_play_full_effect, 10*(can_target(&td) && can_target(&td1)));
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			charge_mana_multi(player, MANACOST_XBG(
													(c1 > 1 ? (choice != 3 ? c1 : c1-1) : (choice != 3 ? c1 : 0)),
													(choice == 1 || choice == 3),
													(choice == 2 || choice == 3)
													)
								);
		}
		if( spell_fizzled != 1 ){
			if( mana_paid[COLOR_BLACK] > 0 && new_pick_target(&td, "Select target creature for the -3/-3.", -1, GS_LITERAL_PROMPT) ){
				instance->info_slot |= CHOICE_M3_M3;
			}
			if( mana_paid[COLOR_GREEN] > 0 && new_pick_target(&td1, "Select target creature for the +3/+3.", -1, GS_LITERAL_PROMPT) ){
				instance->info_slot |= CHOICE_P3_P3;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int result = instance->info_slot & (CHOICE_P3_P3 | CHOICE_M3_M3);
		if( result == (CHOICE_P3_P3 | CHOICE_M3_M3) ){
			if( validate_target(player, card, &td, 0) ){
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -3, -3);
			}
			if( validate_target(player, card, &td, 1) ){
				pump_until_eot(player, card, instance->targets[1].player, instance->targets[1].card, 3, 3);
			}
		}
		else{
			if( validate_target(player, card, &td, 0) ){
				if( result == CHOICE_M3_M3 ){
					pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, -3, -3);
				}
				if( result == CHOICE_P3_P3 ){
					pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 3, 3);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cascade_bluffs(int player, int card, event_t event){
/* CARD_ID_CASCADE_BLUFFS	9749
Cascade Bluffs	""
Land
|T: Add |C to your mana pool.
|UR, |T: Add |U|U, |U|R, or |R|R to your mana pool.*/

	return card_filter_land(player, card, event, COLOR_BLUE, COLOR_RED, COLOR_TEST_BLUE, COLOR_TEST_RED, " Colorless\n U -> UU\n U -> UR\n U -> RR\n R -> UU\n R -> UR\n R -> RR\n Cancel");
}

int card_cauldron_haze(int player, int card, event_t event){
  /* Cauldron Haze |1|{W/B}
   * Instant
   * Choose any number of target creatures. Each of those creatures gains persist until end of turn.
   * 	(When it dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

	modify_cost_for_hybrid_spells(player, card, event, 0);

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
		int total = instance->number_of_targets = 0;
		if( hybrid_casting(player, card, 0) ){
			while( can_target(&td) ){
					if( select_target(player, card, &td, "Select target creature.", &(instance->targets[total])) ){
						state_untargettable(instance->targets[total].player, instance->targets[total].card, 1);
						total++;
					}
					else{
						break;
					}
			}
			int i;
			for(i=0; i<instance->number_of_targets; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
			}
			if( instance->number_of_targets < 1 ){
				spell_fizzled = 1;
			}
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<instance->number_of_targets; i++){
			if( validate_target(player, card, &td, i) ){
				create_targetted_legacy_effect(player, card, &persist_granted, instance->targets[i].player, instance->targets[i].card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_cenns_enlistment(int player, int card, event_t event){
	/* Cenn's Enlistment	|3|W
	 * Sorcery
	 * Put two 1/1 |Swhite Kithkin Soldier creature tokens onto the battlefield.
	 * Retrace */

	if( event == EVENT_CAN_CAST){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_RESOLVE_SPELL){
		generate_tokens_by_id(player, card, CARD_ID_KITHKIN_SOLDIER, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return retrace(player, card, event, CARD_ID_CENNS_ENLISTMENT);
}

int card_clout_of_the_dominus(int player, int card, event_t event){
/* CARD_ID_CLOUT_OF_THE_DOMINUS	9754
Clout of the Dominus	|UR
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Sblue, it gets +1/+1 and has shroud. (It can't be the target of spells or abilities.)
As long as enchanted creature is |Sred, it gets +1/+1 and has haste. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_RED, 1, 1, 0, SP_KEYWORD_HASTE);
	aura_ability_for_color(player, card, event, COLOR_TEST_BLUE, 1, 1, KEYWORD_SHROUD, 0);
	return generic_aura(player, card, event, player, 0, 0, 0, 0, 0, 0, 0);
}

int card_cold_eyed_selkie(int player, int card, event_t event){
/* CARD_ID_COLD_EYED_SELKIE	9755
Cold-Eyed Selkie	|1|GU|GU
Creature - Merfolk Rogue 1/1
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
Whenever ~ deals combat damage to a player, you may draw that many cards. */

	hybrid(player, card, event);

	if( damage_dealt_by_me(player, card, event, DDBM_REPORT_DAMAGE_DEALT|DDBM_MUST_DAMAGE_OPPONENT|DDBM_MUST_BE_COMBAT_DAMAGE|
							DDBM_TRIGGER_OPTIONAL) )
	{
		card_instance_t *instance = get_card_instance( player, card );
		draw_cards(player, instance->targets[16].player);
		instance->targets[16].player = 0;
	}
	return 0;
}

int card_crackleburr(int player, int card, event_t event){
	/* Crackleburr	|1|UR|UR	0x2007f41
	 * Creature - Elemental 2/2
	 * |UR|UR, |T, Tap two untapped |Sred creatures you control: ~ deals 3 damage to any target.
	 * |UR|UR, |Q, Untap two tapped |Sblue creatures you control: Return target creature to its owner's hand. */
	if(IS_GAA_EVENT(event)){

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.allowed_controller = player;
		td2.preferred_controller = player;
		td2.illegal_abilities = 0;
		td2.required_color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		td2.illegal_state = TARGET_STATE_TAPPED;
        td2.special = TARGET_SPECIAL_NOT_ME;

		target_definition_t td3;
		default_target_definition(player, card, &td3, TYPE_CREATURE);
		td3.allowed_controller = player;
		td3.preferred_controller = player;
		td3.illegal_abilities = 0;
		td3.required_color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		td3.required_state = TARGET_STATE_TAPPED;
        td3.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);

		enum{
			CHOICE_DAMAGE = 1,
			CHOICE_BOUNCE
		};

		if( event == EVENT_CAN_ACTIVATE ){
			int result = 0;
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST0);
			if( has_mana_hybrid(player, 2, COLOR_BLUE, COLOR_RED, c1) ){
				if( ! is_tapped(player, card) ){
					int req_targets = 2;
					if( target_available(player, card, &td2) >= req_targets ){
						result = generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, 0, &td, NULL);
					}
				}
				else{
					int req_targets = 2;
					if( target_available(player, card, &td3) >= req_targets ){
						result |= generic_activated_ability(player, card, event, GAA_TAPPED, MANACOST0, 0, &td1, NULL);
					}
				}
			}
			return result;
		}

		if(event == EVENT_ACTIVATE ){
			instance->info_slot = instance->number_of_targets = 0;
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST0);
			if( charge_mana_hybrid(player, card, 2, COLOR_BLUE, COLOR_RED, c1)){
				int choice = CHOICE_DAMAGE;
				if( is_tapped(player, card) ){
					int req_targets = 2;
					if( target_available(player, card, &td3) >= req_targets ){
						choice = CHOICE_BOUNCE;
					}
				}
				if( choice == CHOICE_DAMAGE ){
					int tapped[2];
					int tc = 0;
					while( tc < 2 ){
							if( new_pick_target(&td2,
												get_sleighted_color_text(player, card, "Select an untapped %s creature.", COLOR_RED),
												0, GS_LITERAL_PROMPT) )
							{
								tapped[tc] = instance->targets[0].card;
                                state_untargettable(player, tapped[tc], 1);
                                ++tc;
								instance->number_of_targets = 0;
							}
							else{
								break;
							}
					}
					int i;
					for(i=0; i<tc; i++){
						state_untargettable(player, tapped[i], 0);
					}
					if( tc < 2 ){
						spell_fizzled = 1;
						return 0;
					}
					if( pick_target(&td, "TARGET_ANY") ){
						for(i=0; i<2; i++){
							tap_card(player, tapped[i]);
						}
					}
				}
				if( choice == CHOICE_BOUNCE ){
					int tapped[2];
					int tc = 0;
					while( tc < 2 ){
							if( new_pick_target(&td3,
												get_sleighted_color_text(player, card, "Select a tapped %s creature.", COLOR_BLUE),
												0, GS_LITERAL_PROMPT) )
							{
								tapped[tc] = instance->targets[0].card;
                                state_untargettable(player, tapped[tc], 1);
                                ++tc;
								instance->number_of_targets = 0;
							}
							else{
								break;
							}
					}
					int i;
					for(i=0; i<tc; i++){
						state_untargettable(player, tapped[i], 0);
					}
					if( tc < 2 ){
						spell_fizzled = 1;
						return 0;
					}
					if( pick_target(&td1, "TARGET_CREATURE") ){
						for(i=0; i<2; i++){
							untap_card(player, tapped[i]);
						}
					}
				}
				instance->info_slot = choice;
                return generic_activated_ability(player, card, event,
                    choice == CHOICE_DAMAGE ? GAA_UNTAPPED : GAA_TAPPED, MANACOST0, 0, &td, NULL);
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( instance->info_slot == CHOICE_DAMAGE && valid_target(&td) ){
				damage_target0(player, card, 3);
			}
			if( instance->info_slot == CHOICE_BOUNCE && valid_target(&td1) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return hybrid(player, card, event);
}

int card_crag_puca(int player, int card, event_t event){
/* CARD_ID_CRAG_PUCA	9757
Crag Puca	|UR|UR|UR
Creature - Shapeshifter 2/4
|UR: Switch ~'s power and toughness until end of turn. */

  hybrid(player, card, event);

  if (event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card))
	return has_mana_hybrid(player, 1, COLOR_BLUE, COLOR_RED,
						   get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1)));

  if (event == EVENT_ACTIVATE)
	charge_mana_hybrid(player, card, 1, COLOR_BLUE, COLOR_RED,
					   get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1)));

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  switch_power_and_toughness_until_eot(player, card, instance->parent_controller, instance->parent_card);
	}

  return 0;
}

int card_creakwood_liege(int player, int card, event_t event){

	/* Creakwood Liege	|1|BG|BG|BG
	 * Creature - Horror 2/2
	 * Other |Sblack creatures you control get +1/+1.
	 * Other |Sgreen creatures you control get +1/+1.
	 * At the beginning of your upkeep, you may put a 1/1 |Sblack and |Sgreen Worm creature token onto the battlefield. */


	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WORM, &token);
		token.pow = 1;
		token.tou = 1;
		token.color_forced = COLOR_TEST_GREEN | COLOR_TEST_BLACK;
		generate_token(&token);
	}

	return liege(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_BLACK);
}

int card_deathbringer_liege(int player, int card, event_t event)
{

  /* Deathbringer Liege	|2|WB|WB|WB
   * Creature - Horror 3/4
   * Other |Swhite creatures you control get +1/+1.
   * Other |Sblack creatures you control get +1/+1.
   * Whenever you cast a |Swhite spell, you may tap target creature.
   * Whenever you cast a |Sblack spell, you may destroy target creature if it's tapped. */

  liege_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY, COLOR_TEST_BLACK, COLOR_TEST_WHITE);
  if (event == EVENT_SPA)
  {
    int result = get_color(trigger_cause_controller, trigger_cause);
    if (result & get_sleighted_color_test(player, card, COLOR_TEST_WHITE))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, TYPE_CREATURE);

      if (can_target(&td) &&
          new_pick_target(&td, "Select target creature to tap.", 0, GS_LITERAL_PROMPT))
      {
        action_on_target(player, card, 0, ACT_TAP);
      }
      get_card_instance(player, card)->number_of_targets = 0;
    }
    if (result & get_sleighted_color_test(player, card, COLOR_TEST_BLACK))
    {
      target_definition_t td;
      default_target_definition(player, card, &td, TYPE_CREATURE);

      card_instance_t *instance = get_card_instance(player, card);

      if (can_target(&td) &&
          new_pick_target(&td, "Select target creature to destroy if it's tapped.", 0,
                          GS_LITERAL_PROMPT))
      {
        if (is_tapped(instance->targets[0].player, instance->targets[0].card))
        {
          kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
        }
      }
      instance->number_of_targets = 0;
    }
  }

  return liege(player, card, event, COLOR_TEST_BLACK, COLOR_TEST_WHITE);
}

int card_deity_of_scars(int player, int card, event_t event){

	/* Deity of Scars	|BG|BG|BG|BG|BG
	 * Creature - Spirit Avatar 7/7
	 * Trample
	 * ~ enters the battlefield with two -1/-1 counters on it.
	 * |BG, Remove a -1/-1 counter from ~: Regenerate ~. */

	card_instance_t *instance = get_card_instance( player, card );

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( (event == EVENT_CAST_SPELL || event == EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE) && affect_me(player, card)){
		switch( event ){
				case EVENT_CAST_SPELL:
				{
					if( hybrid_casting(player, card, 0) ){
						enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 2);
					}
					else{
						spell_fizzled = 1;
					}
				}
				break;
				case EVENT_ENTERING_THE_BATTLEFIELD_AS_CLONE:
					enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 2);
					break;
				default:
					break;
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int result = generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST0, GVC_COUNTER(COUNTER_M1_M1), NULL, NULL);
		int result2 = generic_activated_ability(player, card, event, 0, MANACOST0, GVC_COUNTER(COUNTER_M1_M1), NULL, NULL);
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(1));
		if( has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_GREEN, c1) ){
			if( result ){
				return result;
			}
			else{
				return result2;
			}
		}
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(1));
		charge_mana_hybrid(player, card, 1, COLOR_BLACK, COLOR_GREEN, c1);
		if( spell_fizzled != 1 ){
			remove_counters(player, card, player, card, COUNTER_M1_M1, 1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		regenerate_or_shield(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card);
	}

	return 0;
}

int card_desecrator_hag(int player, int card, event_t event){
  /* Desecrator Hag	|2|BG|BG
   * Creature - Hag 2/2
   * When ~ enters the battlefield, return to your hand the creature card in your graveyard with the greatest power. If two or more cards are tied for greatest power, you choose one of them. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		const int *grave = get_grave(player);
		int doubles[2][count_graveyard(player)];
		int double_index = 0;
		int max_pow = -1;
		int count = 0;
		int selected = -1;
		while( grave[count] != -1 ){
				if( is_what(-1, grave[count], TYPE_CREATURE) ){
					int pwr = get_base_power_iid(player, grave[count]);
					if( pwr > max_pow ){
						selected = count;
						max_pow = pwr;
						doubles[0][0] = grave[selected];
						doubles[1][0] = selected;
						double_index = 1;
					}
					else if( pwr == max_pow ){
							doubles[0][double_index] = grave[count];
							doubles[1][double_index] = count;
							double_index++;
					}
				}
				count++;
		}
		if( selected != -1 ){
			if( double_index > 1 ){
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_CREATURE, "Select one among these creature cards.");
				selected = select_card_from_zone(player, player, doubles[0], double_index, 1, AI_MAX_VALUE, -1, &this_test);
			}
			int card_added = add_card_to_hand(player, grave[doubles[1][selected]]);
			remove_card_from_grave(player, doubles[1][selected]);
			reveal_card(player, card, player, card_added);
		}
	}

	return hybrid(player, card, event);
}

int card_divinity_of_pride(int player, int card, event_t event){
/* CARD_ID_DIVINITY_OF_PRIDE	9764
Divinity of Pride	|WB|WB|WB|WB|WB
Creature - Spirit Avatar 4/4
Flying, lifelink
~ gets +4/+4 as long as you have 25 or more life. */

	lifelink(player, card, event);

	if( life[player] > 24 && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		modify_pt_and_abilities(player, card, event, 4, 4, 0);
	}

	return hybrid(player, card, event);
}

int card_dominus_of_fealty(int player, int card, event_t event){
/* CARD_ID_DOMINUS_OF_FEALTY	9765
Dominus of Fealty	|UR|UR|UR|UR|UR
Creature - Spirit Avatar 4/4
Flying
At the beginning of your upkeep, you may gain control of target permanent until end of turn. If you do, untap it and it gains haste until end of turn. */

	if( trigger_condition == TRIGGER_UPKEEP && current_turn == player ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		upkeep_trigger_ability_mode(player, card, event, player, can_target(&td) ? RESOLVE_TRIGGER_AI(player) : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);

		card_instance_t *instance = get_card_instance( player, card );

		if( can_target(&td) && pick_target(&td, "TARGET_PERMANENT") ){
			if( player != instance->targets[0].player ){
				effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
			}
			else{
				untap_card(instance->targets[0].player, instance->targets[0].card);
				pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 0, 0, SP_KEYWORD_HASTE);
			}
		}
		instance->number_of_targets = 0;
	}

	return hybrid(player, card, event);
}

int card_doomgape(int player, int card, event_t event){
/* CARD_ID_DOOMGAPE	9766
Doomgape	|4|BG|BG|BG
Creature - Elemental 10/10
Trample
At the beginning of your upkeep, sacrifice a creature. You gain life equal to that creature's toughness. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int selected = pick_creature_for_sacrifice(player, card, 1);
		if( selected != -1 ){
			gain_life(player, get_toughness(player, selected));
			kill_card(player, selected, KILL_SACRIFICE);
		}
	}

	return hybrid(player, card, event);
}

int card_dream_fracture(int player, int card, event_t event){
/* CARD_ID_DREAM_FRACTURE	9769
Dream Fracture	|1|U|U
Instant
Counter target spell. Its controller draws a card. */

	if( event == EVENT_RESOLVE_SPELL ){
		if( counterspell_validate(player, card, NULL, 0) ){
			card_instance_t *instance = get_card_instance(player, card);
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(instance->targets[0].player, 1);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	else{
		return counterspell(player, card, event, NULL, 0);
	}

	return 0;
}

int card_dream_thief(int player, int card, event_t event){
/* CARD_ID_DREAM_THIEF	9770
Dream Thief	|2|U
Creature - Faerie Rogue 2/1
Flying
When ~ enters the battlefield, draw a card if you've cast another |Sblue spell this turn. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY && get_stormcolor_count(player, COLOR_BLUE) > 1 ){
		draw_cards(player, 1);
	}

	return hybrid(player, card, event);
}

int card_duergar_assailant(int player, int card, event_t event){
/* CARD_ID_DUERGAR_ASSAILANT	9771
Duergar Assailant	|RW
Creature - Dwarf Soldier 1/1
Sacrifice ~: ~ deals 1 damage to target attacking or blocking creature. */

	hybrid(player, card, event);

	if (!IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_CAN_TARGET | GAA_LITERAL_PROMPT, MANACOST0, 0,
									&td, "Select target attacking or blocking creature.");
}

int card_duergar_hedge_mage(int player, int card, event_t event){
/* CARD_ID_DUERGAR_HEDGE_MAGE	9773
Duergar Hedge-Mage	|2|RW
Creature - Dwarf Shaman 2/2
When ~ enters the battlefield, if you control two or more |H1Mountains, you may destroy target artifact.
When ~ enters the battlefield, if you control two or more |H1Plains, you may destroy target enchantment. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_ARTIFACT);

		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_ENCHANTMENT);

		card_instance_t *instance = get_card_instance( player, card );

		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN)) > 1 &&
			can_target(&td) && pick_target(&td, "TARGET_ARTIFACT") )
		{
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;

		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS)) > 1 &&
			can_target(&td1) && pick_target(&td1, "TARGET_ENCHANTMENT") )
		{
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
		instance->number_of_targets = 0;
	}

	return hybrid(player, card, event);
}

int card_duergar_mine_captain(int player, int card, event_t event){
/* CARD_ID_DUERGAR_MINE_CAPTAIN	9774
Duergar Mine-Captain	|2|RW
Creature - Dwarf Soldier 2/1
|1|RW, |Q: Attacking creatures get +1/+0 until end of turn. (|Q is the untap symbol.) */

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 1, 0);
		if( has_mana_hybrid(player, 1, COLOR_RED, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_TAPPED, MANACOST0, 0, NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, 1, 0, 0, 0, 1, 0);
		charge_mana_hybrid(player, card, 1, COLOR_RED, COLOR_WHITE, c1);
		if( spell_fizzled != 1 ){
			untap_card(player, card);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.state = STATE_ATTACKING;
		pump_creatures_until_eot(instance->parent_controller, instance->parent_card, player, 0, 1, 0, 0, 0, &this_test);
	}

	return 0;
}

int card_edge_of_the_divinity(int player, int card, event_t event){
/* CARD_ID_EDGE_OF_THE_DIVINITY	9776
Edge of the Divinity	|WB
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Swhite, it gets +1/+2.
As long as enchanted creature is |Sblack, it gets +2/+1. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_BLACK, 2, 1, 0, 0);
	aura_ability_for_color(player, card, event, COLOR_TEST_WHITE, 1, 2, 0, 0);
	if (IS_AURA_EVENT(player, card, event) || event == EVENT_STATIC_EFFECTS)
    return vanilla_aura(player, card, event, player);
  return 0;
}

int card_endless_horizons(int player, int card, event_t event){

	/* Endless Horizons	|3|W
	 * Enchantment
	 * When ~ enters the battlefield, search your library for any number of |H2Plains cards and exile them. Then shuffle your library.
	 * At the beginning of your upkeep, you may put a card you own exiled with ~ into your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card.", SUBTYPE_PLAINS));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_PLAINS);
		this_test.no_shuffle = 1;

		int *deck = deck_ptr[player];
		int index = 0, leg = 0, idx = 0;
		while( 1 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_DECK, 0, AI_MAX_VALUE, -1, &this_test);
				if( selected != -1 ){
					exiledby_remember(player, card, player, deck[selected], &leg, &idx);
					rfg_card_in_deck(player, selected);
					index++;
				}
				else{
					break;
				}
				if( player == AI && index >= 5 ){
					break;
				}
		}
		shuffle(player);
	}

	if( trigger_condition == TRIGGER_UPKEEP && current_turn == player &&
		 exiledby_choose(player, card, CARD_ID_ENDLESS_HORIZONS, EXBY_FIRST_FOUND, 0, "Plains", 1)
	  ){
		 upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		intptr_t rval = exiledby_choose(player, card, CARD_ID_ENDLESS_HORIZONS, EXBY_CHOOSE, 0, "Plains", 1);
		int* loc = (int*)rval;
		if( loc ){
			int *loc2 = exiledby_find(player, card, *loc, NULL, NULL);
			int iid = *loc2 & ~0x80000000;
			*loc2 = -1;
			add_card_to_hand(player, iid);
		}
	}

	if( event == EVENT_CAN_ACTIVATE && player == HUMAN ){
		return exiledby_choose(player, card, CARD_ID_ENDLESS_HORIZONS, EXBY_FIRST_FOUND, 0, "Plains", 1) != 0;
	}

	if( event == EVENT_ACTIVATE ){
		exiledby_choose(player, card, CARD_ID_ENDLESS_HORIZONS, EXBY_CHOOSE, 0, "Plains", 1);
		spell_fizzled = 1;
	}

	return global_enchantment(player, card, event);
}

int card_evershrike(int player, int card, event_t event){
	/* Evershrike	|3|WB|WB
	 * Creature - Elemental Spirit 2/2
	 * Flying
	 * ~ gets +2/+2 for each Aura attached to it.
	 * |X|WB|WB: Return ~ from your graveyard to the battlefield.
	 *		You may put an Aura card with converted mana cost X or less from your hand onto the battlefield attached to it.
	 *		If you don't, exile ~. */

	if ((event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		event_result += 2 * count_auras_enchanting_me(player, card);
	}
	if(event == EVENT_GRAVEYARD_ABILITY){
		if( has_mana_hybrid(player, 2, COLOR_BLACK, COLOR_WHITE, 0) ){
			return GA_RETURN_TO_PLAY_WITH_EFFECT;
		}
	}
	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		if( charge_mana_hybrid(player, card, 2, COLOR_BLACK, COLOR_WHITE, 0) ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				set_x_for_x_spells(player, card, EVENT_CAST_SPELL, x_value);
				return GAPAID_REMOVE;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATED_GRAVEYARD_ABILITY ){
		int exile_me = 1;
		if( hand_count[player] > 0 ){
			int amount = get_card_instance(player, card)->info_slot;

			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "");
			scnprintf(this_test.message, 100, "Select an Aura card with enchant creature and CMC %d or less.", amount);
			this_test.subtype = SUBTYPE_AURA_CREATURE;
			this_test.cmc = amount+1;
			this_test.cmc_flag = 3;
			this_test.zone = TARGET_ZONE_HAND;
			if( check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
				int result = new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
				if( result >= 0 ){
					put_into_play_aura_attached_to_target(player, result, player, card);
					exile_me = 0;
				}
			}
		}
		if( exile_me == 1 ){
			kill_card(player, card, KILL_EXILE);
		}
	}
	return hybrid(player, card, event);
}

int card_fable_of_the_wolf_and_owl(int player, int card, event_t event)
{
  /* Fable of Wolf and Owl	|3|GU|GU|GU
   * Enchantment
   * Whenever you cast a |Sgreen spell, you may put a 2/2 |Sgreen Wolf creature token onto the
   * battlefield. Whenever you cast a |Sblue spell, you may put a 1/1 |Sblue Bird creature token
   * with flying onto the battlefield. */

  if (event == EVENT_CAN_CAST || event == EVENT_SHOULD_AI_PLAY)
  {
    return global_enchantment(player, card, event);
  }

  liege_trigger(player, card, event, RESOLVE_TRIGGER_AI(player), COLOR_TEST_GREEN, COLOR_TEST_BLUE);
  if (event == EVENT_SPA)
  {
    int result = get_color(trigger_cause_controller, trigger_cause);
    token_generation_t token;
    if (result & get_sleighted_color_test(player, card, COLOR_TEST_BLUE))
    {
      default_token_definition(player, card, CARD_ID_BIRD, &token);
      token.pow = token.tou = 1;
      token.color_forced = COLOR_TEST_BLUE;
      generate_token(&token);
    }
    if (result & get_sleighted_color_test(player, card, COLOR_TEST_GREEN))
    {
      default_token_definition(player, card, CARD_ID_WOLF, &token);
      token.pow = token.tou = 2;
      token.color_forced = COLOR_TEST_GREEN;
      generate_token(&token);
    }
  }
  return hybrid(player, card, event);
}

int card_fang_skulkin(int player, int card, event_t event){
/* CARD_ID_FANG_SKULKIN	9781
Fang Skulkin	|2
Artifact Creature - Scarecrow 2/1
|2: Target |Sblack creature gains wither until end of turn. (It deals damage to creatures in the form of -1/-1 counters.) */

	return skulkin(player, card, event, 2, COLOR_TEST_BLACK, 0, 0, 0, SP_KEYWORD_WITHER);
}

int card_favor_of_the_overbeing(int player, int card, event_t event){
/* CARD_ID_FAVOR_OF_THE_OVERBEING	9782
Favor of the Overbeing	|1|GU
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Sgreen, it gets +1/+1 and has vigilance.
As long as enchanted creature is |Sblue, it gets +1/+1 and has flying. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_GREEN, 1, 1, 0, SP_KEYWORD_VIGILANCE);
	aura_ability_for_color(player, card, event, COLOR_TEST_BLUE, 1, 1, KEYWORD_FLYING, 0);
	return generic_aura(player, card, event, player, 0, 0, 0, 0, 0, 0, 0);
}

int card_fetid_heath(int player, int card, event_t event){
/* CARD_ID_FETID_HEATH	9783
Fetid Heath	""
Land
|T: Add |C to your mana pool.
|WB, |T: Add |W|W, |W|B, or |B|B to your mana pool. */

	return card_filter_land(player, card, event, COLOR_BLACK, COLOR_WHITE, COLOR_TEST_BLACK, COLOR_TEST_WHITE, " Colorless\n B -> BB\n B -> BW\n B -> WW\n W -> BB\n W -> BW\n W -> WW\n Cancel");
}

int card_fiery_bombardment(int player, int card, event_t event){
	/* Fiery Bombardment	|1|R	0x2007f96
	 * Enchantment
	 * Chroma - |2, Sacrifice a creature: ~ deals damage to any target equal to the number of |Sred mana symbols in the sacrificed creature's mana cost. */
	if( !IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_CREATURE, MANACOST_X(2), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(2)) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
			int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &test);
			if( ! sac ){
				spell_fizzled = 1;
				return 0;
			}
			state_untargettable(BYTE2(sac), BYTE3(sac), 0);
			if( pick_target(&td, "TARGET_ANY") ){
				instance->info_slot = chroma(BYTE2(sac), BYTE3(sac), COLOR_RED, 0);
				kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return 0;
}

int card_figure_of_destiny(int player, int card, event_t event)
{
  /* CARD_ID_FIGURE_OF_DESTINY	9785
   * Figure of Destiny	|RW
   * Creature - Kithkin 1/1
   * |RW: ~ becomes a Kithkin Spirit with base power and toughness 2/2.
   * |RW|RW|RW: If ~ is a Spirit, it becomes a Kithkin Spirit Warrior with base power and toughness 4/4.
   * |RW|RW|RW|RW|RW|RW: If ~ is a Warrior, it becomes a Kithkin Spirit Warrior Avatar with base power and toughness 8/8, flying, and first strike. */

  uint8_t ability_mode;
  card_instance_t *instance;
  int ability_choice_or_result;
  int warrior_match;
  int avatar_match;
  int colorless;
  int spirit_available;
  int warrior_available;
  int avatar_available;
  uint32_t avatar_priority;
  uint32_t warrior_priority;
  CardFunction func_ptr;
  int32_t player_00;
  int32_t card_00;
  uint32_t spirit_priority;
  int to_charge[4];
  hybrid(player, card, event);
  if (event == EVENT_CAN_ACTIVATE)
  {
    ability_choice_or_result = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 0, 1, 0);
    ability_choice_or_result = has_mana_hybrid(player, 1, 4, 5, ability_choice_or_result);
    if (ability_choice_or_result == 0)
    {
      return 0;
    }
    ability_choice_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x800, 0,
                                                         0, 0, 0, 0, 0, 0, NULL, NULL);
    return ability_choice_or_result;
  }
  if (event != EVENT_ACTIVATE)
  {
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
    instance = get_card_instance(player, card);
    ability_mode = instance->mode;
    if (ability_mode == '\x01')
    {
      while (ability_choice_or_result = find_repeat_legacy_effect(
                 instance->parent_controller, instance->parent_card, fod_2,
                 instance->parent_controller, instance->parent_card),
             ability_choice_or_result != -1)
      {
        kill_card(player, ability_choice_or_result, KILL_DESTROY);
      }
      while (ability_choice_or_result = find_repeat_legacy_effect(
                 instance->parent_controller, instance->parent_card, fod_3,
                 instance->parent_controller, instance->parent_card),
             ability_choice_or_result != -1)
      {
        kill_card(player, ability_choice_or_result, KILL_DESTROY);
      }
      card_00 = instance->parent_card;
      player_00 = instance->parent_controller;
      func_ptr = fod_1;
    }
    else if (ability_mode == '\x02')
    {
      ability_choice_or_result =
          has_subtype(instance->parent_controller, instance->parent_card, SUBTYPE_SPIRIT);
      if (ability_choice_or_result == 0)
      {
        ai_modifier = ai_modifier - 100;
        return 0;
      }
      while (ability_choice_or_result = find_repeat_legacy_effect(
                 instance->parent_controller, instance->parent_card, fod_1,
                 instance->parent_controller, instance->parent_card),
             ability_choice_or_result != -1)
      {
        kill_card(player, ability_choice_or_result, KILL_DESTROY);
      }
      while (ability_choice_or_result = find_repeat_legacy_effect(
                 instance->parent_controller, instance->parent_card, fod_3,
                 instance->parent_controller, instance->parent_card),
             ability_choice_or_result != -1)
      {
        kill_card(player, ability_choice_or_result, KILL_DESTROY);
      }
      card_00 = instance->parent_card;
      player_00 = instance->parent_controller;
      func_ptr = fod_2;
    }
    else
    {
      if (ability_mode != '\x03')
      {
        return 0;
      }
      ability_choice_or_result =
          has_subtype(instance->parent_controller, instance->parent_card, SUBTYPE_WARRIOR);
      if (ability_choice_or_result == 0)
      {
        ai_modifier = ai_modifier + -100;
        return 0;
      }
      while (ability_choice_or_result = find_repeat_legacy_effect(
                 instance->parent_controller, instance->parent_card, fod_1,
                 instance->parent_controller, instance->parent_card),
             ability_choice_or_result != -1)
      {
        kill_card(player, ability_choice_or_result, KILL_DESTROY);
      }
      while (ability_choice_or_result = find_repeat_legacy_effect(
                 instance->parent_controller, instance->parent_card, fod_2,
                 instance->parent_controller, instance->parent_card),
             ability_choice_or_result != -1)
      {
        kill_card(player, ability_choice_or_result, KILL_DESTROY);
      }
      card_00 = instance->parent_card;
      player_00 = instance->parent_controller;
      func_ptr = fod_3;
    }
    ability_choice_or_result =
        create_targetted_legacy_effect_no_repeat(player_00, card_00, func_ptr, player_00, card_00);
    instance = get_card_instance(player, ability_choice_or_result);
    instance->token_status = instance->token_status | 0x10000;
    return 0;
  }
  instance = get_card_instance(player, card);
  instance->mode = '\0';
  ability_choice_or_result = has_subtype(player, card, SUBTYPE_SPIRIT);
  warrior_match = has_subtype(player, card, SUBTYPE_WARRIOR);
  avatar_match = has_subtype(player, card, SUBTYPE_AVATAR);
  colorless = get_cost_mod_for_activated_abilities(player, card, 0, 0, 0, 0, 1, 0);
  spirit_available = has_mana_hybrid(player, 1, 4, 5, colorless);
  warrior_available = has_mana_hybrid(player, 3, 4, 5, colorless);
  avatar_available = has_mana_hybrid(player, 6, 4, 5, colorless);
  warrior_priority = 0xffffff9c;
  spirit_priority = 0xffffff9c;
  avatar_priority = 0xffffff9c;
  if (ability_choice_or_result == 0 && warrior_match == 0)
    spirit_priority = 10;
  else if (ability_choice_or_result != 0 && warrior_match == 0)
    warrior_priority = 0xf;
  else if (avatar_match == 0)
    avatar_priority = 0x14;
  ability_choice_or_result = dialog_impl(
      player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_MODE, "2/2 Kithkin Spirit", spirit_available,
      spirit_priority, "4/4 Kithkin Spirit Warrior", warrior_available, warrior_priority,
      "8/8 Kithkin Spirit Warrior Avatar", avatar_available, avatar_priority, 0, 0, 0, 0);
  if (ability_choice_or_result == 0)
  {
    spell_fizzled = 1;
    return 0;
  }
  to_charge[0] = 0;
  to_charge[1] = 1;
  to_charge[2] = 3;
  to_charge[3] = 6;
  ability_choice_or_result =
      charge_mana_hybrid(player, card, to_charge[ability_choice_or_result], 4, 5, colorless);
  if (ability_choice_or_result != 0)
  {
    ability_choice_or_result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x800, 0, 0,
                                                         0, 0, 0, 0, 0, NULL, NULL);
    return ability_choice_or_result;
  }
  return 0;
}

int card_fire_at_will(int player, int card, event_t event){
/* CARD_ID_FIRE_AT_WILL	9786
Fire at Will	|RW|RW|RW
Instant
~ deals 3 damage divided as you choose among one, two, or three target attacking or blocking creatures. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.required_state = TARGET_STATE_IN_COMBAT;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( hybrid_casting(player, card, 0) ){
			int i;
			for(i=0; i<3; i++){
				if( new_pick_target(&td, "Select target attacking or blocking creature.", -1, 1 | GS_LITERAL_PROMPT) ){
					add_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
				}
				else{
					break;
				}
			}
			for(i=0; i<instance->number_of_targets; i++){
				remove_state(instance->targets[i].player, instance->targets[i].card, STATE_TARGETTED);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		divide_damage(player, card, &td);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_flame_jab(int player, int card, event_t event){
	/* Flame Jab	|R	0x2007fa0
	 * Sorcery
	 * ~ deals 1 damage to any target.
	 * Retrace */
	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return retrace(player, card, event, CARD_ID_FLAME_JAB);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		damage_target0(player, card, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_flickerwhisp(int player, int card, event_t event){
/* CARD_ID_FLICKERWISP	9788
Flickerwisp	|1|W|W
Creature - Elemental 3/1
Flying
When ~ enters the battlefield, exile another target permanent. Return that card to the battlefield under its owner's control at the beginning of the next end step. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_PERMANENT);
		td.allow_cancel = 0;
		td.special = TARGET_SPECIAL_NOT_ME;

		card_instance_t *instance = get_card_instance(player, card);

		if( can_target(&td) && new_pick_target(&td, "Select another target permanent.", 0, GS_LITERAL_PROMPT) ){
			remove_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
		}
		instance->number_of_targets = 0;
	}

	return 0;
}

int card_flooded_grove(int player, int card, event_t event){
/* CARD_ID_FLOODED_GROVE	9789
Flooded Grove	""
Land
|T: Add |C to your mana pool.
|GU, |T: Add |G|G, |G|U, or |U|U to your mana pool. */

	return card_filter_land(player, card, event, COLOR_BLUE, COLOR_GREEN, COLOR_TEST_BLUE, COLOR_TEST_GREEN, " Colorless\n U -> UU\n U -> UG\n U -> GG\n G -> UU\n G -> UG\n G -> GG\n Cancel");
}

int card_gift_of_the_deity(int player, int card, event_t event){
/* CARD_ID_GIFT_OF_THE_DEITY	9790
Gift of the Deity	|4|BG
Enchantment - Aura
Enchant creature
As long as enchanted creature is |Sblack, it gets +1/+1 and has deathtouch. (Any amount of damage it deals to a creature is enough to destroy that creature.)
As long as enchanted creature is |Sgreen, it gets +1/+1 and all creatures able to block it do so. */

	hybrid(player, card, event);
	aura_ability_for_color(player, card, event, COLOR_TEST_BLACK, 1, 1, 0, SP_KEYWORD_DEATHTOUCH);
	aura_ability_for_color(player, card, event, COLOR_TEST_GREEN, 1, 1, 0, SP_KEYWORD_LURE);
	return generic_aura(player, card, event, player, 0, 0, 0, 0, 0, 0, 0);
}

int card_gilder_bairn(int player, int card, event_t event)
{
  /* CARD_ID_GILDER_BAIRN	9791
   * Gilder Bairn	|1|GU|GU
   * Creature - Ouphe 1/3
   * |2|GU, |Q: Double the number of each kind of counter on target permanent. (|Q is the untap symbol.) */

  int t_player;
  card_instance_t *instance;
  int result;
  target_definition_t *permanent_target_definition;
  target_definition_t td;

  hybrid(player, card, event);
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x80004, 0, 0, 0, 0, 0,
                                         0, 0, &td, NULL);
      if (result == 0)
      {
        return 0;
      }
      result = get_cost_mod_for_activated_abilities(player, card, 2, 0, 0, 1, 0, 0);
      result = has_mana_hybrid(player, 1, 2, 3, result);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x80804, 0, 0, 0, 0, 0, 0,
                                       0, &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      result = get_cost_mod_for_activated_abilities(player, card, 2, 0, 0, 1, 0, 0);
      result = charge_mana_hybrid(player, card, 1, 2, 3, result);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x80804, 0, 0, 0, 0, 0, 0, 0,
                                       &td, "TARGET_PERMANENT");
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x47);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
  }
  permanent_target_definition = &td;
  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (result = valid_target(permanent_target_definition), result != 0))
  {
    result = instance->targets[0].card;
    t_player = instance->targets[0].player;
    copy_counters(player, card, t_player, result, t_player, result, 0);
  }
  return 0;
}

int card_glen_elendra_archmage(int player, int card, event_t event){
/* CARD_ID_GLEN_ELENDRA_ARCHMAGE	9793
Glen Elendra Archmage	|3|U
Creature - Faerie Wizard 2/2
Flying
|U, Sacrifice ~: Counter target noncreature spell.
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

	persist(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_ANY);
	td.illegal_type = TYPE_CREATURE;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( counterspell_validate(player, card, NULL, 0) ){
			real_counter_a_spell(player, card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME | GAA_SPELL_ON_STACK, MANACOST_U(1), 0, &td, NULL);
}

int card_gwyllion_hedge_mage(int player, int card, event_t event){
	/* Gwyllion Hedge-Mage	|2|WB
	 * Creature - Hag Wizard 2/2
	 * When ~ enters the battlefield, if you control two or more |H1Plains, you may put a 1/1 |Swhite Kithkin Soldier creature token onto the battlefield.
	 * When ~ enters the battlefield, if you control two or more |H1Swamps, you may put a -1/-1 counter on target creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance( player, card );

		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS) ) > 1 ){
			int choice = duh_mode(player) ? 0 : do_dialog(player, player, card, -1, -1, " Generate a Kithkin\n Pass", 0);
			if( ! choice ){
				generate_token_by_id(player, card, CARD_ID_KITHKIN_SOLDIER);
			}
		}
		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP)) > 1 && can_target(&td1) &&
			new_pick_target(&td1, "TARGET_CREATURE", 0, 0) )
		{
			add_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1, 1);
		}
		instance->number_of_targets = 0;
	}

	return hybrid(player, card, event);
}

int card_hag_hedge_mage(int player, int card, event_t event){
/* CARD_ID_HAG_HEDGE_MAGE	9797
Hag Hedge-Mage	|2|BG
Creature - Hag Shaman 2/2
When ~ enters the battlefield, if you control two or more |H1Swamps, you may have target player discard a card.
When ~ enters the battlefield, if you control two or more |H1Forests, you may put target card from your graveyard on top of your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		target_definition_t td1;
		default_target_definition(player, card, &td1, TYPE_CREATURE);
		td1.zone = TARGET_ZONE_PLAYERS;

		card_instance_t *instance = get_card_instance( player, card );

		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_FOREST)) > 1 ){
			if( count_graveyard(player) > 0 && ! graveyard_has_shroud(player) ){
				test_definition_t this_test;
				default_test_definition(&this_test, TYPE_ANY);
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_DECK, 0, AI_MAX_VALUE, &this_test);
			}
		}

		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP)) > 1 ){
			if( can_target(&td1) && pick_target(&td1, "TARGET_PLAYER") ){
				discard(instance->targets[0].player, 0, player);
			}
		}
		instance->number_of_targets = 0;
	}

	return hybrid(player, card, event);
}

int card_hallowed_burial(int player, int card, event_t event){
	/* Hallowed Burial	|3|W|W
	 * Sorcery
	 * Put all creatures on the bottom of their owners' libraries. */

	if(event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_manipulate_all(player, card, ANYBODY, &this_test, ACT_PUT_ON_BOTTOM);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_hateflayer(int player, int card, event_t event){
	/* Hateflayer	|5|R|R	0x2007fb9
	 * Creature - Elemental 5/5
	 * Wither
	 * |2|R, |Q: ~ deals damage equal to its power to any target. */
	wither(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td1) ){
			damage_target0(player, card, get_power(instance->parent_controller, instance->parent_card));
		}
	}

	return generic_activated_ability(player, card, event, GAA_TAPPED|GAA_CAN_TARGET, MANACOST_XR(2, 1), 0,
									 &td1, "TARGET_ANY");
}

int card_heartfire_goblin(int player, int card, event_t event){
/* CARD_ID_HEARTHFIRE_HOBGOBLIN	9802
Hearthfire Hobgoblin	|RW|RW|RW
Creature - Goblin Soldier 2/2
Double strike*/
	return hybrid(player, card, event);
}

int card_helix_pinnacle(int player, int card, event_t event)
{
  /* Helix Pinnacle	|G
   * Enchantment
   * Shroud
   * |X: Put X tower counters on ~.
   * At the beginning of your upkeep, if there are 100 or more tower counters on ~, you win the game. */

  int result;
  card_instance_t *instance;
  unsigned int cless;
  int ai_score;
  if (event == EVENT_CAN_CAST)
  {
    return 1;
  }
  if (((current_turn == player) &&
       (result = count_counters(player, card, COUNTER_TOWER), 99 < result)) &&
      (result = upkeep_trigger(player, card, event), result != 0))
  {
    lose_the_game(1 - player);
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = can_use_activated_abilities(player, card);
    if (result == 0)
    {
      return 0;
    }
    cless = 1;
    if ((ai_is_speculating != 1) && (cless = 0, player == 1))
    {
      cless = (unsigned int)(((unsigned char)trace_mode & 2) == 0);
    }
    result = has_mana_for_activated_ability(player, card, cless, 0, 0, 0, 0, 0);
    return (unsigned int)(result != 0);
  }
  if (event == EVENT_ACTIVATE)
  {
    result = charge_mana_for_activated_ability(player, card, -1, 0, 0, 0, 0, 0);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      result = x_value;
      instance->info_slot = x_value;
      if ((player == 1) && ((current_turn != 0 || (current_phase != 0x1f))))
      {
        ai_modifier = ai_modifier - (result * 2 + -1);
      }
    }
    return global_enchantment(player, card, event);
  }
  if (event == EVENT_GET_SELECTED_CARD)
  {
    load_recorded_action_code(0);
    return global_enchantment(player, card, event);
  }
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    add_counters(player, card, player, card, COUNTER_TOWER, instance->info_slot);
    return global_enchantment(player, card, event);
  }
  if (event != EVENT_SHOULD_AI_PLAY)
    return global_enchantment(player, card, event);
  result = count_counters(player, card, COUNTER_TOWER);
  ai_score = result * 2 + 4;
  if (player == 0 || (1 - player == current_turn && current_phase == 0x1f))
    ai_score = ai_score + (unsigned int)(result * result) / 10 +
               (result * result * result * result) / 100000;
  if (result > 99)
    ai_score = ai_score + 1000;
  if (player == 0)
    ai_modifier = ai_modifier - ai_score;
  else
    ai_modifier = ai_modifier + ai_score;
  result = global_enchantment(player, card, event);
  return result;
}

int card_hoof_skulkin(int player, int card, event_t event){
/* CARD_ID_HOOF_SKULKIN	9806
Hoof Skulkin	|3
Artifact Creature - Scarecrow 2/2
|3: Target |Sgreen creature gets +1/+1 until end of turn. */

	return skulkin(player, card, event, 3, COLOR_TEST_GREEN, 1, 1, 0, 0);
}

int card_hotheaded_giant(int player, int card, event_t event){
/* CARD_ID_HOTHEADED_GIANT	9807
Hotheaded Giant	|3|R
Creature - Giant Warrior 4/4
Haste
~ enters the battlefield with two -1/-1 counters on it unless you've cast another |Sred spell this turn. */

	haste(player, card, event);
	if( event == EVENT_CAST_SPELL && affect_me(player, card) && get_stormcolor_count(player, COLOR_RED) < 1 ){
		add_counters(player, card, player, card, COUNTER_M1_M1, 2);
	}
	return 0;
}

int card_idle_thoughts(int player, int card, event_t event){
/* CARD_ID_IDLE_THOUGHTS	9808
Idle Thoughts	|3|U
Enchantment
|2: Draw a card if you have no cards in hand. */

	if( event == EVENT_CAN_ACTIVATE && hand_count[player] < 1 ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(2), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_inundate(int player, int card, event_t event){
/* CARD_ID_INUNDATE	9812
Inundate	|3|U|U|U
Sorcery
Return all non|Sblue creatures to their owners' hands. */

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		this_test.color_flag = 1;
		new_manipulate_all(player, card, 2, &this_test, ACT_BOUNCE);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 0, NULL);
}

int card_inside_out(int player, int card, event_t event)
{
/* CARD_ID_INSIDE_OUT	9811
Inside Out	|1|UR
Instant
Switch target creature's power and toughness until end of turn.
Draw a card. */


	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = ANYBODY;

	card_instance_t* instance = get_card_instance(player, card);

	if (event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
	{
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		if (valid_target(&td))
		{
			switch_power_and_toughness_until_eot(player, card, instance->targets[0].player, instance->targets[0].card);
			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_invert_the_skies(int player, int card, event_t event){
/* CARD_ID_INVERT_THE_SKIES	9813
Invert the Skies	|3|GU
Instant
Creatures your opponents control lose flying until end of turn if |G was spent to cast ~,
and creatures you control gain flying until end of turn if |U was spent to cast it. (Do both if |G|U was spent.) */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		int c1 = get_updated_casting_cost(player, card, -1, event, 3);
		if( has_mana_hybrid(player, 1, COLOR_BLUE, COLOR_GREEN, c1) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int c1 = get_updated_casting_cost(player, card, -1, event, 3);
			test_definition_t test;
			char mana_text[3][100];
			get_mana_from_text(&test, MANACOST_XU(c1, 1));
			strcpy(mana_text[0], test.message);
			get_mana_from_text(&test, MANACOST_XG(c1, 1));
			strcpy(mana_text[1], test.message);
			int can_play_full_effect = 0;
			if( c1 > 0 ){
				if( c1 > 1 ){
					get_mana_from_text(&test, MANACOST_XUG(c1-1, 1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_XUG(c1-1, 1, 1));
				}
				else{
					get_mana_from_text(&test, MANACOST_UG(1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_UG(1, 1));
				}
			}
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							mana_text[0], has_mana_multi(player, MANACOST_XU(c1, 1)), 10,
							mana_text[1], has_mana_multi(player, MANACOST_XG(c1, 1)), 5,
							mana_text[2], can_play_full_effect, 15);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			charge_mana_multi(player, MANACOST_XUG(
													(c1 > 1 ? (choice != 3 ? c1 : c1-1) : (choice != 3 ? c1 : 0)),
													(choice == 1 || choice == 3),
													(choice == 2 || choice == 3)
													)
								);
		}
		if( spell_fizzled != 1 ){
			if( mana_paid[COLOR_BLUE] > 0 ){
				instance->info_slot |= 2;
			}
			if( mana_paid[COLOR_GREEN] > 0 ){
				instance->info_slot |= 4;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 2) ){
			pump_subtype_until_eot(player, card, player, -1, 0, 0,  KEYWORD_FLYING, 0);
		}
		if( (instance->info_slot & 4) ){
			negate_ability_until_eot(player, card, 1-player, -1, KEYWORD_FLYING);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_jawbone_skulkin(int player, int card, event_t event){
/* CARD_ID_JAWBONE_SKULKIN	9814
Jawbone Skulkin	|1
Artifact Creature - Scarecrow 1/1
|2: Target |Sred creature gains haste until end of turn. */

	return skulkin(player, card, event, 2, COLOR_TEST_RED, 0, 0, 0, SP_KEYWORD_HASTE);
}

int card_leering_emblem(int player, int card, event_t event){
/* CARD_ID_LEERING_EMBLEM	9817
Leering Emblem	|2
Artifact - Equipment
Whenever you cast a spell, equipped creature gets +2/+2 until end of turn.
Equip |2 */

	if( event == EVENT_ACTIVATE && ! is_equipping(player, card) ){
		ai_modifier+=30;
	}

	if (is_equipping(player, card)){
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_ANY, NULL, player, card);
		if (event == EVENT_SPA){
			card_instance_t *instance = get_card_instance( player, card );
			pump_until_eot(player, card, instance->damage_target_player, instance->damage_target_card, 2, 2);
		}
	}

	return basic_equipment(player, card, event, 2);
}

int card_light_from_within(int player, int card, event_t event){
/* CARD_ID_LIGHT_FROM_WITHIN	9818
Light from Within	|2|W|W
Enchantment
Chroma - Each creature you control gets +1/+1 for each |Swhite mana symbol in its mana cost. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			event_result += chroma(affected_card_controller, affected_card, COLOR_WHITE, 0);
		}
	}

	return global_enchantment(player, card, event);
}

int card_merrow_bonegnawer(int player, int card, event_t event){
/* CARD_ID_MERROW_BONEGNAWER	9822
Merrow Bonegnawer	|B
Creature - Merfolk Rogue 1/1
|T: Target player exiles a card from his or her graveyard.
Whenever you cast a |Sblack spell, you may untap ~. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
	    test_definition_t test;
	    default_test_definition(&test, TYPE_ANY);
	    test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
	    sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), 0, &test, player, card);
	}
	if (event == EVENT_SPA){ untap_card(player, card); return 0; }
	if (!IS_GAA_EVENT(event)){ return 0; }
	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;
	card_instance_t* instance = get_card_instance(player, card);
	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) && count_graveyard(instance->targets[0].player)){
	    test_definition_t this_test;
	    char msg[100] = "Select a card to exile.";
	    new_default_test_definition(&this_test, TYPE_ANY, msg);
	    new_global_tutor(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_GRAVE, TUTOR_RFG, 1, AI_MIN_VALUE, &this_test);
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PLAYER");
}

int card_merrow_levitator(int player, int card, event_t event){
/* CARD_ID_MERROW_LEVITATOR	9823
Merrow Levitator	|3|U
Creature - Merfolk Wizard 2/3
|T: Target creature gains flying until end of turn.
Whenever you cast a |Sblue spell, you may untap ~. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){ untap_card(player, card); return 0; }

	if (!IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_ability_until_eot(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 0, 0, KEYWORD_FLYING, 0);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_mindwrack_liege(int player, int card, event_t event){

	/* Mindwrack Liege	|3|UR|UR|UR
	 * Creature - Horror 4/4
	 * Other |Sblue creatures you control get +1/+1.
	 * Other |Sred creatures you control get +1/+1.
	 * |UR|UR|UR|UR: You may put a |Sblue or |Sred creature card from your hand onto the battlefield. */

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(4));
		if( has_mana_hybrid(player, 4, COLOR_BLUE, COLOR_RED, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(4));
		charge_mana_hybrid(player, card, 4, COLOR_BLUE, COLOR_RED, c1);
		if( spell_fizzled != 1 ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, get_sleighted_color_text2(player, card, "Select a %s or %s creature card.", COLOR_BLUE, COLOR_RED));
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE | COLOR_TEST_RED);
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

	return liege(player, card, event, COLOR_TEST_BLUE, COLOR_TEST_RED);
}

static int mirror_sheen_player = 0;
/* ManalinkEh 2022: 0x2362ea0; original cards/eventide.c:1682. */
static int fod_1(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (((uint32_t)(event - EVENT_POWER) < 2) || (event == EVENT_CHANGE_TYPE))
  {

    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {

      instance =
          get_card_instance((int)instance->damage_target_player, instance->damage_target_card);
      if ((uint32_t)(event - EVENT_POWER) < 2)
      {
        set_base_pt(2);
      }
      else
      {
        update_alter_remove_creature_subtypes(instance);
        update_alter_add_subtype(SUBTYPE_KITHKIN, instance);
        update_alter_add_subtype(SUBTYPE_SPIRIT, instance);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2363030; original cards/eventide.c:1699. */
static int fod_2(int player, int card, event_t event)
{
  card_instance_t *instance;

  if (((uint32_t)(event - EVENT_POWER) < 2) || (event == EVENT_CHANGE_TYPE))
  {

    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {

      instance =
          get_card_instance((int)instance->damage_target_player, instance->damage_target_card);
      if ((uint32_t)(event - EVENT_POWER) < 2)
      {
        set_base_pt(4);
      }
      else
      {
        update_alter_remove_creature_subtypes(instance);
        update_alter_add_subtype(SUBTYPE_KITHKIN, instance);
        update_alter_add_subtype(SUBTYPE_SPIRIT, instance);
        update_alter_add_subtype(SUBTYPE_WARRIOR, instance);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2362f40; original cards/eventide.c:1717. */
static int fod_3(int player, int card, event_t event)
{
  if ((uint32_t)(event - EVENT_POWER) < 2 || event == EVENT_CHANGE_TYPE || event == EVENT_ABILITIES)
  {
    card_instance_t *inst = get_card_instance(player, card);
    int p = inst->damage_target_player;
    int c = inst->damage_target_card;
    if (affect_me(p, c))
    {
      card_instance_t *att = get_card_instance(p, c);
      if ((uint32_t)(event - EVENT_POWER) < 2)
        set_base_pt(8);
      else if (event == EVENT_ABILITIES)
        event_result |= 0x120;
      else
      {
        update_alter_remove_creature_subtypes(att);
        update_alter_add_subtype(SUBTYPE_KITHKIN, att);
        update_alter_add_subtype(SUBTYPE_SPIRIT, att);
        update_alter_add_subtype(SUBTYPE_WARRIOR, att);
        update_alter_add_subtype(SUBTYPE_AVATAR, att);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x236ad60; original cards/eventide.c:2459. */
int card_lingering_tormentor(int player, int card, event_t event)
{
  /* Lingering Tormentor	|3|B
   * Creature - Spirit 2/2
   * Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
   * Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

  fear(player, card, event);
  persist(player, card, event);
  return 0;
}

static const char* target_is_targeting_mirror_sheen_player(int who_chooses, int player, int card, int unused_source_player, int unused_source_card)
{
  if (target_me(mirror_sheen_player, -1, player, card))
	return NULL;
  else
	return "targets you";
}

int card_mirror_sheen(int player, int card, event_t event){
/* CARD_ID_MIRROR_SHEEN	9825
Mirror Sheen	|1|UR|UR
Enchantment
|1|UR|UR: Copy target instant or sorcery spell that targets you. You may choose new targets for the copy. */

	card_instance_t *instance = get_card_instance(player, card);

	hybrid(player, card, event);

	if (!IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	counterspell_target_definition(player, card, &td, TYPE_INSTANT|TYPE_SORCERY);
	td.preferred_controller = 2;
	td.extra = target_predicate_extra(target_is_targeting_mirror_sheen_player);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	mirror_sheen_player = player;

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST0, 0, NULL, NULL) ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XU(1, 2));
		if( has_mana_hybrid(player, 2, COLOR_BLUE, COLOR_RED, c1) ){
			return activate_twincast(player, card, event, &td, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		activate_twincast(player, card, event, &td, NULL);
		if (spell_fizzled != 1){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_XU(1, 2));
			charge_mana_hybrid(player, card, 2, COLOR_BLUE, COLOR_RED, c1);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		activate_twincast(player, card, event, &td, NULL);
	}

	return 0;
}

int card_monstrify(int player, int card, event_t event){
/* CARD_ID_MONSTRIFY	9826
Monstrify	|3|G
Sorcery
Target creature gets +4/+4 until end of turn.
Retrace (You may cast this card from your graveyard by discarding a land card in addition to paying its other costs.) */

	if (IS_CASTING_FROM_GRAVE(event)){
		return retrace(player, card, event, CARD_ID_MONSTRIFY);
	}
	return card_monstrous_growth(player, card, event);
}

int card_murkfiend_liege(int player, int card, event_t event){

	/* Murkfiend Liege	|2|GU|GU|GU
	 * Creature - Horror 4/4
	 * Other |Sgreen creatures you control get +1/+1.
	 * Other |Sblue creatures you control get +1/+1.
	 * Untap all |Sgreen and/or |Sblue creatures you control during each other player's untap step. */

	if( beginning_of_phase(player, card, event, 1-player, -1, PHASE_UNTAP) ){
		int clr = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		clr |= get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = clr;
		new_manipulate_all(player, card, player, &this_test, ACT_UNTAP);
	}

	return liege(player, card, event, COLOR_TEST_BLUE, COLOR_TEST_GREEN);
}

static int necroskitter_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_NECROSKITTER)
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
        if (count_counters(affected_card_controller, affected_card, COUNTER_M1_M1))
        {
          count_for_gfp_ability_and_store_values(
              player, card, event, 1 - player, TYPE_CREATURE, NULL,
              GFPC_TRACK_DEAD_CREATURES | GFPC_EXTRA_SKIP_TOKENS | GFPC_STORE_OWNER, 1);
        }
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int amount = MIN(10, BYTE0(result));
    for (int i = 1; i <= amount; ++i)
      seek_grave_for_id_to_reanimate(player, -1, instance->targets[i].player,
                                     cards_data[instance->targets[i].card].id, REANIMATE_DEFAULT);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_necroskitter(int player, int card, event_t event)
{
  /* Necroskitter |1|B|B
   *   Creature — Elemental 1/4, 1BB (3)
   *   Wither (This deals damage to creatures in the form of -1/-1 counters.)
   *   Whenever a creature an opponent controls with a -1/-1 counter on it dies, you may return that card
   *   to the battlefield under your control. */

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    int c;
    for (c = 0; c < active_cards_count[player]; c++)
    {
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(player, c);
        if (inst->info_slot == (intptr_t)necroskitter_effect)
        {
          if ((inst->damage_target_player == player && inst->damage_target_card == card) ||
              (inst->targets[0].player == player && inst->targets[0].card == card))
          {
            found = 1;
            break;
          }
        }
      }
    }
    if (!found)
    {
      int legacy = create_targetted_legacy_effect(player, card, &necroskitter_effect, player, card);
      card_instance_t *inst = get_card_instance(player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
  }

  wither(player, card, event);

  if (event == EVENT_GRAVEYARD_FROM_PLAY &&
      count_counters(affected_card_controller, affected_card, COUNTER_M1_M1))
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    count_for_gfp_ability_and_store_values(
        player, card, event, 1 - player, -1, &test,
        GFPC_TRACK_DEAD_CREATURES | GFPC_EXTRA_SKIP_TOKENS | GFPC_STORE_OWNER, 0);
  }

  if (get_card_instance(player, card)->kill_code == 0)
  {
    int amount = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
    if (amount)
    {
      card_instance_t *instance = get_card_instance(player, card);
      int i;
      for (i = 0; i < amount && i < 11; i++)
      {
        int owner = instance->targets[i].player;
        int iid = instance->targets[i].card;
        seek_grave_for_id_to_reanimate(player, card, owner, cards_data[iid].id, REANIMATE_DEFAULT);
      }
    }
  }

  return 0;
}

int card_needle_specter(int player, int card, event_t event){
  /* Needle Specter |1|B|B
   * Creature — Specter 1/1
   * Flying
   * Wither (This deals damage to creatures in the form of -1/-1 counters.)
   * Whenever Needle Specter deals combat damage to a player, that player discards that many cards. */

	wither(player, card, event);
	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT)){
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = {BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player)};
		instance->targets[1].player = 0;
		int damages[2] = {BYTE0(instance->targets[16].player), BYTE1(instance->targets[16].player)};
		instance->targets[16].player = 0;
		APNAP(p, { if (times_damaged[p]){ new_multidiscard(p, damages[p], 0, player); } });
	}
	return 0;
}

int card_nettle_sentinel(int player, int card, event_t event){
/* CARD_ID_NETTLE_SENTINEL	9831
Nettle Sentinel	|G
Creature - Elf Warrior 2/2
~ doesn't untap during your untap step.
Whenever you cast a |Sgreen spell, you may untap ~. */

	does_not_untap(player, card, event);
	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		untap_card(player, card);
	}
	return 0;
}

int card_nightmare_incursion(int player, int card, event_t event){
/* CARD_ID_NIGHTMARE_INCURSION	9832
Nightmare Incursion	|5|B
Sorcery
Search target player's library for up to X cards, where X is the number of |H1Swamps you control, and exile them. Then that player shuffles his or her library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = count_subtype(player, TYPE_LAND, SUBTYPE_SWAMP);
			int removed[99];
			int r_count = 0;
			int i;
			for(i=0; i<amount; i++){
				char msg[100] = "Select a card to exile.";
				test_definition_t this_test;
				new_default_test_definition(&this_test, TYPE_ANY, msg);
				this_test.no_shuffle = 1;
				int crd = new_global_tutor(player, instance->targets[0].player, TUTOR_FROM_DECK, TUTOR_RFG, 0, AI_MAX_VALUE, &this_test);
				if( crd >= 0 ){
					removed[r_count] = get_internal_card_id_from_csv_id(crd);
					r_count++;
				}
			}
			if( 1-instance->targets[0].player == HUMAN && r_count > 0 ){
				show_deck( player, removed, r_count, "Cards exiled by Nightmare Incursion", 0, gs_done_008b40e0 );
			}
			shuffle(instance->targets[0].player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_nightsky_mimic(int player, int card, event_t event){
/* CARD_ID_NIGHTSKY_MIMIC	9833
Nightsky Mimic	|1|WB
Creature - Shapeshifter 2/1
Whenever you cast a spell that's both |Swhite and |Sblack, ~ has base power and toughness 4/4 until end of turn and gains flying until end of turn. */

	return mimic(player, card, event, COLOR_TEST_WHITE, COLOR_TEST_BLACK, 4, 4, KEYWORD_FLYING, 0);
}

int card_nip_gwyllion(int player, int card, event_t event){
/* CARD_ID_NIP_GWYLLION	9834
Nip Gwyllion	|WB
Creature - Hag 1/1
Lifelink (Damage dealt by this creature also causes you to gain that much life.) */

	lifelink(player, card, event);
	return hybrid(player, card, event);
}

int card_nobilis_of_war(int player, int card, event_t event){
/* CARD_ID_NOBILIS_OF_WAR	9835
Nobilis of War	|RW|RW|RW|RW|RW
Creature - Spirit Avatar 3/4
Flying
Attacking creatures you control get +2/+0. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_POWER && affected_card_controller == player && is_attacking(affected_card_controller, affected_card) ){
			event_result+=2;
		}
	}

	return hybrid(player, card, event);
}

int card_noggle_hedge_mage(int player, int card, event_t event){
	/* Noggle Hedge-Mage	|2|UR	0x200802c
	 * Creature - Noggle Wizard 2/2
	 * When ~ enters the battlefield, if you control two or more |H1Islands, you may tap two target permanents.
	 * When ~ enters the battlefield, if you control two or more |H1Mountains, you may have ~ deal 2 damage to target player or planeswalker. */
	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){

		card_instance_t *instance = get_card_instance( player, card );

		instance->number_of_targets = 0;
		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_ISLAND)) >= 2 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_PERMANENT);

			int i;
			for(i=0; i<2; i++){
				if( new_pick_target(&td, "TARGET_PERMANENT", i, 0) ){
					state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
				}
				else{
					break;
				}
			}
			for(i=0; i<instance->number_of_targets; i++){
				state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
				tap_card(instance->targets[i].player, instance->targets[i].card);
			}
			instance->number_of_targets = 0;
		}

		if( count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN)) >= 2 ){
			target_definition_t td1;
			default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
			td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

			if( can_target(&td1) && pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER") ){
				damage_target0(player, card, 2);
			}
			instance->number_of_targets = 0;
		}
	}

	return hybrid(player, card, event);
}

int card_noxious_hatchling(int player, int card, event_t event){
	/* Noxious Hatchling	|3|BG
	 * Creature - Elemental 6/6
	 * ~ enters the battlefield with four -1/-1 counters on it.
	 * Wither
	 * Whenever you cast a |Sblack spell, remove a -1/-1 counter from ~.
	 * Whenever you cast a |Sgreen spell, remove a -1/-1 counter from ~. */
	wither(player, card, event);
	return hatchling(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_BLACK);
}

int card_nucklavee(int player, int card, event_t event){
/* CARD_ID_NUCKLAVEE	9841
Nucklavee	|4|UR|UR
Creature - Beast 4/4
When ~ enters the battlefield, you may return target |Sred sorcery card from your graveyard to your hand.
When ~ enters the battlefield, you may return target |Sblue instant card from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( ! graveyard_has_shroud(player) ){
			char msg[100] = "Select a blue instant card.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_INSTANT | TYPE_INTERRUPT, msg);
			this_test.type_flag = F1_NO_CREATURE;
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
			if( new_special_count_grave(player, &this_test) ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			}

			strcpy(msg, "Select a red sorcery card.");
			new_default_test_definition(&this_test, TYPE_SORCERY, msg);
			this_test.type_flag = F1_NO_CREATURE;
			this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
			if( new_special_count_grave(player, &this_test) ){
				new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
			}
		}
	}

	return hybrid(player, card, event);
}

int card_oonas_grace(int player, int card, event_t event){
	/* Oona's Grace	|2|U
	 * Instant
	 * Target player draws a card.
	 * Retrace */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return retrace(player, card, event, CARD_ID_OONAS_GRACE);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			draw_cards(instance->targets[0].player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_overbeing_of_myth(int player, int card, event_t event)
{
  /* Overbeing of Myth	|GU|GU|GU|GU|GU
   * Creature - Spirit Avatar 100/100
   * ~'s power and toughness are each equal to the number of cards in your hand.
   * At the beginning of your draw step, draw an additional card. */

  if ((uint32_t)(event - EVENT_POWER) < 2)
  {
    if (affect_me(player, card) && player != -1)
      set_characteristic_pt(hand_count[player]);
  }
  else if (event == EVENT_DRAW_PHASE && current_turn == player &&
           !is_humiliated(player, card, AL_TRIG))
    ++event_result;
  return hybrid(player, card, event);
}

int card_patrol_signaler(int player, int card, event_t event){
	/* Patrol Signaler	|1|W
	 * Creature - Kithkin Soldier 1/1
	 * |1|W, |Q: Put a 1/1 |Swhite Kithkin Soldier creature token onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		generate_token_by_id(player, card, CARD_ID_KITHKIN_SOLDIER);
	}

	return generic_activated_ability(player, card, event, GAA_TAPPED, MANACOST_XW(1, 1), 0, NULL, NULL);
}

int card_primalcrux(int player, int card, event_t event)
{
  /* Primalcrux	|G|G|G|G|G|G
   * Creature - Elemental 100/100
   * Trample
   * Chroma - ~'s power and toughness are each equal to the number of |Sgreen mana symbols in the mana costs of permanents you control. */

  int newval;
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    newval = chroma(player, -1, COLOR_GREEN, 0);
    set_characteristic_pt(newval);
  }
  return 0;
}

int card_puncture_blast(int player, int card, event_t event){
	/* Puncture Blast	|2|R	0x200804f
	 * Instant
	 * Wither
	 * ~ deals 3 damage to any target. */

  if (!IS_GS_EVENT(player, card, event))
	return 0;

  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

  // As a non-permanent, this never gets EVENT_ABILITIES, and so wither() won't ever do anything.
  card_instance_t* instance = get_card_instance(player, card);
  instance->ext->sp_keywords |= SP_KEYWORD_WITHER;

  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (valid_target(&td))
		damage_target0(player, card, 3);

	  kill_card(player, card, KILL_DESTROY);
	}

  return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_phyrric_revival(int player, int card, event_t event){
/* CARD_ID_PYRRHIC_REVIVAL	9849
Pyrrhic Revival	|3|WB|WB|WB
Sorcery
Each player returns each creature card from his or her graveyard to the battlefield with an additional -1/-1 counter on it. */

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		APNAP(p,{
					new_reanimate_all(p, -1, p, &test, REANIMATE_MINUS1_MINUS1_COUNTER);
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return hybrid(player, card, event);
}

int card_quillspike(int player, int card, event_t event){
	/* Quillspike	|2|BG
	 * Creature - Beast 1/1
	 * |BG, Remove a -1/-1 counter from a creature you control: ~ gets +3/+3 until end of turn. */

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int max = count_1_1_counters(player, -1);
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_G(1));
		int pump = 0;

		int i;
		for(i=1; i<max; i++){
			if( has_mana_hybrid(player, i, COLOR_BLACK, COLOR_GREEN, c1) ){
				pump += 3;
			}
		}
		return pump;
	}

	if( ! IS_GAA_EVENT(event) ){
		return hybrid(player, card, event);
	}

	target_definition_t td;
	base_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;
	td.special = TARGET_SPECIAL_REQUIRES_COUNTER;
	td.extra = COUNTER_M1_M1;
	td.illegal_abilities = 0;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_G(1));
		if( has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_GREEN, c1) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0,
											&td, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_G(1));
		if( charge_mana_hybrid(player, card, 1, COLOR_BLACK, COLOR_GREEN, c1) ){
			generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST | GAA_LITERAL_PROMPT,
										MANACOST0, 0, &td, "Select a creature you control with a -1/-1 counter.");
			if( spell_fizzled != 1 ){
				remove_counters(player, card, instance->targets[0].player, instance->targets[0].card, COUNTER_M1_M1, 1);
				instance->number_of_targets = 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 3, 3);
	}

	return 0;
}

// goblin soldier token --> rhino token.

int card_ravens_crime(int player, int card, event_t event){
	/* Raven's Crime	|B
	 * Sorcery
	 * Target player discards a card.
	 * Retrace */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return retrace(player, card, event, CARD_ID_RAVENS_CRIME);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			discard(instance->targets[0].player, 0, player);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_regal_force(int player, int card, event_t event){
/* CARD_ID_REGAL_FORCE	9854
Regal Force	|4|G|G|G
Creature - Elemental 5/5
When ~ enters the battlefield, draw a card for each |Sgreen creature you control. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

		int amount = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &test);

		draw_cards(player, amount);
	}

	return 0;
}

int card_rekindle_the_flame(int player, int card, event_t event){
	/* Rekindled Flame	|2|R|R	0x200805e
	 * Sorcery
	 * ~ deals 4 damage to any target.
	 * At the beginning of your upkeep, if an opponent has no cards in hand, you may return ~ from your graveyard to your hand. */
	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		int choice = 1;
		if( hand_count[1-player] < 1 ){
			choice = do_dialog(player, player, card, -1, -1," Return Rekindle the Flame to hand\n Pass", 0);
		}
		if( choice == 0 ){
			instance->state &= ~STATE_INVISIBLE;
			hand_count[player]++;
			return -1;
		}
		else{
			return -2;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x236d3e0; original cards/eventide.c:3281. */
int card_rendclaw_trow(int player, int card, event_t event)
{
  /* Rendclaw Trow	|2|BG
   * Creature - Troll 2/2
   * Wither (This deals damage to creatures in the form of -1/-1 counters.)
   * Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

  wither(player, card, event);
  persist(player, card, event);
  return hybrid(player, card, event);
}

int card_restless_apparition(int player, int card, event_t event){
/* CARD_ID_RESTLESS_APPARITION	9857
Restless Apparition	|WB|WB|WB
Creature - Spirit 2/2
|WB|WB|WB: ~ gets +3/+3 until end of turn.
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

	persist(player, card, event);

	if( event == EVENT_POW_BOOST || event == EVENT_TOU_BOOST ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_G(1));
		int pump = 0;
		int i;
		for(i=1; i<99; i++){
			if( has_mana_hybrid(player, 3*i, COLOR_BLACK, COLOR_WHITE, c1) ){
				pump += 3;
			}
			else{
				break;
			}
		}
		return pump;
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(3));
		if( has_mana_hybrid(player, 3, COLOR_BLACK, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(3));
		if( charge_mana_hybrid(player, card, 3, COLOR_BLACK, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return generic_shade_merge_pt(player, card, event, 0, MANACOST_B(3), 3, 3);
	}

	return hybrid(player, card, event);
}

int card_rise_of_the_hobgoblins(int player, int card, event_t event){
	/* Rise of the Hobgoblins	|RW|RW
	 * Enchantment
	 * When ~ enters the battlefield, you may pay |X. If you do, put X 1/1 |Sred and |Swhite Goblin Soldier creature tokens onto the battlefield.
	 * |RW: |SRed creatures and |Swhite creatures you control gain first strike until end of turn. */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return hybrid(player, card, event);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		charge_mana(player, COLOR_COLORLESS, -1);
		if( spell_fizzled != 1 ){
			generate_tokens_by_id(player, card, CARD_ID_GOBLIN_SOLDIER, x_value);
		}
	}

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(1));
		if( has_mana_hybrid(player, 1, COLOR_RED, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if(event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(1));
		if( charge_mana_hybrid(player, card, 1, COLOR_RED, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED) | get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		pump_creatures_until_eot(instance->parent_controller, instance->parent_card, player, 0, 0, 0, KEYWORD_FIRST_STRIKE, 0, &this_test);
	}

	return global_enchantment(player, card, event);
}

int card_riverfall_mimic(int player, int card, event_t event){
/* CARD_ID_RIVERFALL_MIMIC	9859
Riverfall Mimic	|1|UR
Creature - Shapeshifter 2/1
Whenever you cast a spell that's both |Sblue and |Sred, ~ has base power and toughness 3/3 until end of turn and can't be blocked this turn. */

	return mimic(player, card, event, COLOR_TEST_RED, COLOR_TEST_BLUE, 3, 3, 0, SP_KEYWORD_UNBLOCKABLE);
}

int card_rugged_prarie(int player, int card, event_t event){
/* CARD_ID_RUGGED_PRAIRIE	9860
Rugged Prairie	""
Land
|T: Add |C to your mana pool.
|RW, |T: Add |R|R, |R|W, or |W|W to your mana pool. */

	return card_filter_land(player, card, event, COLOR_WHITE, COLOR_RED, COLOR_TEST_WHITE, COLOR_TEST_RED, " Colorless\n W -> WW\n W -> WR\n W -> RR\n R -> WW\n R -> WR\n R -> RR\n Cancel");
}

int card_sanity_grinding(int player, int card, event_t event){
/* CARD_ID_SANITY_GRINDING	9861
Sanity Grinding	|U|U|U
Sorcery
Chroma - Reveal the top ten cards of your library.
For each |Sblue mana symbol in the mana costs of the revealed cards, target opponent puts the top card of his or her library into his or her graveyard.
Then put the cards you revealed this way on the bottom of your library in any order. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int to_reveal = MIN(10, count_deck(player));
			if( to_reveal > 0 ){
				int *deck = deck_ptr[player];
				int result = 0;
				int count = 0;
				while( deck[count] != -1 && count < to_reveal ){
						result+=count_chroma(-1, deck[count], COLOR_BLUE, 0, 0);
						count++;
				}
				show_deck( HUMAN, deck, to_reveal, "Cards revealed with Sanity Grinding", 0, gs_done_008b40e0 );
				put_top_x_on_bottom(player, player, to_reveal);
				if( result > 0 ){
					mill(instance->targets[0].player, result);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_ONLY_TARGET_OPPONENT, &td, NULL, 1, NULL);
}

int card_sapling_of_colfenor(int player, int card, event_t event){
	/* Sapling of Colfenor	|3|BG|BG
	 * Legendary Creature - Treefolk Shaman 2/5
	 * Indestructible
	 * Whenever ~ attacks, reveal the top card of your library. If it's a creature card, you gain life equal to that card's toughness, lose life equal to its power, then put it into your hand. */

	indestructible(player, card, event);

	check_legend_rule(player, card, event);

	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
	if (event == EVENT_DAT_ABILITY){
		int iid = deck_ptr[player][0];
		if( iid != -1 ){
			reveal_card_iid(player, card, iid);
			if( cards_data[iid].type & TYPE_CREATURE ){
				int pwr = get_base_power_iid(player, iid);
				int tou = get_base_toughness_iid(player, iid);

				gain_life(player, tou);
				lose_life(player, pwr);
				add_card_to_hand(player, iid);
				remove_card_from_deck(player, 0);
			}
		}
	}

	return hybrid(player, card, event);
}

int card_savage_conception(int player, int card, event_t event){
	/* Savage Conception	|3|G|G
	 * Sorcery
	 * Put a 3/3 |Sgreen Beast creature token onto the battlefield.
	 * Retrace */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return retrace(player, card, event, CARD_ID_SAVAGE_CONCEPTION);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		generate_token_by_id(player, card, CARD_ID_BEAST);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, 0, NULL, NULL, 1, NULL);
}

int card_scarecrone(int player, int card, event_t event){
  /* Scarecrone |3
   * 	  Artifact Creature - Scarecrow 1/2
   * 	  {1}, Sacrifice a Scarecrow: Draw a card.
   * 	  {4}, {T}: Return target artifact creature card from your graveyard to the battlefield. */

	if (!IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_PERMANENT, "Select an artifact creature to reanimate.");
	this_test.special_selection_function = &is_artifact_creature_by_internal_id;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) &&
			can_sacrifice(player, player, 1, TYPE_PERMANENT, SUBTYPE_SCARECROW)
		  ){
			return 1;
		}
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL) ){
			if( new_special_count_grave(player, &this_test) > 0 && ! graveyard_has_shroud(player) ){
				return 1;
			}
		}
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = instance->number_of_targets = 0;
		int abilities[2] = {	generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, MANACOST_X(1), 0, NULL, NULL) &&
								can_sacrifice(player, player, 1, TYPE_PERMANENT, SUBTYPE_SCARECROW),
								generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL) &&
								new_special_count_grave(player, &this_test) > 0 && ! graveyard_has_shroud(player)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
						"Sac & draw", abilities[0], 5,
						"Reanimate an artifact creature", abilities[1], 15);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(choice == 1 ? 1 : 4)) ){
			if( choice == 1 ){
				test_definition_t this_test2;
				new_default_test_definition(&this_test2, TYPE_PERMANENT, "Select a Scarecrow to sacrifice.");
				this_test2.subtype = SUBTYPE_SCARECROW;

				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK|SAC_AS_COST|SAC_RETURN_CHOICE, &this_test2);
				if (!sac){
					spell_fizzled = 1;
				}
				else{
					kill_card(BYTE2(sac), BYTE3(sac), KILL_SACRIFICE);
				}
			}

			if( choice == 2 ){
				if( select_target_from_grave_source(player, card, player, 0, AI_MAX_CMC, &this_test, 0) != -1 ){
					tap_card(player, card);
				}
				else{
					spell_fizzled = 1;
				}
			}
			if( spell_fizzled != 1 ){
				instance->info_slot = choice;
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == 1 ){
			draw_cards(player, 1);
		}
		if( instance->info_slot == 2 ){
			int selected = validate_target_from_grave_source(player, card, player, 0);
			if( selected != -1 ){
				reanimate_permanent(player, card, player, selected, REANIMATE_DEFAULT);
			}
		}
	}

	return 0;
}

int card_scourge_of_the_nobilis(int player, int card, event_t event){
  /* Scourge of the Nobilis |2|{R/W}
   * Enchantment — Aura
   * Enchant creature
   * As long as enchanted creature is red, it gets +1/+1 and has "{R/W}: This creature gets +1/+0 until end of turn."
   * As long as enchanted creature is white, it gets +1/+1 and has lifelink. (Damage dealt by the creature also causes its controller to gain that much life.) */

	hybrid(player, card, event);

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player != - 1 && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance( player, card );

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		int clr = get_color(p, c);

		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) ){
			int boost = (clr & get_sleighted_color_test(player, card, COLOR_TEST_RED)) ? 1 : 0;
			boost += (clr & get_sleighted_color_test(player, card, COLOR_TEST_WHITE)) ? 1 : 0;
			event_result += boost;
		}

		if( event == EVENT_ABILITIES && affect_me(p, c) ){
			if( clr & get_sleighted_color_test(player, card, COLOR_TEST_WHITE) ){
				lifelink(player, card, event);
			}
		}
		if( IS_GAA_EVENT(event) ){
			if( event == EVENT_CAN_ACTIVATE && (clr & get_sleighted_color_test(player, card, COLOR_TEST_RED)) ){
				int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(1));
				if( has_mana_hybrid(player, 1, COLOR_RED, COLOR_WHITE, c1) ){
					return granted_generic_activated_ability(player, card, p, c, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0,
															0, NULL, NULL);
				}
			}

			if(event == EVENT_ACTIVATE ){
				int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_R(1));
				if( charge_mana_hybrid(player, card, 1, COLOR_RED, COLOR_WHITE, c1) ){
					return granted_generic_activated_ability(player, card, p, c, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0,
															0, NULL, NULL);
				}
			}

			if( event == EVENT_RESOLVE_ACTIVATION ){
				pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card, p, c, 1, 0);
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }
	return vanilla_aura(player, card, event, player);
}

int card_selkie_hedge_mage(int player, int card, event_t event){
/* CARD_ID_SELKIE_HEDGE_MAGE	9866
Selkie Hedge-Mage	|2|GU
Creature - Merfolk Wizard 2/2
When ~ enters the battlefield, if you control two or more |H1Forests, you may gain 3 life.
When ~ enters the battlefield, if you control two or more |H1Islands, you may return target tapped creature to its owner's hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		if( count_subtype(player, TYPE_LAND, get_sleighted_color_test(player, card, SUBTYPE_ISLAND)) >= 2){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_CREATURE);
			td.required_state = TARGET_STATE_TAPPED;

			if( can_target(&td) && new_pick_target(&td, "Select target tapped creature.", 0, GS_LITERAL_PROMPT) ){
				card_instance_t *instance = get_card_instance( player, card );
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
		}
		if( count_subtype(player, TYPE_LAND, get_sleighted_color_test(player, card, SUBTYPE_FOREST)) >= 2 ){
			int choice = duh_mode(player) ? 0 : do_dialog(player, player, card, -1, -1, " Gain 3 life\n Pass", 0);
			if( ! choice ){
				gain_life(player, 3);
			}
		}
	}

	return hybrid(player, card, event);
}

int card_shell_skulkin(int player, int card, event_t event){
/* CARD_ID_SHELL_SKULKIN	9867
Shell Skulkin	|4
Artifact Creature - Scarecrow 3/2
|3: Target |Sblue creature gains shroud until end of turn. (It can't be the target of spells or abilities.) */

	return skulkin(player, card, event, 3, COLOR_TEST_BLUE, 0, 0, KEYWORD_SHROUD, 0);
}

int card_shorecrasher_mimic(int player, int card, event_t event){
/* CARD_ID_SHORECRASHER_MIMIC	9868
Shorecrasher Mimic	|1|GU
Creature - Shapeshifter 2/1
Whenever you cast a spell that's both |Sgreen and |Sblue, ~ has base power and toughness 5/3 until end of turn and gains trample until end of turn. */

	return mimic(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_BLUE, 5, 3, KEYWORD_TRAMPLE, 0);
}

int card_shrewd_hatchling(int player, int card, event_t event){
/* Shrewd Hatchling	|3|UR
 * Creature - Elemental 6/6
 * ~ enters the battlefield with four -1/-1 counters on it.
 * |UR: Target creature can't block ~ this turn.
 * Whenever you cast a |Sblue spell, remove a -1/-1 counter from ~.
 * Whenever you cast a |Sred spell, remove a -1/-1 counter from ~. */

	if( IS_GAA_EVENT(event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);

		card_instance_t *instance = get_card_instance(player, card);

		if( event == EVENT_CAN_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1));
			if( has_mana_hybrid(player, 1, COLOR_BLUE, COLOR_RED, c1) ){
				return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0,
												&td, NULL);
			}
		}

		if(event == EVENT_ACTIVATE ){
			int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_U(1));
			if( charge_mana_hybrid(player, card, 1, COLOR_BLUE, COLOR_RED, c1) ){
				return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0,
												&td, "TARGET_CREATURE");
			}
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( valid_target(&td) ){
				int p = instance->parent_controller, c = instance->parent_card;
				creature1_cant_block_creature2_until_eot(p, c, instance->targets[0].player, instance->targets[0].card, p, c);
			}
		}
	}

	return hatchling(player, card, event, COLOR_TEST_RED, COLOR_TEST_BLUE);
}

int card_slippery_bogle(int player, int card, event_t event){
/* CARD_ID_SLIPPERY_BOGLE	9870
Slippery Bogle	|GU
Creature - Beast 1/1
Hexproof (This creature can't be the target of spells or abilities your opponents control.) */

  hexproof(player, card, event);
  return hybrid(player, card, event);
}

int card_snakeform(int player, int card, event_t event){
  /* Snakeform |2|{G/U}
   *    Instant
   *    Target creature loses all abilities and becomes a 1/1 green Snake until end of turn.
   *    Draw a card. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( hybrid_casting(player, card, 0) ){
			pick_target(&td, "TARGET_CREATURE");
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t hc;
			default_test_definition(&hc, 0);
			hc.power = 1;
			hc.toughness = 1;
			hc.subtype = SUBTYPE_SNAKE;
			hc.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
			humiliate_and_set_pt_abilities(player, card, instance->targets[0].player, instance->targets[0].card, 4, &hc);

			draw_cards(player, 1);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_soot_imp(int player, int card, event_t event){
/* CARD_ID_SOOT_IMP	9873
Soot Imp	|1|B|B
Creature - Imp 1/2
Flying
Whenever a player casts a non|Sblack spell, that player loses 1 life. */

	if (trigger_condition == TRIGGER_SPELL_CAST && affect_me(player, card)){
		test_definition_t test;
		default_test_definition(&test, TYPE_ANY);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		test.color_flag = DOESNT_MATCH;
		sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_NONE, &test, player, card);
	}
	if (event == EVENT_SPA){
		lose_life(trigger_cause_controller, 1);
	}
	return 0;
}

int card_soul_snuffers(int player, int card, event_t event){
	/* Soul Snuffers	|2|B|B
	 * Creature - Elemental Shaman 3/3
	 * When ~ enters the battlefield, put a -1/-1 counter on each creature. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		manipulate_type(player, card, ANYBODY, TYPE_CREATURE, ACT_ADD_COUNTERS(COUNTER_M1_M1, 1));
	}

	return 0;
}

int card_spirit_of_the_hearth(int player, int card, event_t event){
/* CARD_ID_SPIRIT_OF_THE_HEARTH	9876
Spirit of the Hearth	|4|W|W
Creature - Cat Spirit 4/5
Flying
You have hexproof. (You can't be the target of spells or abilities your opponents control.) */
   if( in_play(player, card) && !is_humiliated(player, card, AL_STATIC) ){
	    give_hexproof_to_player(player, card, event);
	}

	return 0;
}

int card_spitemare(int player, int card, event_t event){
	/* Spitemare	|2|RW|RW	0x200ba06
	 * Creature - Elemental 3/3
	 * Whenever ~ is dealt damage, it deals that much damage to any target. */
	// Used externally
	hybrid(player, card, event);

	if( ! is_humiliated(player, card, AL_TRIG) ){
	  card_instance_t* damage = damage_being_dealt(event);
	  if (damage
		  && damage->damage_target_card == card && damage->damage_target_player == player)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (instance->info_slot < 10)
			{
			  if (instance->info_slot < 1)
				instance->info_slot = 1;

			  instance->targets[instance->info_slot++].player = damage->info_slot;
			}
		}

	  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (instance->info_slot > 1)
			{
			  if (event == EVENT_TRIGGER)
				event_result |= RESOLVE_TRIGGER_MANDATORY;

			  if (event == EVENT_RESOLVE_TRIGGER)
				{
				  target_definition_t td;
				  default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
				  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
				  td.allow_cancel = 0;

				  char prompt[100];
				  prompt[0] = 0;

				  // Free up info_slot and targets, just in case something triggers instantly (and improperly) which eventually results in dealing damage back.
				  int i, dam[10];
				  for (i = 1; i < instance->info_slot; ++i)
					dam[i] = instance->targets[i].player;

				  int numdam = instance->info_slot;
				  instance->info_slot = 1;

				  for (i = 1; i < numdam; ++i)
					{
					  if (ai_is_speculating != 1)
						{
						  load_text(0, "TARGET_ANY");
						  scnprintf(prompt, 100, "%s (%d damage)", text_lines[0], dam[i]);
						}
					  instance->number_of_targets = 0;
					  if (can_target(&td) && pick_next_target_noload(&td, prompt))
						damage_target0(player, card, dam[i]);
					}
				}
			}
		}
	}

	return 0;
}

int card_spitting_image(int player, int card, event_t event){
	/* Spitting Image	|4|GU|GU
	 * Sorcery
	 * Put a token that's a copy of target creature onto the battlefield.
	 * Retrace */

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_GRAVEYARD_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.zone = TARGET_ZONE_HAND;

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );

		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		if( has_mana_hybrid(player, 2, COLOR_BLUE, COLOR_GREEN, cless) &&
			check_battlefield_for_special_card(player, card, player, 0, &this_test) &&
			can_sorcery_be_played(player, event)
		  ){
			return can_target(&td) ? GA_PLAYABLE_FROM_GRAVE : 0;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		charge_mana_hybrid(player, card, 2, COLOR_BLUE, COLOR_GREEN, cless);
		if( spell_fizzled != 1 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to discard.");
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				discard_card(player, selected);
				return GAPAID_REMOVE;
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( IS_GS_EVENT(player, card, event) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE );

		if( event == EVENT_CAN_CAST ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
		}

		if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}

		if( event == EVENT_RESOLVE_SPELL ){
			if( valid_target(&td) ){
				copy_token(player, card, instance->targets[0].player, instance->targets[0].card);
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}

	return hybrid(player, card, event);
}

int card_springjack_pasture(int player, int card, event_t event)
{
  /* Springjack Pasture	""
   * Land
   * |T: Add |1 to your mana pool.
   * |4, |T: Put a 0/1 |Swhite Goat creature token onto the battlefield.
   * |T, Sacrifice X Goats: Add X mana of any one color to your mana pool. You gain X life. */

  if (event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE || event == EVENT_RESOLVE_ACTIVATION || event == EVENT_RESOLVE_SPELL
	  || (event == EVENT_COUNT_MANA && affect_me(player, card)) || (event == EVENT_CHANGE_TYPE && affect_me(player, card)))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goat to sacrifice.");
	  test.subtype = SUBTYPE_GOAT;

	  int can_common = !is_tapped(player, card) && !is_animated_and_sick(player, card);
	  int can_mana = can_common && can_produce_mana(player, card);
	  int num_goats = max_can_sacrifice_as_cost(player, card, &test);

	  if (event == EVENT_RESOLVE_SPELL)
		{
		  play_land_sound_effect_force_color(player, card, num_goats ? COLOR_TEST_ANY : COLOR_TEST_COLORLESS);
		  return 0;
		}

	  if (event == EVENT_CHANGE_TYPE)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  if (num_goats)
			instance->info_slot |= (1<<30);	// bearing in mind that DIALOG uses byte0
		  else
			instance->info_slot &= ~(1<<30);
		  return 0;
		}

	  if (event == EVENT_COUNT_MANA)
		{
		  if (num_goats)
			declare_mana_available_hex(player, COLOR_TEST_ANY_COLORED, num_goats);
		  else if (can_mana)
			declare_mana_available(player, COLOR_COLORLESS, 1);
		  return 0;
		}

	  // EVENT_CAN_ACTIVATE/EVENT_ACTIVATE/EVENT_RESOLVE_ACTIVATION.

	  int can_make_goat = (can_common && !paying_mana() && can_use_activated_abilities(player, card)
						   && has_mana_for_activated_ability(player, card, MANACOST_X(4 + (can_mana ? MAX(num_goats, 1) : 0))));

	  enum
	  {
		CHOICE_MANA = 1,
		CHOICE_MAKE_GOAT,
		CHOICE_SAC_GOATS,
	  } choice = DIALOG(player, card, event,
						DLG_RANDOM,
						"Produce 1", can_mana, paying_mana() ? 2 : -1,
						"Make a Goat", can_make_goat, 4,
						"Sacrifice Goats", can_mana, paying_mana() ? num_goats : (1 + num_goats) / 2);

	  if (event == EVENT_CAN_ACTIVATE)
		return choice;
	  else if (event == EVENT_ACTIVATE)
		switch (choice)
		  {
			case CHOICE_MANA:
			  produce_mana_tapped(player, card, COLOR_COLORLESS, 1);
			  break;

			case CHOICE_MAKE_GOAT:
			  add_state(player, card, STATE_TAPPED);
			  if (!charge_mana_for_activated_ability(player, card, MANACOST_X(4)))
				remove_state(player, card, STATE_TAPPED);
			  tapped_for_mana_color = -1;
			  break;

			case CHOICE_SAC_GOATS:
			  if (num_goats)	// and therefore can sacrifice at least one as cost
				{
				  char marked[2][151] = {{0}};

				  target_definition_t td;
				  default_target_definition(player, card, &td, TYPE_PERMANENT);
				  td.allowed_controller = td.preferred_controller = player;
				  if (check_battlefield_for_id(ANYBODY, CARD_ID_ANGEL_OF_JUBILATION))
					td.illegal_type = TYPE_CREATURE;
				  td.required_subtype = SUBTYPE_GOAT;
				  td.allow_cancel = 3;	//done and cancel buttons

				  int num_roasted = mark_up_to_n_targets_noload(&td, "Select a Goat to sacrifice.", -1, marked);
				  if (cancel == 1)
					return 0;

				  if (num_roasted > 0)
					{
					  int c;
					  for (c = 0; c < active_cards_count[player]; ++c)
						if (marked[player][c])
						  kill_card(player, c, KILL_SACRIFICE);

					  gain_life(player, num_roasted);
					}

				  FORCE(produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, num_roasted));
				}
			  else
				produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 0);
			  break;
		  }
	  else if (choice == CHOICE_MAKE_GOAT)	// and event == EVENT_RESOLVE_ACTIVATION
		generate_token_by_id(player, card, CARD_ID_GOAT);
	}

  return 0;
}

/* ManalinkEh 2022: 0x23700f0; original cards/eventide.c:4086. */
int card_springjack_shepherd(int player, int card, event_t event)
{
  /* Springjack Shepherd	|3|W
   * Creature - Kithkin Wizard 1/2
   * Chroma - When this creature enters, create a 0/1 white Goat creature token for each white mana symbol in the mana costs of permanents you control. */

  int result;
  result = player;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  result = chroma(player, -1, COLOR_WHITE, 0);
  generate_tokens_by_id(player, card, CARD_ID_GOAT, result);
  return 0;
}

static int stigma_lasher_legacy(int player, int card, event_t event){

	if( event == EVENT_STATIC_EFFECTS && affect_me(player, card) )
	{
		int aff = get_card_instance(player, card)->targets[0].player;
		if( aff > -1 ){
			player_bits[aff] |= PB_CANT_GAIN_LIFE;
		}
	}

	return 0;
}

int card_stigma_lasher(int player, int card, event_t event){
  /* Stigma Lasher |R|R
   * Creature — Elemental Shaman 2/2
   * Wither (This deals damage to creatures in the form of -1/-1 counters.)
   * Whenever Stigma Lasher deals damage to a player, that player can't gain life for the rest of the game. */

	wither(player, card, event);

	if (damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		int found[2] = {0};
		CYCLE_ALL_CARDS(p, c, 	{
									if( in_play(p, c) && is_what(p, c, TYPE_EFFECT) ){
										card_instance_t *inst = get_card_instance(p, c);
										if( inst->info_slot == (intptr_t)stigma_lasher_legacy ){
											found[p] = 1;
										}
									}
								};
		);
		APNAP(p,{
					if( ! found[p] && times_damaged[p] ){
						int legacy = create_legacy_effect(player, card, stigma_lasher_legacy);
						get_card_instance(player, legacy)->targets[0].player = p;
					}
				};
		);
	}

	return 0;
}

int card_stillmoon_cavalier(int player, int card, event_t event){
  /* Stillmoon Cavalier	|1|WB|WB
   * Creature - Zombie Knight 2/1
   * Protection from white and from black
   * |WB: This creature gains flying until end of turn.
   * |WB: This creature gains first strike until end of turn.
   * |WB|WB: This creature gets +1/+0 until end of turn. */

/*
Stillmoon Cavalier 1{W/B}{W/B}
Creature - Zombie Knight 2/1
Protection from white and from black
{W/B}: Stillmoon Cavalier gains flying until end of turn.
{W/B}: Stillmoon Cavalier gains first strike until end of turn.
{W/B}{W/B}: Stillmoon Cavalier gets +1/+0 until end of turn.
*/
	hybrid(player, card, event);

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(1));
		if( has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t *instance = get_card_instance( player, card);

		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(1));
		int abilities[3] = {	has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_WHITE, c1),
								has_mana_hybrid(player, 1, COLOR_BLACK, COLOR_WHITE, c1),
								has_mana_hybrid(player, 2, COLOR_BLACK, COLOR_WHITE, c1)
		};
		int priorities[3] = {
							current_phase < PHASE_DECLARE_BLOCKERS && ! check_for_ability(player, card, KEYWORD_FLYING) ? 10 : 0,
							current_phase == PHASE_AFTER_BLOCKING && ! check_for_ability(player, card, KEYWORD_FIRST_STRIKE) ? 15 : 0,
							current_phase == PHASE_AFTER_BLOCKING ? 10 : 0,
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Gain flying", abilities[0], priorities[0],
							"Gain first strike", abilities[1], priorities[1],
							"+1/+0", abilities[2], priorities[2]);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_hybrid(player, card, choice == 3 ? 2 : 1, COLOR_BLACK, COLOR_WHITE, c1) ){
			instance->info_slot = choice;
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card);

		if( instance->info_slot == 1 ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, KEYWORD_FLYING, 0);
		}
		if( instance->info_slot == 2 ){
			pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
									0, 0, KEYWORD_FIRST_STRIKE, 0);
		}
		if( instance->info_slot == 3 ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, 0);
		}
	}

	return 0;
}

int card_sturdy_hatchling(int player, int card, event_t event){

	/* Sturdy Hatchling	|3|GU
	 * Creature - Elemental 6/6
	 * ~ enters the battlefield with four -1/-1 counters on it.
	 * |GU: ~ gains shroud until end of turn.
	 * Whenever you cast a |Sgreen spell, remove a -1/-1 counter from ~.
	 * Whenever you cast a |Sblue spell, remove a -1/-1 counter from ~. */

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(1));
		if( has_mana_hybrid(player, 1, COLOR_BLUE, COLOR_GREEN, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(1));
		if( charge_mana_hybrid(player, card, 1, COLOR_BLUE, COLOR_GREEN, c1) ){
			return generic_activated_ability(player, card, event, GAA_DO_NOT_RECALCULATE_ACTIVATION_COST, MANACOST0, 0, NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
								0, 0, KEYWORD_SHROUD, 0);
	}

	return hatchling(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_BLUE);
}

int card_suture_spirit(int player, int card, event_t event){
/* CARD_ID_SUTURE_SPIRIT	9885
Suture Spirit	|1|W
Creature - Spirit 1/1
Flying
|WB|WB|WB: Regenerate target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.required_state = TARGET_STATE_DESTROYED;
	td.preferred_controller = player;
	if( player == AI ){
		td.special = TARGET_SPECIAL_REGENERATION;
	}

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(3));
		if( has_mana_hybrid(player, 3, COLOR_BLACK, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_REGENERATION | GAA_DO_NOT_RECALCULATE_ACTIVATION_COST,
											MANACOST0, 0, &td, NULL);
		}
	}

	if( event == EVENT_ACTIVATE ){
		int c1 = get_cost_mod_for_activated_abilities(player, card, MANACOST_B(3));
		if( charge_mana_hybrid(player, card, 3, COLOR_BLACK, COLOR_WHITE, c1) ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_REGENERATION |
											GAA_DO_NOT_RECALCULATE_ACTIVATION_COST | GAA_LITERAL_PROMPT, MANACOST0, 0, &td,
											"Select target creature to regenerate.");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}

	return 0;
}

int card_syphon_life(int player, int card, event_t event){
	/* Syphon Life	|1|B|B
	 * Sorcery
	 * Target player loses 2 life and you gain 2 life.
	 * Retrace */

	if( event == EVENT_GRAVEYARD_ABILITY || event == EVENT_PAY_FLASHBACK_COSTS ){
		return retrace(player, card, event, CARD_ID_SYPHON_LIFE);
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL){
		if( valid_target(&td) ){
			lose_life(instance->targets[0].player, 2);
			gain_life(player, 2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_talaras_battalion(int player, int card, event_t event){
/* CARD_ID_TALARAS_BATTALION	9889
Talara's Battalion	|1|G
Creature - Elf Warrior 4/3
Trample
Cast ~ only if you've cast another |Sgreen spell this turn. */

	if( event == EVENT_MODIFY_COST && get_stormcolor_count(player, COLOR_GREEN) < 1 ){
		infinite_casting_cost();
	}

	return 0;
}

int card_thunderblust(int player, int card, event_t event){
/* CARD_ID_THUNDERBLUST	9891
Thunderblust	|2|R|R|R
Creature - Elemental 7/2
Haste
~ has trample as long as it has a -1/-1 counter on it.
Persist (When this creature dies, if it had no -1/-1 counters on it, return it to the battlefield under its owner's control with a -1/-1 counter on it.) */

	haste(player, card, event);

	persist(player, card, event);

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_ABILITIES && affect_me(player, card) && count_minus1_minus1_counters(player, card) ){
			event_result |= KEYWORD_TRAMPLE;
		}
	}

	return 0;
}

int card_tilling_treefolk(int player, int card, event_t event)
{
  /* Tilling Treefolk	|2|G
   * Creature - Treefolk Druid 1/3
   * When ~ enters the battlefield, you may return up to two target land cards from your graveyard to your hand. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( ! graveyard_has_shroud(player) )
		{
			test_definition_t test;
			new_default_test_definition(&test, TYPE_LAND, "Select target land card.");
			test.qty = 2;

			new_global_tutor(player, player, TUTOR_FROM_GRAVE, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		}
	}

	return 0;
}

int card_twilight_mire(int player, int card, event_t event){
/* CARD_ID_TWILIGHT_MIRE	9894
Twilight Mire	""
Land
|T: Add |C to your mana pool.
|BG, |T: Add |B|B, |B|G, or |G|G to your mana pool. */
	return card_filter_land(player, card, event, COLOR_BLACK, COLOR_GREEN, COLOR_TEST_BLACK, COLOR_TEST_GREEN, " Colorless\n B -> BB\n B -> BG\n B -> GG\n G -> BB\n G -> BG\n G -> GG\n Cancel");
}

int card_twinblade_slasher(int player, int card, event_t event){
/* CARD_ID_TWINBLADE_SLASHER	9895
Twinblade Slasher	|G
Creature - Elf Warrior 1/1
Wither (This deals damage to creatures in the form of -1/-1 counters.)
|1|G: ~ gets +2/+2 until end of turn. Activate this ability only once each turn. */

	wither(player, card, event);
	return card_basking_rootwalla(player, card, event);
}

int card_umbra_stalker(int player, int card, event_t event)
{
  /* CARD_ID_UMBRA_STALKER	9896
   * Umbra Stalker	|4|B|B|B
   * Creature - Elemental 100/100
   * Chroma - ~'s power and toughness are each equal to the number of |Sblack mana symbols in the mana costs of cards in your graveyard. */

  int count;
  int card_00;
  int *grave_cursor;
  int newval;
  if (((((uint32_t)(event - EVENT_POWER) < 2) && (card == affected_card)) &&
       (affected_card_controller == player)) &&
      (player != -1))
  {
    grave_cursor = graveyard_ptr_mutable[player];
    card_00 = *grave_cursor;
    if (card_00 == -1)
    {
      newval = 0;
    }
    else
    {
      newval = 0;
      do
      {
        grave_cursor = grave_cursor + 1;
        count = count_chroma(-1, card_00, 1, 0, 0);
        card_00 = *grave_cursor;
        newval = newval + count;
      } while (card_00 != -1);
    }
    set_characteristic_pt(newval);
  }
  return 0;
}

int card_unmake(int player, int card, event_t event){
/* CARD_ID_UNMAKE	9897
Unmake	|WB|WB|WB
Instant
Exile target creature. */

	modify_cost_for_hybrid_spells(player, card, event, 0);

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
		instance->number_of_targets = 0;
		if( hybrid_casting(player, card, 0) ){
			return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		}
		else{
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_unnerving_assault(int player, int card, event_t event){
/* CARD_ID_UNNERVING_ASSAULT	9898
Unnerving Assault	|2|UR
Instant
Creatures your opponents control get -1/-0 until end of turn if |U was spent to cast ~,
and creatures you control get +1/+0 until end of turn if |R was spent to cast it. (Do both if |U|R was spent.) */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_MODIFY_COST ){
		int c1 = get_updated_casting_cost(player, card, -1, event, 3);
		if( has_mana_hybrid(player, 1, COLOR_BLUE, COLOR_RED, c1) ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	if( event == EVENT_CAN_CAST ){
		return 1;
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int c1 = get_updated_casting_cost(player, card, -1, event, 3);
			test_definition_t test;
			char mana_text[3][100];
			get_mana_from_text(&test, MANACOST_XU(c1, 1));
			strcpy(mana_text[0], test.message);
			get_mana_from_text(&test, MANACOST_XR(c1, 1));
			strcpy(mana_text[1], test.message);
			int can_play_full_effect = 0;
			if( c1 > 0 ){
				if( c1 > 1 ){
					get_mana_from_text(&test, MANACOST_XUR(c1-1, 1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_XUR(c1-1, 1, 1));
				}
				else{
					get_mana_from_text(&test, MANACOST_UR(1, 1));
					strcpy(mana_text[2], test.message);
					can_play_full_effect = has_mana_multi(player, MANACOST_UR(1, 1));
				}
			}
			int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							mana_text[0], has_mana_multi(player, MANACOST_XU(c1, 1)), 5,
							mana_text[1], has_mana_multi(player, MANACOST_XR(c1, 1)), 5,
							mana_text[2], can_play_full_effect, 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
			charge_mana_multi(player, MANACOST_XUR(
													(c1 > 1 ? (choice != 3 ? c1 : c1-1) : (choice != 3 ? c1 : 0)),
													(choice == 1 || choice == 3),
													(choice == 2 || choice == 3)
													)
								);
		}
		if( spell_fizzled != 1 ){
			if( mana_paid[COLOR_BLUE] > 0 ){
				instance->info_slot |= 2;
			}
			if( mana_paid[COLOR_RED] > 0 ){
				instance->info_slot |= 4;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( (instance->info_slot & 2) ){
			pump_subtype_until_eot(player, card, 1-player, -1, -1, 0, 0, 0);
		}
		if( (instance->info_slot & 4) ){
			pump_subtype_until_eot(player, card, player, -1, 1, 0, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_unwilling_recruit(int player, int card, event_t event){
/* CARD_ID_UNWILLING_RECRUIT	9899
Unwilling Recruit	|X|R|R|R
Sorcery
Gain control of target creature until end of turn. Untap that creature. It gets +X/+0 and gains haste until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			effect_act_of_treason_and_modify_pt_or_abilities(player, card, instance->targets[0].player, instance->targets[0].card,
															instance->info_slot, 0, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_X_SPELL, &td, "TARGET_CREATURE", 1, NULL);
}

int card_voracious_hatchling(int player, int card, event_t event){
	/* Voracious Hatchling	|3|WB
	 * Creature - Elemental 6/6
	 * Lifelink
	 * ~ enters the battlefield with four -1/-1 counters on it.
	 * Whenever you cast a |Swhite spell, remove a -1/-1 counter from ~.
	 * Whenever you cast a |Sblack spell, remove a -1/-1 counter from ~. */
	lifelink(player, card, event);
	return hatchling(player, card, event, COLOR_TEST_BLACK, COLOR_TEST_WHITE);
}

int card_wake_thrasher(int player, int card, event_t event){
/* CARD_ID_WAKE_THRASHER	9901
Wake Thrasher	|2|U
Creature - Merfolk Soldier 1/1
Whenever a permanent you control becomes untapped, ~ gets +1/+1 until end of turn. */

  player_bits[player] |= PB_SEND_EVENT_UNTAP_CARD_TO_ALL;

  if (event == EVENT_UNTAP_CARD && affected_card_controller == player && !is_humiliated(player, card, AL_TRIG) && in_play(player, card))
	pump_until_eot_merge_previous(player, card, player, card, 1,1);

  return 0;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_ward_of_bones(int player, int card, event_t event){
  /* Ward of Bones |6
   * Artifact
   * Each opponent who controls more creatures than you can't play creature cards. The same is true for artifacts, enchantments, and lands. */

	if( event == EVENT_MODIFY_COST_GLOBAL && affected_card_controller == 1-player && ! is_humiliated(player, card, AL_STATIC) ){
		if( is_what(affected_card_controller, affected_card, TYPE_CREATURE) ){
			if( count_subtype(player, TYPE_CREATURE, -1) < count_subtype(1-player, TYPE_CREATURE, -1) ){
				infinite_casting_cost();
				return 0;
			}
		}
		if( is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
			if( count_subtype(player, TYPE_ARTIFACT, -1) < count_subtype(1-player, TYPE_ARTIFACT, -1) ){
				infinite_casting_cost();
				return 0;
			}
		}
		if( is_what(affected_card_controller, affected_card, TYPE_ENCHANTMENT) ){
			if( count_subtype(player, TYPE_ENCHANTMENT, -1) < count_subtype(1-player, TYPE_ENCHANTMENT, -1) ){
				infinite_casting_cost();
				return 0;
			}
		}
	}

	return 0;
}

int card_wickerbough_elder(int player, int card, event_t event){
	/* Wickerbough Elder	|3|G
	 * Creature - Treefolk Shaman 4/4
	 * ~ enters the battlefield with a -1/-1 counter on it.
	 * |G, Remove a -1/-1 counter from ~: Destroy target artifact or enchantment. */

	enters_the_battlefield_with_counters(player, card, event, COUNTER_M1_M1, 1);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_G(1), GVC_COUNTER(COUNTER_M1_M1), &td, "DISENCHANT");
}

int card_wistful_selkie(int player, int card, event_t event){
/* CARD_ID_WISTFUL_SELKIE	1419
Wistful Selkie	|GU|GU|GU
Creature - Merfolk Wizard 2/2
When ~ enters the battlefield, draw a card. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		draw_a_card(player);
	}

	return hybrid(player, card, event);
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

int card_woodlurker_mimic(int player, int card, event_t event){
/* CARD_ID_WOODLURKER_MIMIC	9906
Woodlurker Mimic	|1|BG
Creature - Shapeshifter 2/1
Whenever you cast a spell that's both |Sblack and |Sgreen, ~ has base power and toughness 4/5 until end of turn and gains wither until end of turn.
(It deals damage to creatures in the form of -1/-1 counters.) */

	return mimic(player, card, event, COLOR_TEST_GREEN, COLOR_TEST_BLACK, 4, 5, 0, SP_KEYWORD_WITHER);
}

int card_worm_harvest(int player, int card, event_t event){
	/* Worm Harvest	|2|BG|BG|BG
	 * Sorcery
	 * Put a 1/1 |Sblack and |Sgreen Worm creature token onto the battlefield for each land card in your graveyard.
	 * Retrace */

	if( event == EVENT_GRAVEYARD_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.zone = TARGET_ZONE_HAND;
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		if( has_mana_hybrid(player, 3, COLOR_BLACK, COLOR_GREEN, cless) && can_sorcery_be_played(player, event) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test) ? GA_PLAYABLE_FROM_GRAVE : 0;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		charge_mana_hybrid(player, card, 3, COLOR_BLACK, COLOR_GREEN, cless);
		if( spell_fizzled != 1 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a land card to discard.");
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				discard_card(player, selected);
				return GAPAID_REMOVE;
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_CAN_CAST){
		return 1;
	}

	if(event == EVENT_RESOLVE_SPELL){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WORM, &token);
		token.pow = token.tou = 1;
		token.color_forced = COLOR_TEST_GREEN | COLOR_TEST_BLACK;
		token.qty = count_graveyard_by_type(player, TYPE_LAND);
		generate_token(&token);

		kill_card(player, card, KILL_DESTROY);
	}

	return hybrid(player, card, event);
}

int card_waves_of_aggression(int player, int card, event_t event){
	/* Waves of Aggression	|3|RW|RW
	 * Sorcery
	 * Untap all creatures that attacked this turn. After this main phase, there is an additional combat phase followed by an additional main phase.
	 * Retrace */

	if( event == EVENT_GRAVEYARD_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.zone = TARGET_ZONE_HAND;
		int cless = get_updated_casting_cost(player, card, -1, event, 3);
		if( has_mana_hybrid(player, 2, COLOR_RED, COLOR_WHITE, cless) && can_sorcery_be_played(player, event) ){
			return check_battlefield_for_special_card(player, card, player, 0, &this_test) ? GA_PLAYABLE_FROM_GRAVE : 0;
		}
	}

	if( event == EVENT_PAY_FLASHBACK_COSTS ){
		int cless = get_updated_casting_cost(player, card, -1, event, -1);
		charge_mana_hybrid(player, card, 2, COLOR_RED, COLOR_WHITE, cless);
		if( spell_fizzled != 1 ){
			char msg[100] = "Select a land card to discard.";
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, msg);
			int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &this_test);
			if( selected != -1 ){
				discard_card(player, selected);
				return GAPAID_REMOVE;
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_CAN_CAST){
		return 1;
	}

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.state = STATE_ATTACKING | STATE_ATTACKED;
		extra_combat_effect(player, card, event, 1, &test, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return hybrid(player, card, event);
}

#ifndef SHANDALAR
/* Recovered October 2022 attacker/keyword helpers. */

/* ManalinkEh 2022: 0x23636c0; original cards/eventide.c:71. */

#endif
