#include "manalink.h"

/* Canonical recovered helper declarations. */
static int ap_disciples(int player, int card, event_t event, color_test_t colors);

// Functions

int card_whirlpool_rider(int player, int card, event_t event);

static int whirlpool_rider_part(int player, int card, event_t event);

int recruiting(int player, int card, int subtype, int cards_to_look)
{
  int *library;
  int result;
  int argument2;
  int revealed_remaining;
  int recruited_count;

  revealed_remaining = reveal_top_cards_of_library(player, cards_to_look);
  if (revealed_remaining < 1)
  {
    return 0;
  }
  library = deck_ptr[player];
  recruited_count = 0;
  argument2 = revealed_remaining + -1;
  do
  {
    result = has_subtype_by_id(player, (unsigned int)cards_data[library[argument2]].id, subtype);
    if (result != 0)
    {
      add_card_to_hand(player, library[argument2]);
      result = argument2 - 1;
      remove_card_from_deck(player, argument2);
      recruited_count = recruited_count + 1;
      revealed_remaining = revealed_remaining - 1;
      argument2 = result;
    }
    else
      argument2 = argument2 - 1;
  } while (argument2 != -1);
  if (revealed_remaining < 1)
  {
    return recruited_count;
  }
  put_top_x_on_bottom(player, player, revealed_remaining);
  return recruited_count;
}

static void recruiter(int player, int card, event_t event, int subtype){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		recruiting(player, card, subtype, 4);
	}
}

#ifndef SHANDALAR
static void phyrexians(int player, int card, event_t event, int amount)
{
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    lose_life(player, amount);
    draw_cards(player, amount);
  }
}
#endif

void volver(int player, int card, event_t event, int colorless, int black, int blue, int green, int red, int white,
			int colorless2, int black2, int blue2, int green2, int red2, int white2, int kicker1_priority, int kicker2_priority
  ){
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! check_special_flags(player, card, SF_NOT_CAST) && ! is_token(player, card) ){
			int options[10] = {
							black > 0 && has_mana_multi(player, colorless, black, 0, 0, 0, 0),
							black2 > 0 && has_mana_multi(player, 0, black2, 0, 0, 0, 0),
							blue > 0 && has_mana_multi(player, colorless, 0, blue, 0, 0, 0),
							blue2 > 0 && has_mana_multi(player, 0, 0, blue2, 0, 0, 0),
							green > 0 && has_mana_multi(player, colorless, 0, 0, green, 0, 0),
							green2 > 0 && has_mana_multi(player, 0, 0, 0, green2, 0, 0),
							red > 0 && has_mana_multi(player, colorless, 0, 0, 0, red, 0),
							red2 > 0 && has_mana_multi(player, 0, 0, 0, 0, red2, 0),
							white > 0 && has_mana_multi(player, colorless, 0, 0, 0, 0, white),
							white2 > 0 && has_mana_multi(player, 0, 0, 0, 0, 0, white2)
			};

			int choice = DIALOG(player, card, event, DLG_NO_STORAGE, DLG_OMIT_ILLEGAL, DLG_NO_CANCEL,
/*1*/					"Pay black kicker", options[0], kicker1_priority,
/*2*/					"Pay black kicker", options[1], kicker2_priority,
/*3*/					"Pay blue kicker", options[2], kicker1_priority,
/*4*/					"Pay blue kicker", options[3], kicker2_priority,
/*5*/					"Pay green kicker", options[4], kicker1_priority,
/*6*/					"Pay green kicker", options[5], kicker2_priority,
/*7*/					"Pay red kicker", options[6], kicker1_priority,
/*8*/					"Pay red kicker", options[7], kicker2_priority,
/*9*/					"Pay white kicker", options[8], kicker1_priority,
/*10*/					"Pay white kicker", options[9], kicker2_priority,
/*11*/					"Pay both", has_mana_multi(player, colorless+colorless2, black+black2, blue+blue2, green+green2, red+red2, white+white2), kicker1_priority+kicker2_priority,
/*12*/					"Decline", 1, 1);

			if( choice != 12 ){
				int mana_to_charge[6];
				if( choice == 11 ){
					mana_to_charge[0] = colorless+colorless2;
				}
				else{
					mana_to_charge[0] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? colorless : colorless2;
				}
				if( choice == 11 ){
					mana_to_charge[1] = black+black2;
				}
				else{
					mana_to_charge[1] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? black : black2;
				}
				if( choice == 11 ){
					mana_to_charge[2] = blue+blue2;
				}
				else{
					mana_to_charge[2] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? blue : blue2;
				}
				if( choice == 11 ){
					mana_to_charge[3] = green+green2;
				}
				else{
					mana_to_charge[3] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? green : green2;
				}
				if( choice == 11 ){
					mana_to_charge[4] = red+red2;
				}
				else{
					mana_to_charge[4] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? red : red2;
				}
				if( choice == 11 ){
					mana_to_charge[5] = white+white2;
				}
				else{
					mana_to_charge[5] = choice == 1 || choice == 3 || choice == 5 || choice == 7 || choice == 9 ? white : white2;
				}
				charge_mana_multi(player, mana_to_charge[0], mana_to_charge[1], mana_to_charge[2], mana_to_charge[3], mana_to_charge[4], mana_to_charge[5]);
				if( spell_fizzled != 1 ){
					if( choice == 11 ){
						set_special_flags(player, card, SF_KICKED | SF_KICKED2);
					}
					else{
						int sf = mana_to_charge[0] > 0 ? SF_KICKED : SF_KICKED2;
						set_special_flags(player, card, sf);
					}
				}
			}
		}
	}
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x205ffb0; original cards/apocalypse.c:135. */
static int ap_disciples(int player, int card, event_t event, color_test_t colors)
{
  enum
  {
    CHOICE_M2_M0 = 1,
    CHOICE_FLYING = 2,
    CHOICE_MANA = 3,
    CHOICE_P2_P0 = 4,
    CHOICE_PREVENT_DAMAGE = 5
  };

  int choice;
  card_instance_t *instance;
  int32_t ability_choice;
  int black_cost_or_result;
  unsigned int white_available;
  unsigned int black_available;
  int blue;
  int red;
  target_definition_t *td_00;
  uint32_t weaken_priority;
  unsigned int red_available;
  unsigned int green_available;
  unsigned int blue_available;
  target_definition_t *weaken_target_definition;
  target_definition_t *damage_target_definition;
  target_definition_t *creature_target_definition;
  target_definition_t td;
  target_definition_t td2;
  target_definition_t td3;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    td_00 = &td;
    default_target_definition(player, card, td_00, 2);
    td.preferred_controller = player;
    default_target_definition(player, card, &td2, 2);
    default_target_definition(player, card, &td3, 0);
    td3.extra = damage_card;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (((colors & COLOR_TEST_WHITE) != COLOR_TEST_0) &&
          (black_cost_or_result = generic_activated_ability(
               player, card, EVENT_CAN_ACTIVATE, 0x1000005, 0, 0, 0, 0, 0, 1, 0, &td3, NULL),
           black_cost_or_result != 0))
      {
        return 99;
      }
      if (((((colors & COLOR_TEST_BLACK) == COLOR_TEST_0) ||
            (black_cost_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                              0, 1, 0, 0, 0, 0, 0, &td2, NULL),
             black_cost_or_result == 0)) &&
           (((colors & COLOR_TEST_BLUE) == COLOR_TEST_0 ||
             (black_cost_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                               0, 0, 1, 0, 0, 0, 0, td_00, NULL),
              black_cost_or_result == 0)))) &&
          (((colors & COLOR_TEST_RED) == COLOR_TEST_0 ||
            (black_cost_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                              0, 0, 0, 0, 1, 0, 0, td_00, NULL),
             black_cost_or_result == 0))))
      {
        if ((colors & COLOR_TEST_GREEN) == COLOR_TEST_0)
        {
          return 0;
        }
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x200001, 0, 0, 0, 1, 0, 0,
                                      0, NULL, NULL) == 0)
        {
          return 0;
        }
        return mana_producer(player, card, EVENT_CAN_ACTIVATE);
      }
      return 1;
    }
    if (event == EVENT_ACTIVATE)
    {
      black_available = 0;
      instance->info_slot = 0;
      instance->number_of_targets = '\0';
      if ((colors & COLOR_TEST_BLACK) != COLOR_TEST_0)
      {
        black_cost_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 1,
                                                         0, 0, 0, 0, 0, &td2, NULL);
        black_available = (unsigned int)(black_cost_or_result != 0);
      }
      blue_available = 0;
      if ((colors & COLOR_TEST_BLUE) != COLOR_TEST_0)
      {
        black_cost_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0,
                                                         1, 0, 0, 0, 0, td_00, NULL);
        blue_available = (unsigned int)(black_cost_or_result != 0);
      }
      green_available = 0;
      if ((colors & COLOR_TEST_GREEN) != COLOR_TEST_0)
      {
        black_cost_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x200001,
                                                         0, 0, 0, 1, 0, 0, 0, NULL, NULL);
        green_available = (unsigned int)(black_cost_or_result != 0);
      }
      red_available = 0;
      if ((colors & COLOR_TEST_RED) != COLOR_TEST_0)
      {
        black_cost_or_result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 0, 0,
                                                         0, 0, 1, 0, 0, td_00, NULL);
        red_available = (unsigned int)(black_cost_or_result != 0);
      }
      white_available = 0;
      if ((colors & COLOR_TEST_WHITE) != COLOR_TEST_0)
      {
        black_cost_or_result = generic_activated_ability(
            player, card, EVENT_CAN_ACTIVATE, 0x1000005, 0, 0, 0, 0, 0, 1, 0, &td3, NULL);
        white_available = (unsigned int)(black_cost_or_result != 0);
      }
      if (current_phase < 0x17)
      {
        weaken_priority = 10;
        black_cost_or_result = 1;
      }
      else
      {
        weaken_priority = 1;
        black_cost_or_result = (unsigned int)(current_phase == 0x18) * 9 + 1;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_OMIT_ILLEGAL, "Give -2/-0",
                           black_available, weaken_priority, "Give Flying", blue_available,
                           black_cost_or_result, "Get mana", green_available, 1, "Give +2/+0",
                           red_available, black_cost_or_result, "Prevent damage", white_available,
                           ((white_available == 0) ? 1U : 50U), 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_FLYING)
      {
        red = 0;
        blue = 1;
        black_cost_or_result = 0;
      }
      else if (choice == CHOICE_M2_M0)
      {
        red = 0;
        blue = 0;
        black_cost_or_result = 1;
        td_00 = &td2;
      }
      else
      {
        if (choice == CHOICE_MANA)
        {
          charge_mana_for_activated_ability(player, card, 0, 0, 0, 1, 0, 0);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          produce_mana_tapped_all_one_color(player, card, COLOR_TEST_ANY_COLORED, 1);
          return 0;
        }
        if (choice != CHOICE_P2_P0)
        {
          if (choice != CHOICE_PREVENT_DAMAGE)
          {
            return 0;
          }
          return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x1000005, 0, 0, 0, 0, 0,
                                           1, 0, &td3, "TARGET_DAMAGE");
        }
        red = 1;
        blue = 0;
        black_cost_or_result = 0;
      }
      return generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 0, black_cost_or_result,
                                       blue, 0, red, 0, 0, td_00, "TARGET_CREATURE");
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
    default_target_definition(player, card, &td2, 2);
    default_target_definition(player, card, &td3, 0);
    td3.extra = damage_card;
    instance = get_card_instance(player, card);
  }
  creature_target_definition = &td;
  damage_target_definition = &td3;
  weaken_target_definition = &td2;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    ability_choice = instance->info_slot;
    if (ability_choice == CHOICE_M2_M0)
    {
      black_cost_or_result = valid_target(weaken_target_definition);
      if (black_cost_or_result != 0)
      {
        pump_until_eot_merge_previous_alt(instance->parent_controller, instance->parent_card,
                                          instance->targets[0].player, instance->targets[0].card,
                                          -2, 0, 2);
      }
      ability_choice = instance->info_slot;
    }
    if (ability_choice == CHOICE_FLYING)
    {
      black_cost_or_result = valid_target(creature_target_definition);
      if (black_cost_or_result != 0)
      {
        pump_ability_until_eot_no_repeat_alt(instance->parent_controller, instance->parent_card,
                                             instance->targets[0].player, instance->targets[0].card,
                                             0x20, 0, 1);
      }
      ability_choice = instance->info_slot;
    }
    if (ability_choice == CHOICE_P2_P0)
    {
      black_cost_or_result = valid_target(creature_target_definition);
      if (black_cost_or_result != 0)
      {
        pump_until_eot_merge_previous_alt(instance->parent_controller, instance->parent_card,
                                          instance->targets[0].player, instance->targets[0].card, 2,
                                          0, 1);
      }
      ability_choice = instance->info_slot;
    }
    if ((ability_choice == CHOICE_PREVENT_DAMAGE) &&
        (black_cost_or_result = valid_target(damage_target_definition), black_cost_or_result != 0))
    {
      prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
      return 0;
    }
  }
  return 0;
}
#endif

static int desolation_creature(int player, int card, event_t event, type_t type){
	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! has_mana(player, COLOR_WHITE, 2) ){
			ai_modifier -= count_subtype(player, type, -1)*10;
		}
	}

	kicker(player, card, event, MANACOST_W(2));

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		default_test_definition(&test, type);
		new_manipulate_all(player, card, player, &test, ACT_KILL_DESTROY);
		if( kicked(player, card) ){
			new_manipulate_all(player, card, 1-player, &test, ACT_KILL_DESTROY);
		}
	}

	return 0;
}

// Cards

int card_aether_mutation(int player, int card, event_t event){
	/* AEther Mutation	|3|G|U
	 * Sorcery
	 * Return target creature to its owner's hand. Put X 1/1 |Sgreen Saproling creature tokens onto the battlefield, where X is that creature's converted mana cost. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td1);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td1, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			generate_tokens_by_id(player, card, CARD_ID_SAPROLING, get_cmc(instance->targets[0].player, instance->targets[0].card));
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

/* ManalinkEh 2022: 0x2061850; original cards/apocalypse.c:332. */
int card_ana_disciple(int player, int card, event_t event){
  /* Ana Disciple |G
   * Creature — Human Wizard 1/1
   * {U}, {T}: Target creature gains flying until end of turn.
   * {B}, {T}: Target creature gets -2/-0 until end of turn. */

	#ifndef SHANDALAR
	return ap_disciples(player, card, event, COLOR_TEST_BLUE | COLOR_TEST_BLACK);
#else
	return ap_disciples(player, card, event, COLOR_TEST_BLACK | COLOR_TEST_BLUE);
#endif
}

int card_ana_sanctuary(int player, int card, event_t event){
  /* Ana Sanctuary	|2|G
   * Enchantment
   * At the beginning of your upkeep, if you control a blue or black permanent, target creature gets +1/+1 until end of turn. If you control a blue permanent and a black permanent, that creature gets +5/+5 until end of turn instead. */

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

		score |= 2*check_battlefield_for_special_card(player, card, player, 0, &this_test);

		upkeep_trigger_ability_mode(player, card, event, player, score && can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.preferred_controller = player;
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

		score |= 2*check_battlefield_for_special_card(player, card, player, 0, &this_test);

		if( score > 0 ){
			int pump = 0;
			if( (score & 1) || (score & 2) ){
				pump = 1;
			}
			if( score == 3 ){
				pump = 5;
			}
			if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
				instance->number_of_targets = 1;
				pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, pump, pump);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_anavolver(int player, int card, event_t event){
  /* Anavolver |3|G
   * 	  Creature - Volver 3/3
   * 	  Kicker {1}{U} and/or {B} (You may pay an additional {1}{U} and/or {B} as you cast this spell.)
   * 	  If Anavolver was kicked with its {1}{U} kicker, it enters the battlefield with two +1/+1 counters on it and with flying.
   * 	  If Anavolver was kicked with its {B} kicker, it enters the battlefield with a +1/+1 counter on it and with "Pay 3 life: Regenerate Anavolver." */

	volver(player, card, event, MANACOST_XU(1, 1), MANACOST_B(1), 10, 5);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = (check_special_flags(player, card, SF_KICKED) ? 2 : 0) + (check_special_flags(player, card, SF_KICKED2) ? 1 : 0);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, amount);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_FLYING);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED2) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_REGENERATION);
	}

	if( ! IS_GAA_EVENT(event) || ! check_special_flags(player, card, SF_KICKED2) ){
		return 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		return regeneration(player, card, event, MANACOST0);
	}

	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST0, 3, NULL, NULL);
}

int card_bloodfire_colossus(int player, int card, event_t event){
/* CARD_ID_BLOODFIRE_COLOSSUS	5335
Bloodfire Colossus	|6|R|R
Creature - Giant 6/6
|R, Sacrifice ~: ~ deals 6 damage to each creature and each player. */

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_R(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_damage_all(player, card, ANYBODY, 6, NDA_PLAYER_TOO, &this_test);
	}

	return 0;
}

int card_bloodfire_dwarf(int player, int card, event_t event){
/* CARD_ID_BLOODFIRE_DWARF	5336
Bloodfire Dwarf	|R
Creature - Dwarf 1/1
|R, Sacrifice ~: ~ deals 1 damage to each creature without flying. */

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_R(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		this_test.keyword = KEYWORD_FLYING;
		this_test.keyword_flag = DOESNT_MATCH;
		new_damage_all(player, card, ANYBODY, 1, 0, &this_test);
	}

	return 0;
}

int card_bloodfire_kavu(int player, int card, event_t event){
/* CARD_ID_BLOODFIRE_KAVU	5338
Bloodfire Kavu	|2|R|R
Creature - Kavu 2/2
|R, Sacrifice ~: ~ deals 2 damage to each creature. */

	if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST_R(1), 0, NULL, NULL);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_CREATURE);
		new_damage_all(player, card, ANYBODY, 2, 0, &this_test);
	}

	return 0;
}

int card_brass_herald(int player, int card, event_t event){
/* Brass Herald	|6
 * Artifact Creature - Golem 2/2
 * As ~ enters the battlefield, choose a creature type.
 * When ~ enters the battlefield, reveal the top four cards of your library.
 *		Put all creature cards of the chosen type revealed this way into your hand and the rest on the bottom of your library in any order.
 * Creatures of the chosen type get +1/+1. */

	if (event == EVENT_RESOLVE_SPELL){
		get_card_instance(player, card)->info_slot = select_a_subtype(player, card) + 1;
		// + 1 so if nothing's chosen - perhaps something turned into a copy of this after entering the battlefield -
		// it's distinct from SUBTYPE_ADVISOR at 0
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    card_instance_t* instance;

	if( event == EVENT_ETB_ABILITY ){
		instance = get_card_instance(player, card);

		int creature_type = instance->info_slot - 1;

		int i=0;
		int *deck = deck_ptr[player];
		for(i=0;i<4;i++){
			int card_added = add_card_to_hand(player, deck[0] );
			reveal_card(player, card, player, card_added);
			if( creature_type >= 0 && has_subtype(player, card_added, creature_type) ){
				remove_card_from_deck( player, 0 );
			}
			else{
				obliterate_card(player, card_added);
				put_top_card_of_deck_to_bottom(player);
			}
		}
	}
	else if( (instance = in_play(player, card) ) && ( get_card_instance(player, card)->info_slot >= 1 ) &&
	         has_subtype(affected_card_controller, affected_card, get_card_instance(player, card)->info_slot -1) ){
		     boost_creature_type(player, card, event, 0, 1, 1, 0, 0);
	}

	return 0;
}

int card_captains_maneuver(int player, int card, event_t event){
	/* Captain's Maneuver	|X|R|W	0x2008b21
	 * Instant
	 * The next X damage that would be dealt to target creature, planeswalker, or player this turn is dealt to another target creature, planeswalker, or player instead. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( can_target(&td1) ){
			return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, NULL, 1, NULL);
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
		if( spell_fizzled != 1 ){
			new_pick_target(&td1, "TARGET_ANY", 1, 1);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) && validate_target(player, card, &td1, 1) ){
			card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( instance->info_slot >= dmg->info_slot ){
				dmg->damage_target_player = instance->targets[1].player;
				dmg->damage_target_card = instance->targets[1].card;
			}
			else{
				dmg->info_slot-=instance->info_slot;
				damage_creature(instance->targets[1].player, instance->targets[1].card, instance->info_slot,
								dmg->damage_source_player, dmg->damage_source_card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

/* ManalinkEh 2022: 0x2062680; original cards/apocalypse.c:586. */
int card_ceta_disciple(int player, int card, event_t event){
  /* Ceta Disciple |U
   * Creature — Merfolk Wizard 1/1
   * {R}, {T}: Target creature gets +2/+0 until end of turn.
   * {G}, {T}: Add one mana of any color to your mana pool. */

	#ifndef SHANDALAR
	return ap_disciples(player, card, event, COLOR_TEST_RED | COLOR_TEST_GREEN);
#else
	return ap_disciples(player, card, event, COLOR_TEST_GREEN | COLOR_TEST_RED);
#endif
}

int card_cetavolver(int player, int card, event_t event){
  /* Cetavolver English |1|U
   * 	  Creature - Volver 1/1
   * 	  Kicker {1}{R} and/or {G} (You may pay an additional {1}{R} and/or {G} as you cast this spell.)
   * 	  If Cetavolver was kicked with its {1}{R} kicker, it enters the battlefield with two +1/+1 counters on it and with first strike.
   * 	  If Cetavolver was kicked with its {G} kicker, it enters the battlefield with a +1/+1 counter on it and with trample. */

	volver(player, card, event, MANACOST_XR(1, 1), MANACOST_G(1), 5, 10);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = (check_special_flags(player, card, SF_KICKED) ? 2 : 0) + (check_special_flags(player, card, SF_KICKED2) ? 1 : 0);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, amount);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_FIRST_STRIKE);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED2) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_TRAMPLE);
	}

	return 0;
}

int card_ceta_sanctuary(int player, int card, event_t event){
/* CARD_ID_CETA_SANCTUARY	5343
Ceta Sanctuary	|2|U
Enchantment
At the beginning of your upkeep, if you control a |Sred or |Sgreen permanent, draw a card, then discard a card.
If you control a |Sred permanent and a |Sgreen permanent, instead draw two cards, then discard a card. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		score |= 2*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		if( score > 0 ){
			int crds = 0;
			if( (score & 1) || (score & 2) ){
				crds = 1;
			}
			if( score == 3 ){
				crds = 2;
			}
			if( crds > 0  ){
				draw_cards(player, crds);
				discard(player, 0, player);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_order_chaos(int player, int card, event_t event)
{
  /* Order // Chaos	|3|W // |2|R
   * Instant // Instant
   * Order:
   * Exile target attacking creature.
   *
   * Chaos:
   * Creatures can't block this turn. */

  card_instance_t *instance;
  int can_play_first_half;
  unsigned int half_flags;
  int chaos_priority;
  int order_priority;
  target_definition_t td;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    generic_split_card(player, card, EVENT_MODIFY_COST, 0x4b1b, can_target(&td), 5, 0x4adb, 1, 10);
    return 0;
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_ATTACKING;
    new_pick_target(&td, "Select target attacking creature.", 0, 0x81);
    return 0;
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.required_state = TARGET_STATE_ATTACKING;
  chaos_priority = 8;
  order_priority = 5;
  if (event == EVENT_RESOLVE_SPELL)
  {

    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      if (valid_target(&td) != 0)
      {
        kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
      }
      half_flags = instance->info_slot;
    }
    if ((half_flags & 2) != 0)
    {
      pump_subtype_until_eot(player, card, 2, -1, 0, 0, 0, 0x20);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  else if (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller)
  {
    order_priority = 0;
    bool choose_chaos_priority = true;
    if (current_turn == player)
    {
      chaos_priority = can_target(&td);
      order_priority = 0;
      if (chaos_priority != 0)
        order_priority = 10;
      if (current_turn == player)
      {
        chaos_priority = 0;
        choose_chaos_priority = false;
      }
    }
    if (choose_chaos_priority)
    {
      chaos_priority = 10;
      if (0x14 < current_phase)
        chaos_priority = 0;
    }
  }
  can_play_first_half = can_target(&td);
  return generic_split_card(player, card, event, 0x4b1b, can_play_first_half, order_priority,
                            0x4adb, 1, chaos_priority);
}

int card_consume_strength(int player, int card, event_t event){
/* CARD_ID_CONSUME_STRENGTH	5348
Consume Strength	|1|B|G
Instant
Target creature gets +2/+2 until end of turn. Another target creature gets -2/-2 until end of turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && can_target(&td1) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target creature for +2/+2.", 1, NULL);
		if( spell_fizzled != 1 ){
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
			new_pick_target(&td1, "Select target creature for -2/-2.", 1, 1 | GS_LITERAL_PROMPT);
			state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
		if( validate_target(player, card, &td1, 1) ){
			pump_until_eot(player, card, instance->targets[1].player, instance->targets[1].card, -2, -2);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

/* ManalinkEh 2022: 0x2063090; original cards/apocalypse.c:757. */
int card_credit_voucher(int player, int card, event_t event)
{
  /* Credit Voucher	|2
   * Artifact
   * |2, |T, Sacrifice this artifact: Shuffle any number of cards from your hand into your library, then draw that many cards. */

  int result;
  int selected;
  test_definition_t test;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&test, 0xff, "Select a card to shuffle into library.");
    result = 0;
    if (hand_count[player] != 0)
    {
      result = 0;
      do
      {
        selected = new_select_a_card(player, player, 4, 0, 2, -1, &test);
        if (selected < 0)
          break;
        put_on_top_of_deck(player, selected);
        result = result + 1;
      } while (hand_count[player] != 0);
    }
    shuffle(player);
    draw_cards(player, result);
  }
  result = generic_activated_ability(player, card, event, 9, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
  return result;
}

static const char* is_blocking_crm(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(card, targeting_card))
	return NULL;
  else
	return "must be blocking Cromat";
}

static const char* crm_is_blocking_him(int who_chooses, int player, int card, int targeting_player, int targeting_card)
{
  if (is_blocking(targeting_card, card))
	return NULL;
  else
	return "must be blocked by Cromat";
}

int card_cromat(int player, int card, event_t event){
  /* Cromat	|W|U|B|R|G
   * Legendary Creature - Illusion 5/5
   * |W|B: Destroy target creature blocking or blocked by ~.
   * |U|R: ~ gains flying until end of turn.
   * |B|G: Regenerate ~.
   * |R|W: ~ gets +1/+1 until end of turn.
   * |G|U: Put ~ on top of its owner's library. */

	/*
	  Cromat English

	  Legendary Creature - Illusion 5/5, WUBRG (5)

	  {W}{B}: Destroy target creature blocking or blocked by Cromat.

	  {U}{R}: Cromat gains flying until end of turn.

	  {B}{G}: Regenerate Cromat.

	  {R}{W}: Cromat gets +1/+1 until end of turn.

	  {G}{U}: Put Cromat on top of its owner's library.
	*/
	check_legend_rule(player, card, event);

	// Debug code: put into play a land for each basic type when it resolves. Do not remove.
	/*
	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for(i=0; i<5; i++){
			int card_added = add_card_to_hand(player, i);
			put_into_play(player, card_added);
		}
	}
	*/

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_DESTR_BLOCK = 1,
		CHOICE_FLYING = 2,
		CHOICE_REGENERATE = 3,
		CHOICE_PUMP = 4,
		CHOICE_RETURN_DECK = 5,
	};

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller =  1-player;
	td2.preferred_controller = 1-player;
	td2.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td2.extra = target_predicate_extra(is_blocking_crm);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller =  1-player;
	td1.preferred_controller = 1-player;
	td1.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td1.extra = target_predicate_extra(crm_is_blocking_him);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card) ){
		int result = regeneration(player, card, event, MANACOST_BG(1, 1));
		if( result ){
			return result;
		}
		if( current_turn == player ){
			if( current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_BW(1, 1), 0, &td2, "TARGET_CREATURE") ){
				return 1;
			}
		}
		if( current_turn != player ){
			if( current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_BW(1, 1), 0, &td1, "TARGET_CREATURE") ){
				return 1;
			}
		}
		if( has_mana_for_activated_ability(player, card, MANACOST_UR(1, 1)) ){ // gains flying
			return 1;
		}
		if( has_mana_for_activated_ability(player, card, MANACOST_RW(1, 1)) ){ // pump
			return 1;
		}
		if( has_mana_for_activated_ability(player, card, MANACOST_UG(1, 1)) ){ // put on top of deck
			return 1;
		}
	}

	if (event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		int can_regen = regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST_BG(1, 1));
		int can_destroy_blocked_blocking_creature = 0;
		if( current_turn == player ){
			can_destroy_blocked_blocking_creature = (current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_BG(1, 1), 0, &td2, "TARGET_CREATURE"));
		}
		else{
			can_destroy_blocked_blocking_creature = (current_phase == PHASE_AFTER_BLOCKING && generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET, MANACOST_BG(1, 1), 0, &td1, "TARGET_CREATURE"));
		}
		int can_gain_flying =  has_mana_for_activated_ability(player, card, MANACOST_UR(1, 1)) ;
		int can_pump = has_mana_for_activated_ability(player, card, MANACOST_RW(1, 1)) ;
		int can_be_put_on_top_of_library = has_mana_for_activated_ability(player, card, MANACOST_UG(1, 1)) ;

		int choice = can_regen ? CHOICE_REGENERATE : 0;
		if( choice ){
			if( ! charge_mana_for_activated_ability(player, card, MANACOST_BG(1, 1)) ){
				spell_fizzled = 1;
				return 0;
			}
		}
		else{
			choice = DIALOG(player, card, event,
							DLG_RANDOM,
							"Destroy a creature blocking / blocked by me", can_destroy_blocked_blocking_creature, 15, DLG_MANA(MANACOST_BW(1, 1)),
							"Gains flying", can_gain_flying, 5+30*(current_phase < PHASE_DECLARE_BLOCKERS), DLG_MANA(MANACOST_UR(1, 1)),
							"Regenerate", can_regen, 35, DLG_MANA(MANACOST_BG(1, 1)),
							"Gains +1/+1", can_pump, 25-20*can_destroy_blocked_blocking_creature, DLG_MANA(MANACOST_RW(1, 1)),
							"Put on top of deck", can_be_put_on_top_of_library, 5, DLG_MANA(MANACOST_UG(1, 1))
							);
		}
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}

		if( choice == CHOICE_DESTR_BLOCK ){
			if( current_turn == player ){
				if( ! select_target(player, card, &td2, "Select a creature that is blocking Cromat", &(instance->targets[0])) ){
					spell_fizzled = 1;
					return 0;
				}
			}
			else{
				if( ! select_target(player, card, &td1, "Select a creature that is blocked by Cromat", &(instance->targets[0])) ){
					spell_fizzled = 1;
					return 0;
				}
			}
		}
		instance->info_slot = choice;
	}

	if( event == EVENT_RESOLVE_ACTIVATION){
			int choice = instance->info_slot;
			switch (choice)
				{
				  case CHOICE_REGENERATE:
					{
						if( can_be_regenerated(instance->parent_controller, instance->parent_card) ){
							regenerate_target(instance->parent_controller, instance->parent_card);
						}
						break;
					}

				  case CHOICE_DESTR_BLOCK:
					{
						if( (current_turn == player && valid_target(&td2)) || (current_turn != player && valid_target(&td1)) ){
							kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
						}
						break;
					}
				  case CHOICE_FLYING:
					{
						pump_ability_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card,
												0, 0, KEYWORD_FLYING, 0);
						break;
					}
				  case CHOICE_PUMP:
					{
						pump_until_eot(instance->parent_controller, instance->parent_card, instance->parent_controller, instance->parent_card, 1, 1);
						break;
					}
				  case CHOICE_RETURN_DECK:
					{
						if( in_play(instance->parent_controller, instance->parent_card) ){
							put_on_top_of_deck(instance->parent_controller, instance->parent_card);
						}
						break;
					}
				}
	}

	return 0;
}

int card_death_grasp(int player, int card, event_t event){
	/* Death Grasp	|X|W|B	0x2005651
	 * Sorcery
	 * ~ deals X damage to any target. You gain X life. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
			gain_life(player, instance->info_slot);
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return generic_spell(player, card, event, GS_X_SPELL | GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

/* ManalinkEh 2022: 0x2063de0; original cards/apocalypse.c:1003. */
int card_dega_disciple(int player, int card, event_t event){
  /* Dega Disciple |W
   * Creature — Human Wizard 1/1
   * {B}, {T}: Target creature gets -2/-0 until end of turn.
   * {R}, {T}: Target creature gets +2/+0 until end of turn. */

	#ifndef SHANDALAR
	return ap_disciples(player, card, event, COLOR_TEST_RED | COLOR_TEST_BLACK);
#else
	return ap_disciples(player, card, event, COLOR_TEST_BLACK | COLOR_TEST_RED);
#endif
}

int card_dega_sanctuary(int player, int card, event_t event){
/* CARD_ID_DEGA_SANCTUARY	5353
Dega Sanctuary	|2|W
Enchantment
At the beginning of your upkeep, if you control a |Sblack or |Sred permanent, you gain 2 life.
If you control a |Sblack permanent and a |Sred permanent, you gain 4 life instead. */


	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_RED);
		score |=  2*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		if( score > 0 ){
			int crds = 0;
			if( (score & 1) || (score & 2) ){
				crds = 2;
			}
			if( score == 3 ){
				crds = 4;
			}
			if( crds > 0  ){
				gain_life(player, crds);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_degavolver(int player, int card, event_t event){
  /* Degavolver |1|W
   * 	  Creature - Volver 1/1
   * 	  Kicker {1}{B} and/or {R} (You may pay an additional {1}{B} and/or {R} as you cast this spell.)
   * 	  If Degavolver was kicked with its {1}{B} kicker, it enters the battlefield with two +1/+1 counters on it and with "Pay 3 life: Regenerate Degavolver."
   * 	  If Degavolver was kicked with its {R} kicker, it enters the battlefield with a +1/+1 counter on it and with first strike. */

	volver(player, card, event, MANACOST_XB(1, 1), MANACOST_R(1), 5, 10);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = (check_special_flags(player, card, SF_KICKED) ? 2 : 0) + (check_special_flags(player, card, SF_KICKED2) ? 1 : 0);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, amount);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED2) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_FIRST_STRIKE);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_REGENERATION);
	}

	if( ! IS_GAA_EVENT(event) || ! check_special_flags(player, card, SF_KICKED) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_REGENERATION);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		regeneration(player, card, event, MANACOST0);
	}

	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST0, 3, NULL, NULL);
}

int card_desolation_angel(int player, int card, event_t event)
{
  /* Desolation Angel |3|B|B
   * Creature — Angel 5/4
   * Kicker {W}{W} (You may pay an additional {W}{W} as you cast this spell.)
   * Flying
   * When Desolation Angel enters the battlefield, destroy all lands you control. If it was kicked, destroy all lands instead. */

return desolation_creature(player, card, event, TYPE_LAND);
}

int card_desolation_giant(int player, int card, event_t event)
{
  /* Desolation Giant	|2|R|R
   * Creature - Giant 3/3
   * Kicker |W|W (You may pay an additional |W|W as you cast this spell.)
   * When this creature enters, destroy all other creatures you control. If it was kicked, destroy all other creatures instead. */

  /*
Desolation Giant English
Creature — Giant 3/3, 2RR (4)
Kicker {W}{W} (You may pay an additional {W}{W} as you cast this spell.)
When Desolation Giant enters the battlefield, destroy all other creatures you control.
	If it was kicked, destroy all other creatures instead.
*/

if (event == EVENT_CAST_SPELL && affect_me(player, card) && !has_mana(player, COLOR_WHITE, 2))
    ai_modifier -= 10 * count_subtype(player, TYPE_CREATURE, -1);
  kicker(player, card, event, MANACOST_W(2));
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    test_definition_t test;
    default_test_definition(&test, TYPE_CREATURE);
    test.not_me = 1;
    new_manipulate_all(player, card, kicked(player, card) ? ANYBODY : player, &test, ACT_KILL_DESTROY);
  }
  return 0;
}

// divine light --> sivvi's rouse

// dodecapod --> vanilla

int card_dragon_arc(int player, int card, event_t event){
/* CARD_ID_DRAGON_ARCH	5360
Dragon Arch	|5
Artifact
|2, |T: You may put a multicolored creature card from your hand onto the battlefield. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		char msg[100] = "Select a multicolored creature.";
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, msg);
		this_test.color_flag = F3_MULTICOLORED;
		new_global_tutor(player, player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
	}

  return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(2), 0, NULL, NULL);
}

int card_ebony_treefolk(int player, int card, event_t event){
/* CARD_ID_EBONY_TREEFOLK	5363
Ebony Treefolk	|1|B|G
Creature - Treefolk 3/3
|B|G: ~ gets +1/+1 until end of turn. */

	return generic_shade(player, card, event, 0, MANACOST_BG(1, 1), 1, 1, 0, 0);
}

int card_enlistment_officer(int player, int card, event_t event){
	/* Enlistment Officer	|3|W
	 * Creature - Human Soldier 2/3
	 * First strike
	 * When ~ enters the battlefield, reveal the top four cards of your library. Put all Soldier cards revealed this way into your hand and the rest on the
	 * bottom of your library in any order. */
	recruiter(player, card, event, SUBTYPE_SOLDIER);
	return 0;
}

int card_evasive_action(int player, int card, event_t event){
	/* Evasive Action	|1|U
	 * Instant
	 * Domain - Counter target spell unless its controller pays |1 for each basic land type among lands you control. */

	if( event == EVENT_CAN_CAST || (event == EVENT_CAST_SPELL && affect_me(player, card)) ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		counterspell_resolve_unless_pay_x(player, card, NULL, 0, count_domain(player, card));
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_fervent_charge(int player, int card, event_t event)
{
/* CARD_ID_FERVENT_CHARGE	5368
Fervent Charge	|1|W|B|R
Enchantment
Whenever a creature you control attacks, it gets +2/+2 until end of turn. */

  store_attackers(player, card, event, DAT_TRACK, player, -1, NULL);
  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
  if (event == EVENT_DAT_ABILITY)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  int amt = instance->info_slot;
	  unsigned char* attackers = (unsigned char*)(&instance->targets[2].player);
	  for (--amt; amt >= 0; --amt)
		if (in_play(current_turn, attackers[amt]))
		  pump_until_eot(player, card, current_turn, attackers[amt], 2, 2);
	}

  return global_enchantment(player, card, event);
}

int card_fire_ice(int player, int card, event_t event)
{
  /* Fire // Ice	|1|R|/|1|U	0x2001380
   * Instant // Instant
   * Fire deals 2 damage divided as you choose among one or two targets.
   * //
   * Tap target permanent.
   * Draw a card. */

  card_instance_t *instance;
  int ice_available;
  int can_play_first_half;
  unsigned int half_flags;
  int ice_priority_or_target_card;
  target_definition_t *fire_target_definition;
  target_definition_t td;
  target_definition_t td2;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    default_target_definition(player, card, &td, 2);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    default_target_definition(player, card, &td2, 0x47);
    ice_priority_or_target_card = can_target(&td2);
    generic_split_card(player, card, EVENT_MODIFY_COST, 0x4aff, can_target(&td), 10, 0x4b07,
                       ice_priority_or_target_card, 8);
    return 0;
  }
  if (event == EVENT_PLAY_FIRST_HALF)
  {
    instance->number_of_targets = '\0';
    default_target_definition(player, card, &td2, 0x102);
    td2.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    ice_priority_or_target_card = pick_target(&td2, "TARGET_ANY");
    if (ice_priority_or_target_card != 0)
    {
      ice_priority_or_target_card = instance->targets[0].card;
      if (ice_priority_or_target_card != -1)
      {
        add_state(instance->targets[0].player, ice_priority_or_target_card, 0x200000);
      }
      new_pick_target(&td2, "TARGET_ANY", 1, 1);
      remove_state(instance->targets[0].player, instance->targets[0].card, 0x200000);
    }
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    instance->number_of_targets = '\0';
    default_target_definition(player, card, &td2, 0x47);
    pick_target(&td2, "TARGET_PERMANENT");
    return 0;
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
  default_target_definition(player, card, &td2, 0x47);
  ice_priority_or_target_card = 5;
  if (event == EVENT_RESOLVE_SPELL)
  {

    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      divide_damage(player, card, &td);
      half_flags = instance->info_slot;
    }
    if (((half_flags & 2) != 0) &&
        (ice_priority_or_target_card = valid_target(&td2), ice_priority_or_target_card != 0))
    {
      tap_card(instance->targets[0].player, instance->targets[0].card);
      draw_a_card(player);
    }
    kill_card(player, card, KILL_DESTROY);
    ice_priority_or_target_card = 5;
  }
  else if (event == EVENT_CAST_SPELL && card == affected_card &&
           player == affected_card_controller && current_turn != player)
    ice_priority_or_target_card = ((current_phase < 0x17) ? 15U : 5U);
  fire_target_definition = &td;
  ice_available = can_target(&td2);
  can_play_first_half = can_target(fire_target_definition);
  return generic_split_card(player, card, event, 0x4aff, can_play_first_half, 10, 0x4b07,
                            ice_available, ice_priority_or_target_card);
}

int card_flowstone_charger(int player, int card, event_t event){
	/* Flowstone Charger	|2|R|W
	 * Creature - Beast 2/5
	 * Whenever ~ attacks, it gets +3/-3 until end of turn. */

	return when_attacks_pump_self(player, card, event, 3,-3);
}

int card_fungal_shambler(int player, int card, event_t event){
/* CARD_ID_FUNGAL_SHAMBLER	5372
Fungal Shambler	|4|G|U|B
Creature - Fungus Beast 6/4
Trample
Whenever ~ deals damage to an opponent, you draw a card and that opponent discards a card. */

	if( damage_dealt_by_me(player, card, event, DDBM_MUST_DAMAGE_OPPONENT) ){
		draw_cards(player, 1);
		discard(1-player, 0, player);
	}

	return 0;
}

int card_gerrard_capashen(int player, int card, event_t event){
  /* Gerrard Capashen |3|W|W
   * Legendary Creature — Human Soldier 3/4
   * At the beginning of your upkeep, you gain 1 life for each card in target opponent's hand.
   * {3}{W}: Tap target creature. Activate this ability only if Gerrard Capashen is attacking. */

	check_legend_rule(player, card, event);

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		target_definition_t td_player;
		default_target_definition(player, card, &td_player, 0);
		td_player.zone = TARGET_ZONE_PLAYERS;

		upkeep_trigger_ability_mode(player, card, event, player, would_validate_arbitrary_target(&td_player, 1-player, -1) ?
									RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td_player;
		default_target_definition(player, card, &td_player, 0);
		td_player.zone = TARGET_ZONE_PLAYERS;

		if( would_validate_arbitrary_target(&td_player, 1-player, -1) ){
			gain_life(player, hand_count[1-player]);
		}
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td_creature;
	default_target_definition(player, card, &td_creature, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && is_attacking(player, card) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(3, 1), 0, &td_creature, "TARGET_CREATURE");
	}

	if( event == EVENT_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_XW(3, 1), 0, &td_creature, "TARGET_CREATURE");
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td_creature) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return 0;
}

int card_gerrards_verdict(int player, int card, event_t event){
  /* Gerrard's Verdict |W|B
   * Sorcery
   * Target player discards two cards. You gain 3 life for each land card discarded this way. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");

			int amount = MIN(2, hand_count[instance->targets[0].player]);
			while( amount ){
					int result = new_select_a_card(instance->targets[0].player, instance->targets[0].player, TUTOR_FROM_HAND, 1,
													AI_MIN_VALUE, -1, &test);
					int lifegain = is_what(instance->targets[0].player, result, TYPE_LAND) ? 1 : 0;
					new_discard_card(instance->targets[0].player, result, 0, player);
					if( lifegain ){
						gain_life(player, 3);
					}
					amount--;
			}
			kill_card(player, card, KILL_DESTROY);
		}
	}
	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_goblin_legionnaire(int player, int card, event_t event){
	/* Goblin Legionnaire	|R|W	0x2008b7b
	 * Creature - Goblin Soldier 2/2
	 * |R, Sacrifice ~: It deals 2 damage to any target.
	 * |W, Sacrifice ~: Prevent the next 2 damage that would be dealt to any target this turn. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, 0);
	td1.extra = damage_card;

	enum{
		CHOICE_DAMAGE = 1,
		CHOICE_PREVENT
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_SACRIFICE_ME, MANACOST_W(1), 0,
			&td1, NULL) )
		{
			return 99;
		}
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0, &td, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		int abilities[3];
		abilities[0] = 0; // choice == 0 -> spell_fizzled = 1
		abilities[1] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0,
												&td, NULL);
		abilities[2] = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION |
												GAA_SACRIFICE_ME, MANACOST_W(1), 0, &td1, NULL);
		instance->info_slot = instance->number_of_targets = 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Damage target",	abilities[1], 5,
							"Prevent damage",	abilities[2], 20);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_DAMAGE ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_SACRIFICE_ME, MANACOST_R(1), 0,
											&td, "TARGET_ANY");
		}
		if( choice == CHOICE_PREVENT ){
			return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION | GAA_SACRIFICE_ME,
											MANACOST_W(1), 0, &td1, "TARGET_DAMAGE");
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_DAMAGE  && valid_target(&td) ){
			damage_target0(player, card, 2);
		}
		if( instance->info_slot == CHOICE_PREVENT ){
			if( valid_target(&td1) ){
				prevent_damage(instance->targets[0].player, instance->targets[0].card, 2);
			}
		}
	}

	return 0;
}

int card_goblin_ringleader( int player, int card, event_t event){
	/* Goblin Ringleader	|3|R
	 * Creature - Goblin 2/2
	 * Haste
	 * When ~ enters the battlefield, reveal the top four cards of your library. Put all Goblin cards revealed this way into your hand and the rest on the
	 * bottom of your library in any order. */

	haste(player, card, event);
	recruiter(player, card, event, SUBTYPE_GOBLIN);
	return 0;
}

// goblin soldier --> rhino token

int card_goblin_trenches(int player, int card, event_t event){
/* Goblin Trenches	|1|R|W
 * Enchantment
 * |2, Sacrifice a land: Put two 1/1 |Sred and |Swhite Goblin Soldier creature tokens onto the battlefield. */

	if (!IS_GAA_EVENT(event)){
		return global_enchantment(player, card, event);
	}
	if (event == EVENT_RESOLVE_ACTIVATION){
		generate_tokens_by_id(player, card, CARD_ID_GOBLIN_SOLDIER, 2);
	}
	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_NONE, MANACOST_X(2), 0, NULL, NULL, TYPE_LAND, NULL, SARV_0);
}

int card_grave_defiler( int player, int card, event_t event){
	/* Grave Defiler	|3|B
	 * Creature - Zombie 2/1
	 * When ~ enters the battlefield, reveal the top four cards of your library. Put all Zombie cards revealed this way into your hand and the rest on the
	 * bottom of your library in any order.
	 * |1|B: Regenerate ~. */

	recruiter(player, card, event, SUBTYPE_ZOMBIE);
	return regeneration(player, card, event, MANACOST_XB(1, 1));
}

int check_deck_for_test(int player, test_definition_t *test){
	int i;
	int test_result = new_get_test_score(test);
	for(i = 0; i < 500 && deck_ptr[player][i] != -1; i++){
		if( new_make_test(-1, deck_ptr[player][i], test_result, test) ){
			return 1;
		}
	}
	return 0;
}

int card_guided_passage(int player, int card, event_t event)
{
  /* Guided Passage |U|R|G
   *   Sorcery
   *   Reveal the cards in your library. An opponent chooses from among them a creature card, a land
   *   card, and a noncreature, nonland card. You put the chosen cards into your hand. Then shuffle your
   *   library. */

  if (event == EVENT_RESOLVE_SPELL)
  {
    int tutored = 0;
    while (tutored < 3)
    {
      test_definition_t this_test;
      if (tutored == 0)
      {
        new_default_test_definition(&this_test, TYPE_CREATURE,
                                    "Select a Creature to add to opponent's hand.");
        this_test.no_shuffle = 1;
      }
      if (tutored == 1)
      {
        new_default_test_definition(&this_test, TYPE_LAND,
                                    "Select a Land to add to opponent's hand.");
        this_test.no_shuffle = 1;
      }
      if (tutored == 2)
      {
        new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_LAND,
                                    "Select a noncreature, nonland card.");
        this_test.type_flag = 1;
        this_test.no_shuffle = 1;
      }
      if (check_deck_for_test(player, &this_test))
      {
        int selected =
            new_select_a_card(1 - player, player, TUTOR_FROM_DECK, 1, AI_MIN_VALUE, -1, &this_test);
        if (selected != -1)
        {
          add_card_to_hand(player, deck_ptr[player][selected]);
          remove_card_from_deck(player, selected);
        }
      }
      tutored++;
    }
    shuffle(player);
    kill_card(player, card, KILL_DESTROY);
  }

  return basic_spell(player, card, event);
}

int card_index2(int player, int card, event_t event){
  /* Index |U
   * Sorcery
   * Look at the top five cards of your library, then put them back in any order. */

	if( event == EVENT_RESOLVE_SPELL ){
		rearrange_top_x(player, player, 5);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_jilt(int player, int card, event_t event){
/* CARD_ID_JILT	5388
Jilt	|1|U
Instant
Kicker |1|R (You may pay an additional |1|R as you cast this spell.)
Return target creature to its owner's hand. If ~ was kicked, it deals 2 damage to another target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_CREATURE") ){
			if( ! is_token(player, card) ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				if( can_target(&td) && do_kicker(player, card, MANACOST_XR(1,1)) ){
					new_pick_target(&td, "TARGET_CREATURE", 1, 1);
				}
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		for (i = 0; i < instance->number_of_targets; i++){
			if( i == 0 && validate_target(player, card, &td, 0) ){
				bounce_permanent(instance->targets[0].player, instance->targets[0].card);
			}
			if( i == 1 && validate_target(player, card, &td, 1) ){
				damage_creature(instance->targets[1].player, instance->targets[1].card, 2, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_kavu_howler( int player, int card, event_t event){
	/* Kavu Howler	|4|G|G
	 * Creature - Kavu 4/5
	 * When ~ enters the battlefield, reveal the top four cards of your library. Put all Kavu cards revealed this way into your hand and the rest on the bottom
	 * of your library in any order. */
	recruiter(player, card, event, SUBTYPE_KAVU);
	return 0;
}

int card_kavu_mauler( int player, int card, event_t event){
/* CARD_ID_KAVU_MAULER	5392
Kavu Mauler	|4|G|G
Creature - Kavu 4/4
Trample
Whenever ~ attacks, it gets +1/+1 until end of turn for each other attacking Kavu. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event == EVENT_DAT_ABILITY)
	{
	  test_definition_t this_test;
	  default_test_definition(&this_test, TYPE_CREATURE);
	  this_test.subtype = SUBTYPE_KAVU;
	  this_test.not_me = 1;
	  this_test.state = STATE_ATTACKING;

	  int amount = check_battlefield_for_special_card(player, card, player, CBFSC_GET_COUNT, &this_test);
	  if (amount > 0)
		pump_until_eot(player, card, player, card, amount, amount);
	}

	return 0;
}

int card_last_stand(int player, int card, event_t event){
	/* Last Stand	|W|U|B|R|G
	 * Sorcery
	 * Target opponent loses 2 life for each |H2Swamp you control.
	 * ~ deals damage equal to the number of |H1Mountains you control to target creature.
	 * Put a 1/1 |Sgreen Saproling creature token onto the battlefield for each |H2Forest you control.
	 * You gain 2 life for each |H2Plains you control.
	 * Draw a card for each |H2Island you control, then discard that many cards. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST && generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL) ){
		return can_target(&td1);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( pick_target(&td, "TARGET_PLAYER") ){
			new_pick_target(&td1, "TARGET_CREATURE", 1, 1);
		}
	}


	if( event == EVENT_RESOLVE_SPELL){
		int good = 0;
		int amount = 0;
		if( validate_target(player, card, &td, 0) ){
			amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_SWAMP));
			lose_life(instance->targets[0].player, amount*2);
			good++;
		}
		if( validate_target(player, card, &td1, 1) ){
			amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_MOUNTAIN));
			if( amount > 0 ){
				damage_creature(instance->targets[1].player, instance->targets[1].card, amount, player, card);
			}
			good++;
		}
		if( good ){
			amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_FOREST));
			if( amount > 0 ){
				generate_tokens_by_id(player, card, CARD_ID_SAPROLING, amount);
			}

			amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_PLAINS));
			gain_life(player, amount*2);

			amount = count_subtype(player, TYPE_LAND, get_hacked_subtype(player, card, SUBTYPE_ISLAND));
			if( amount > 0 ){
				draw_cards(player, amount);
				int j;
				for(j=0;j<amount;j++){
					discard (player, 0, player);
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_lay_of_the_land(int player, int card, event_t event){
  /* Lay of the Land |G
   * 	  Sorcery
   * 	  Search your library for a basic land card, reveal it, put it into your hand, then shuffle your library. */

	if( event == EVENT_RESOLVE_SPELL ){
		tutor_basic_lands(player, TUTOR_HAND, 1);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_legacy_weapon(int player, int card, event_t event){

/* Legacy Weapon	|7
 * Legendary Artifact
 * |W|U|B|R|G: Exile target permanent.
 * If ~ would be put into a graveyard from anywhere, reveal ~ and shuffle it into its owner's library instead. */

// Last ability handled in special_mill() and raw_put_iid_on_top_of_graveyard_with_triggers().

	check_legend_rule(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET, 0, 1, 1, 1, 1, 1, 0, &td, "TARGET_PERMANENT");
}

int card_life_death(int player, int card, event_t event)
{
  /* Life // Death	|G // |1|B
   * Sorcery // Sorcery
   * Life:
   * All lands you control become 1/1 creatures until end of turn. They're still lands.
   *
   * Death:
   * Return target creature card from your graveyard to the battlefield. You lose life equal to its mana value. */

  card_instance_t *instance;
  unsigned int death_available;
  int result;
  unsigned int half_flags;
  char msg[100];
  test_definition_t this_test;

  instance = get_card_instance(player, card);
  if (event == EVENT_MODIFY_COST)
  {
    result = count_graveyard_by_type(player, 2);
    death_available = 0;
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      death_available = (unsigned int)(result == 0);
    }
    generic_split_card(player, card, EVENT_MODIFY_COST, 0x4b0d, 1, 8, 0x4ae7, death_available, 10);
    return 0;
  }
  if (event == EVENT_PLAY_SECOND_HALF)
  {
    memset(msg, 0, sizeof(msg));
    memcpy(msg, "Select a creature card.\000", 24);
    new_default_test_definition(&this_test, 2, msg);
    result = new_select_target_from_grave(player, card, player, SFG_0, 2, &this_test, 0);
    if (result != -1)
    {
      return 0;
    }
    spell_fizzled = 1;
    return 0;
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET)
    return 0;
  result = count_graveyard_by_type(player, 2);
  death_available = 0;
  if (result == 0)
    return generic_split_card(player, card, event, 0x4b0d, 1, 8, 0x4ae7, death_available, 10);
  result = graveyard_has_shroud(player);
  death_available = (unsigned int)(result == 0);
  if (event == EVENT_RESOLVE_SPELL)
  {
    half_flags = instance->info_slot;
    if ((half_flags & 1) != 0)
    {
      default_test_definition(&this_test, 1);
      land_animation2(player, card, player, -1, 1, 1, 1, 0, 0, 0, &this_test);
      half_flags = instance->info_slot;
    }
    if (((half_flags & 2) != 0) &&
        (result = validate_target_from_grave(player, card, player, 0), result != -1))
    {
      result = reanimate_permanent(player, card, player, result, REANIMATE_DEFAULT);
      result = get_cmc(player, result);
      lose_life(player, result);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_split_card(player, card, event, 0x4b0d, 1, 8, 0x4ae7, death_available, 10);
}

int card_living_airship(int player, int card, event_t event){
/* CARD_ID_LIVING_AIRSHIP	5396
Living Airship	|3|U
Creature - Metathran 2/3
Flying
|2|G: Regenerate ~. */

	return regeneration(player, card, event, 2, 0, 0, 1, 0, 0);
}

/* Llanowar Dead	|B|G	=>battle_for_zendikar.c:card_generic_combat_1_mana_producing_creature
 * Creature - Zombie Elf 2/2
 * |T: Add |B to your mana pool. */

int card_mystic_snake(int player, int card, event_t event){
	/* Mystic Snake	|1|G|U|U
	 * Creature - Snake 2/2
	 * Flash
	 * When ~ enters the battlefield, counter target spell. */

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		if( card_counterspell(player, card, event) ){
			instance->info_slot = 1;
			return 99;
		}
		else{
			instance->info_slot = 0;
			return 1;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card)  && current_turn == player){
		ai_modifier -= 10;
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
	if (event == EVENT_ETB_ABILITY){
		if( instance->info_slot == 1 ){
			real_counter_a_spell(player, card, card_on_stack_controller, card_on_stack);
		}
	}
	return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2066c10; original cards/apocalypse.c:1873. */
int card_necra_disciple(int player, int card, event_t event)
{
  /* Necra Disciple	|B
   * Creature - Human Wizard 1/1
   * |G, |T: Add one mana of any color.
   * |W, |T: Prevent the next 1 damage that would be dealt to any target this turn. */

  return ap_disciples(player, card, event, COLOR_TEST_WHITE | COLOR_TEST_GREEN);
}
#endif

int card_necra_sanctuary(int player, int card, event_t event){
  /* Necra Sanctuary |2|B
   * Enchantment
   * At the beginning of your upkeep, if you control a green or white permanent, target player loses 1 life.
   * If you control a green permanent and a white permanent, that player loses 3 life instead. */

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		score |=  2*check_battlefield_for_special_card(player, card, player, 0, &this_test);

		upkeep_trigger_ability_mode(player, card, event, player, score && can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.zone = TARGET_ZONE_PLAYERS;
		td.allow_cancel = 0;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		score |=  2*check_battlefield_for_special_card(player, card, player, 0, &this_test);

		card_instance_t* instance = get_card_instance(player, card);

		instance->number_of_targets = 0;

		if( score > 0 ){
			int pump = 0;
			if( (score & 1) || (score & 2) ){
				pump = 1;
			}
			if( score == 3 ){
				pump = 3;
			}
			if( can_target(&td) && pick_target(&td, "TARGET_PLAYER") ){
				lose_life(instance->targets[0].player, pump);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_necravolver(int player, int card, event_t event){
  /* Necravolver |2|B
   * 	  Creature - Volver 2/2
   * 	  Kicker {1}{G} and/or {W} (You may pay an additional {1}{G} and/or {W} as you cast this spell.)
   * 	  If Necravolver was kicked with its {1}{G} kicker, it enters the battlefield with two +1/+1 counters on it and with trample.
   * 	  If Necravolver was kicked with its {W} kicker, it enters the battlefield with a +1/+1 counter on it and with "Whenever Necravolver deals damage, you gain that much life." */

	volver(player, card, event, MANACOST_XG(1, 1), MANACOST_W(1), 8, 10);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = (check_special_flags(player, card, SF_KICKED) ? 2 : 0) + (check_special_flags(player, card, SF_KICKED2) ? 1 : 0);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, amount);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_TRAMPLE);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED2) ){
		spirit_link_effect(player, card, event, player);
	}

	return 0;
}

int card_orims_thunder(int player, int card, event_t event){
/* CARD_ID_ORIMS_THUNDER	5411
Orim's Thunder	|2|W
Instant
Kicker |R (You may pay an additional |R as you cast this spell.)
Destroy target artifact or enchantment. If ~ was kicked, it deals damage equal to that permanent's converted mana cost to target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT | TYPE_ENCHANTMENT);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( pick_target(&td, "DISENCHANT") ){
			if( ! is_token(player, card) && can_target(&td1) && do_kicker(player, card, MANACOST_R(1)) ){
				new_pick_target(&td1, "TARGET_CREATURE", 1, 1);
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int dmg = get_cmc(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			if( kicked(player, card) && validate_target(player, card, &td1, 1) ){
				damage_creature(instance->targets[1].player, instance->targets[1].card, dmg, player, card);
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}
	return 0;
}

int card_overgrown_estate(int player, int card, event_t event){
/* CARD_ID_OVERGROWN_ESTATE	5412
Overgrown Estate	|B|G|W
Enchantment
Sacrifice a land: You gain 3 life. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		gain_life(player, 3);
	}

	return altar_basic(player, card, event, 0, TYPE_LAND);
}

int card_penumbra_bobcat(int player, int card, event_t event){
	/* Penumbra Bobcat	|2|G
	 * Creature - Cat 2/1
	 * When ~ dies, put a 2/1 |Sblack Cat creature token onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_CAT, &token);
		token.pow = 2;
		token.tou = 1;
		token.color_forced = COLOR_TEST_BLACK;
		generate_token(&token);
	}
	return 0;
}

int card_penumbra_wurm(int player, int card, event_t event){
	/* Penumbra Wurm	|5|G|G
	 * Creature - Wurm 6/6
	 * Trample
	 * When ~ dies, put a 6/6 |Sblack Wurm creature token with trample onto the battlefield. */

	if( this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_WURM, &token);
		token.color_forced = COLOR_TEST_BLACK;
		token.key_plus = KEYWORD_TRAMPLE;
		generate_token(&token);
	}
	return 0;
}

int card_pernicious_deed(int player, int card, event_t event){
  /* Pernicious Deed |1|B|G
   * Enchantment
   * {X}, Sacrifice Pernicious Deed: Destroy each artifact, creature, and enchantment with converted mana cost X or less. */

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, GAA_SACRIFICE_ME, MANACOST0, 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE){
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			charge_mana(player, COLOR_COLORLESS, -1);
			if( spell_fizzled != 1 ){
				card_instance_t* instance = get_card_instance(player, card);
				instance->info_slot = x_value;

				test_definition_t test;
				default_test_definition(&test, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_ENCHANTMENT);
				test.cmc = instance->info_slot+1;
				test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
				ai_modifier += ((check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &test) -
								check_battlefield_for_special_card(player, card, 1-player, CBFSC_GET_COUNT, &test))*5);
				kill_card(player, card, KILL_SACRIFICE);
			}
		}
	}

	if(event == EVENT_RESOLVE_ACTIVATION){
		card_instance_t* instance = get_card_instance(player, card);
		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT|TYPE_CREATURE|TYPE_ENCHANTMENT);
		test.cmc = instance->info_slot+1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
	}

	return global_enchantment(player, card, event);
}

int card_phyrexian_arena(int player, int card, event_t event){
/* CARD_ID_PHYREXIAN_ARENA	1110
Phyrexian Arena	|1|B|B
Enchantment
At the beginning of your upkeep, you draw a card and you lose 1 life. */
		// Used externally
	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		draw_cards(player, 1);
		lose_life(player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_phyrexian_rager(int player, int card, event_t event)
{
  /* CARD_ID_PHYREXIAN_RAGER	5418
Phyrexian Rager	|2|B
Creature - Horror 2/2
When ~ enters the battlefield, you draw a card and you lose 1 life. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    lose_life(player, 1);
    draw_cards(player, 1);
  }
  return 0;
}

int card_phyrexian_gargantua(int player, int card, event_t event)
{
  /* CARD_ID_PHYREXIAN_GARGANTUA	5417
Phyrexian Gargantua	|4|B|B
Creature - Horror 4/4
When ~ enters the battlefield, you draw two cards and you lose 2 life. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    lose_life(player, 2);
    draw_cards(player, 2);
  }
  return 0;
}

int card_planar_despair(int player, int card, event_t event){
/* CARD_ID_PLANAR_DESPAIR	5419
Planar Despair	|3|B|B
Sorcery
Domain - All creatures get -1/-1 until end of turn for each basic land type among lands you control. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p, {pump_subtype_until_eot(player, card, p, -1, -count_domain(player, card), -count_domain(player, card), 0, 0);};);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_powerstone_minefield(int player, int card, event_t event){
/* CARD_ID_POWERSTONE_MINEFIELD	5420
Powerstone Minefield	|2|R|W
Enchantment
Whenever a creature attacks or blocks, ~ deals 2 damage to it. */

	store_attackers(player, card, event, DAT_TRACK, ANYBODY, -1, NULL);
	dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
	    card_instance_t* instance = get_card_instance(player, card);
	    int i;
	    for (i = instance->info_slot - 1; i >= 0; --i){
	        unsigned char *attackers = (unsigned char *)&instance->targets[2].player;
	        if (in_play(current_turn, attackers[i])){ damage_creature(current_turn, attackers[i], 2, player, card); }
	    }
	}
	if (event == EVENT_DECLARE_BLOCKERS){
	    int c;
	    for (c = 0; c < active_cards_count[1-current_turn]; ++c){
	        if (in_play(1-current_turn, c) && is_what(1-current_turn, c, TYPE_CREATURE) && blocking(1-current_turn, c, event)){ damage_creature(1-current_turn, c, 2, player, card); }
	    }
	}
	if (event == EVENT_CHECK_PUMP && current_phase < 0x16){
	    pumpable_toughness[current_turn] -= 2;
	    pumpable_toughness[1-current_turn] -= 2;
	}
	return global_enchantment(player, card, event);
}

int card_prophetic_bolt(int player, int card, event_t event){
	/* Prophetic Bolt	|3|U|R	0x2005471
	 * Instant
	 * ~ deals 4 damage to any target. Look at the top four cards of your library. Put one of those cards into your hand and the rest on the bottom of your library in any order. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
			impulse_effect(player, 4, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_ANY", 1, NULL);
}

int card_putrid_warrior(int player, int card, event_t event){
/* CARD_ID_PUTRID_WARRIOR	5422
Putrid Warrior	|W|B
Creature - Zombie Soldier Warrior 2/2
Whenever ~ deals damage, choose one -
* Each player loses 1 life.
* Each player gains 1 life. */

	if( damage_dealt_by_me(player, card, event, DDBM_MAY_DAMAGE_PLANESWALKER) ){
		int ai_choice = 0;
		if( life[player] == 1 ){
			ai_choice = 1;
		}
		int choice = do_dialog(player, player, card, -1, -1, " Players lose 1 life\n Players gain 1 life", ai_choice);
		if( choice == 0 ){
			lose_life(player, 1);
			lose_life(1-player, 1);
		}
		else{
			gain_life(player, 1);
			gain_life(1-player, 1);
		}
	}
	return 0;
}

int card_quicksilver_dagger(int player, int card, event_t event){
	/* Quicksilver Dagger	|1|U|R	0x2008bd5
	 * Enchantment - Aura
	 * Enchant creature
	 * Enchanted creature has "|T: This creature deals 1 damage to target player or planeswalker. You draw a card." */
	if( in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;

		target_definition_t td1;
		default_target_definition(t_player, t_card, &td1, TARGET_TYPE_PLANESWALKER);
		td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

		if( event == EVENT_CAN_ACTIVATE || event == EVENT_ACTIVATE){
			return generic_activated_ability(t_player, t_card, event, GAA_UNTAPPED | GAA_CAN_TARGET, MANACOST0, 0, &td1, "TARGET_PLAYER_OR_PLANESWALKER");
		}

		if( event == EVENT_RESOLVE_ACTIVATION ){
			if( validate_target(t_player, t_card, &td1, 0) ){
				damage_target0(t_player, t_card, 1);
				draw_cards(t_player, 1);
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = td.preferred_controller = player;

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x20680a0; original cards/apocalypse.c:2278. */
int card_raka_disciple(int player, int card, event_t event)
{
  /* Raka Disciple	|R
   * Creature - Minotaur Wizard 1/1
   * |W, |T: Prevent the next 1 damage that would be dealt to any target this turn.
   * |U, |T: Target creature gains flying until end of turn. */

  return ap_disciples(player, card, event, COLOR_TEST_WHITE | COLOR_TEST_BLUE);
}
#endif

int card_raka_sanctuary(int player, int card, event_t event){
/* CARD_ID_RAKA_SANCTUARY	5426
Raka Sanctuary	|2|R
Enchantment
At the beginning of your upkeep, if you control a |Swhite or |Sblue permanent, ~ deals 1 damage to target creature.
If you control a |Swhite permanent and a |Sblue permanent, ~ deals 3 damage to that creature instead. */

	if( trigger_condition == TRIGGER_UPKEEP && affect_me(player, card) ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		score |=  2*check_battlefield_for_special_card(player, card, player, 0, &this_test);

		upkeep_trigger_ability_mode(player, card, event, player, score && can_target(&td) ? RESOLVE_TRIGGER_MANDATORY : 0);
	}

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 0;

		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_PERMANENT);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLUE);
		int score = 1*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_WHITE);
		score |=  2*check_battlefield_for_special_card(player, card, player, 0, &this_test);
		if( score > 0 ){
			int pump = 0;
			if( (score & 1) || (score & 2) ){
				pump = 1;
			}
			if( score == 3 ){
				pump = 3;
			}
			if( can_target(&td) && pick_target(&td, "TARGET_CREATURE") ){
				instance->number_of_targets = 1;
				damage_creature(instance->targets[0].player, instance->targets[0].card, pump, player, card);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_rakavolver(int player, int card, event_t event){
  /* Rakavolver |2|R
   * 	  Creature - Volver 2/2
   * 	  Kicker {1}{W} and/or {U} (You may pay an additional {1}{W} and/or {U} as you cast this spell.)
   * 	  If Rakavolver was kicked with its {1}{W} kicker, it enters the battlefield with two +1/+1 counters on it and with "Whenever Rakavolver deals damage, you gain that much life."
   * 	  If Rakavolver was kicked with its {U} kicker, it enters the battlefield with a +1/+1 counter on it and with flying. */

	volver(player, card, event, MANACOST_XW(1, 1), MANACOST_U(1), 8, 10);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int amount = (check_special_flags(player, card, SF_KICKED) ? 2 : 0) + (check_special_flags(player, card, SF_KICKED2) ? 1 : 0);
		enters_the_battlefield_with_counters(player, card, event, COUNTER_P1_P1, amount);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED) ){
		spirit_link_effect(player, card, event, player);
	}

	if( in_play(player, card) && check_special_flags(player, card, SF_KICKED2) ){
		modify_pt_and_abilities(player, card, event, 0, 0, KEYWORD_FLYING);
	}

	return 0;
}

int card_soul_link(int player, int card, event_t event)
{
  /* Soul Link	|1|W|B
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature deals damage, you gain that much life.
   * Whenever enchanted creature is dealt damage, you gain that much life. */

  card_instance_t* instance, *damage = damage_being_dealt(event);
  if (damage
	  && (instance = in_play(player, card)) && instance->damage_target_card >= 0
	  && ((damage->damage_source_card == instance->damage_target_card && damage->damage_source_player == instance->damage_target_player)
		  || (damage->damage_target_card == instance->damage_target_card && damage->damage_target_player == instance->damage_target_player)))
	{
	  /* Store damage amounts in targets[3]..targets[10] - backwards compatibility squanders targets[0], aura movement squanders targets[2], and global storage
	   * squanders targets[11]-targets[18]. This leaves us with 64 bytes out of 144. */
	  uint8_t* storage = (void*)(&instance->targets[3].player);
	  if (instance->info_slot < 64 && damage->info_slot <= 255)
		storage[instance->info_slot++] = damage->info_slot;
	  else
		gain_life(player, damage->info_slot);	// No more room, just gain life immediately
	}

  if (trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) && reason_for_trigger_controller == player
	  && (instance = get_card_instance(player, card))->info_slot > 0)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;

	  if (event == EVENT_RESOLVE_TRIGGER)
		{
		  uint8_t* storage = (void*)(&instance->targets[3].player);
		  // Free up info_slot and targets, just in case something triggers instantly (and improperly) which eventually results in dealing damage.
		  uint8_t dam[64];
		  int i, numdam = instance->info_slot;
		  memcpy(dam, storage, 64);
		  instance->info_slot = 0;

		  for (i = 0; i < numdam; ++i)
			gain_life(player, dam[i]);
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	return targeted_aura(player, card, event, &td, "TARGET_CREATURE");
}

int card_spiritmonger(int player, int card, event_t event){

	/* Spiritmonger	|3|B|G
	 * Creature - Beast 6/6
	 * Whenever ~ deals damage to a creature, put a +1/+1 counter on ~.
	 * |B: Regenerate ~.
	 * |G: ~ becomes the color of your choice until end of turn. */

	/* Usually we'd deal with this in TRIGGER_DEAL_DAMAGE, but the counter shouldn't be added until after the damage has fully resolved, which happens *after*
	 * TRIGGER_DEAL_DAMAGE, or else it might raise the Spiritmonger's toughness out of lethal damage range. */
	card_instance_t* damage = damage_being_dealt(event);
	if (damage
		&& damage->damage_source_card == card && damage->damage_source_player == player
		&& damage->damage_target_card != -1 && is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE)
	   ){
		card_instance_t *instance = get_card_instance(player, card);
		if (instance->targets[3].player < 0){
			instance->targets[3].player = 0;
		}
		++instance->targets[3].player;
	}
	if (event == EVENT_AFTER_DAMAGE && get_card_instance(player, card)->targets[3].player > 0){
		int counters = get_card_instance(player, card)->targets[3].player;
		get_card_instance(player, card)->targets[3].player = 0;
		add_counters(player, card, player, card, COUNTER_P1_P1, counters);
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_CHANGE_COLOR = 1,
		CHOICE_REGENERATION
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( regeneration(player, card, event, MANACOST_B(1)) ){
			return 99;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_G(1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_REGENERATION;
		if( ! regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST_B(1)) ){
			choice = CHOICE_CHANGE_COLOR;
		}
		instance->info_slot = choice;
		if( choice == CHOICE_REGENERATION ){
			return regeneration(player, card, event, MANACOST_B(1));
		}
		if( choice == CHOICE_CHANGE_COLOR ){
			charge_mana_for_activated_ability(player, card, MANACOST_G(1));
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATION ){
			return regeneration(player, card, event, MANACOST_B(1));
		}
		if( instance->info_slot == CHOICE_CHANGE_COLOR ){
			int clr = 1<<choose_a_color(player, get_deck_color(player, 1-player));
			change_color(player, instance->parent_card, player, instance->parent_card, clr, CHANGE_COLOR_SET|CHANGE_COLOR_END_AT_EOT|CHANGE_COLOR_NO_SLEIGHT);
		}
	}

	return 0;
}

static int squee_embrace_legacy(int player, int card, event_t event){

	if( resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY) ){
		card_instance_t *instance = get_card_instance(player, card);
		int owner = instance->targets[0].player;
		int iid = instance->targets[0].card;
		int i;
		for(i=count_graveyard(owner)-1; i > -1; i--){
			if( get_grave(owner)[i] == iid ){
				add_card_to_hand(owner, iid);
				remove_card_from_grave(owner, i);
				break;
			}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_squees_embrace(int player, int card, event_t event){
/* CARD_ID_SQUEES_EMBRACE	5436
Squee's Embrace	|R|W
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2.
When enchanted creature dies, return that card to its owner's hand. */

	if (in_play(player, card) && get_card_instance(player, card)->damage_target_player != -1){
		card_instance_t* instance = get_card_instance(player, card);
		int p = instance->damage_target_player, c = instance->damage_target_card;
		if (attached_creature_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY)){
			int legacy = create_legacy_effect(player, card, squee_embrace_legacy);
			card_instance_t* effect = get_card_instance(player, legacy);
			effect->targets[0].player = get_owner(p, c);
			effect->targets[0].card = get_original_internal_card_id(p, c);
			effect->targets[11].player = 1;
		}
	}
	return generic_aura(player, card, event, player, 2, 2, 0, 0, 0, 0, 0);
}

int card_squees_revenge(int player, int card, event_t event){
/* CARD_ID_SQUEES_REVENGE	5437
Squee's Revenge	|1|U|R
Sorcery
Choose a number. Flip a coin that many times or until you lose a flip, whichever comes first. If you win all the flips, draw two cards for each flip. */

	if( event == EVENT_RESOLVE_SPELL ){
		int flips = choose_a_number(player, "Flip how many coins?", 2);
		int count = 0;
		int result = 0;
		while( count < flips ){
				if( flip_a_coin(player, card) ){
					result++;
				}
				else{
					break;
				}
				count++;
		}
		if( result == flips ){
			draw_cards(player, flips*2);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_strength_of_night(int player, int card, event_t event){
/* CARD_ID_STRENGTH_OF_NIGHT	5439
Strength of Night	|2|G
Instant
Kicker |B (You may pay an additional |B as you cast this spell.)
Creatures you control get +1/+1 until end of turn. If ~ was kicked, Zombie creatures you control get an additional +2/+2 until end of turn. */

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
		if( kicked(player, card) ){
			pump_subtype_until_eot(player, card, player, SUBTYPE_ZOMBIE, 2, 2, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return kicker(player, card, event, MANACOST_B(1));
}

int card_suffocating_blast(int player, int card, event_t event){
  /* Suffocating Blast |1|U|U|R
   * Instant
   * Counter target spell and Suffocating Blast deals 3 damage to target creature. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		if( counterspell(player, card, event, NULL, 0) && can_target(&td) ){
			return 0x63;
		}
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		counterspell(player, card, event, NULL, 0);
		if (can_target(&td)){ new_pick_target(&td, "TARGET_CREATURE", 1, 1); }
	}

	if( event == EVENT_RESOLVE_SPELL ){
		counterspell(player, card, event, NULL, 0);
		instance->number_of_targets = 2;
		if( validate_target(player, card, &td, 1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, 3, player, card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

static int suppress_legacy(int player, int card, event_t event){

	card_instance_t *instance = get_card_instance(player, card);

	/* targets[0].player: low byte contains player to return cards to;
	 *                    low bit of BYTE1 is set if this is the same turn the spell was cast.
	 * targets[0].card, targets[1..18].player, targets[1..18].card: contain internal_card_id's of exiled cards. */

	if (event == EVENT_BEGIN_TURN){
		instance->targets[0].player &= ~0x100;
	}
	if (current_turn == instance->targets[0].player && eot_trigger(player, card, event)){
		instance->targets[0].player = -1;	// Not a card id
		int i;
		for (i = 18; i >= 0; --i){
			if (instance->targets[i].card != -1){
				int id = cards_data[instance->targets[i].card].id;
				if( check_rfg(current_turn, id) ){
					add_card_to_hand(current_turn, instance->targets[i].card);
					remove_card_from_rfg(current_turn, id);
				}
			}
			if (instance->targets[i].player != -1){
				int id = cards_data[instance->targets[i].player].id;
				if( check_rfg(current_turn, id) ){
					add_card_to_hand(current_turn, instance->targets[i].player);
					remove_card_from_rfg(current_turn, id);
				}
			}
		}
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_suppress(int player, int card, event_t event){
/* CARD_ID_SUPPRESS	5441
Suppress	|2|B
Sorcery
Target player exiles all cards from his or her hand face down.
At the beginning of the end step of that player's next turn, that player returns those cards to his or her hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return can_target(&td);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		pick_target(&td, "TARGET_PLAYER");
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			if( hand_count[instance->targets[0].player] > 0 ){
				int legacy = create_legacy_effect(player, card, &suppress_legacy);
				card_instance_t *leg = get_card_instance(player, legacy);
				int removed = 0;
				int count = active_cards_count[instance->targets[0].player]-1;
				leg->targets[0].player = instance->targets[0].player;
				leg->targets[0].player |= 0x100;	// prevents legacy from triggering at end of this turn, since it won't match current_turn anymore
				while( count > -1 ){
						if( in_hand(instance->targets[0].player, count) && removed < 19 ){
							card_instance_t *crd = get_card_instance(instance->targets[0].player, count);
							if (leg->targets[removed].player == -1){
								leg->targets[removed].player = crd->internal_card_id;
							} else {
								leg->targets[removed].card = crd->internal_card_id;
								++removed;
							}
							rfg_card_in_hand(instance->targets[0].player, count);
							removed++;
						}
						count--;
				}
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_sylvan_messenger( int player, int card, event_t event){
	/* Sylvan Messenger	|3|G
	 * Creature - Elf 2/2
	 * Trample
	 * When ~ enters the battlefield, reveal the top four cards of your library. Put all Elf cards revealed this way into your hand and the rest on the bottom
	 * of your library in any order. */
	recruiter(player, card, event, SUBTYPE_ELF);
	return 0;
}

int card_symbiotic_deployment(int player, int card, event_t event){
/* CARD_ID_SYMBIOTIC_DEPLOYMENT	5443
Symbiotic Deployment	|2|G
Enchantment
Skip your draw step.
|1, Tap two untapped creatures you control: Draw a card. */

	skip_your_draw_step(player, event);

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;
	td.illegal_abilities = 0;
	td.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
			return target_available(player, card, &td) > 1 ? 1 : 0;
		}
	}

	if(event == EVENT_ACTIVATE ){
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			if( pick_target(&td, "TARGET_CREATURE") ){
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 1);
				if( new_pick_target(&td, "TARGET_CREATURE", 1, 1) ){
					tap_card(instance->targets[0].player, instance->targets[0].card);
					tap_card(instance->targets[1].player, instance->targets[1].card);
				}
				state_untargettable(instance->targets[0].player, instance->targets[0].card, 0);
			}
		}
		instance->number_of_targets = 0;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return 0;
}

int card_temporal_spring(int player, int card, event_t event){
/* CARD_ID_TEMPORAL_SPRING	5445
Temporal Spring	|1|G|U
Sorcery
Put target permanent on top of its owner's library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_PERMANENT);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PERMANENT", 1, NULL);
}

int card_tidal_courier( int player, int card, event_t event){
	/* Tidal Courier	|3|U
	 * Creature - Merfolk 1/2
	 * When ~ enters the battlefield, reveal the top four cards of your library. Put all Merfolk cards revealed this way into your hand and the rest on the
	 * bottom of your library in any order.
	 * |3|U: ~ gains flying until end of turn. */
	recruiter(player, card, event, SUBTYPE_MERFOLK);
	return generic_shade(player, card, event, 0, MANACOST_XU(3, 1), 0, 0, KEYWORD_FLYING, 0);
}

int card_urborg_uprising(int player, int card, event_t event)
{
  /* Urborg Uprising	|4|B
   * Sorcery
   * Return up to two target creature cards from your graveyard to your hand.
   * Draw a card. */
  if (!IS_GS_EVENT(player, card, event))
	return 0;
  test_definition_t test;
  new_default_test_definition(&test, TYPE_CREATURE, "Select up to two target creature cards.");
  int rval = spell_return_up_to_n_target_cards_from_your_graveyard_to_your_hand(player, card, event, 2, &test, 1);
  if (event == EVENT_RESOLVE_SPELL)
	{
	  if (spell_fizzled != 1)
		draw_a_card(player);
	  kill_card(player, card, KILL_DESTROY);
	}
  return rval;
}

/* ManalinkEh 2022: 0x2069960; original cards/apocalypse.c:2829. */
int card_whirlpool_rider(int player, int card, event_t event)
{
  /* Whirlpool Rider	|1|U
   * Creature - Merfolk 1/1
   * When this creature enters, shuffle the cards from your hand into your library, then draw that many cards. */

  int card_00;
  card_00 = player;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  whirlpool_rider_part(player, card_00, card);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2060dd0; original cards/apocalypse.c:2829. */
static int whirlpool_rider_part(int player, int card, event_t event)
{
  int result;

  result = reshuffle_hand_into_deck(player, 0);
  result = draw_cards(player, result);
  return result;
}
#endif

/* ManalinkEh 2022: 0x20699b0; original cards/apocalypse.c:2844. */
int card_whirlpool_drake(int player, int card, event_t event)
{
  /* Whirlpool Drake	|3|U
   * Creature - Drake 2/2
   * Flying
   * When this creature enters, shuffle the cards from your hand into your library, then draw that many cards.
   * When this creature dies, shuffle the cards from your hand into your library, then draw that many cards. */

  int result;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    result = player;
    etb_ability(player, card, EVENT_ETB, RESOLVE_TRIGGER_MANDATORY);
    whirlpool_rider_part(player, result, card);
    return 0;
  }
  result = this_dies_trigger(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result == 0)
  {
    return 0;
  }
  result = reshuffle_hand_into_deck(player, 0);
  draw_cards(player, result);
  return 0;
}

/* ManalinkEh 2022: 0x2069a60; original cards/apocalypse.c:2866. */
int card_whirlpool_warrior(int player, int card, event_t event)
{
  /* Whirlpool Warrior	|2|U
   * Creature - Merfolk Warrior 2/2
   * When this creature enters, shuffle the cards from your hand into your library, then draw that many cards.
   * |R, Sacrifice this creature: Each player shuffles the cards from their hand into their library, then draws that many cards. */

  int amount;
  int result;
  unsigned int player_00;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  result = current_turn;
  if (event == EVENT_ETB)
  {
    result = player;
    etb_ability(player, card, EVENT_ETB, RESOLVE_TRIGGER_MANDATORY);
    whirlpool_rider_part(player, result, card);
    return 0;
  }
  if ((event != EVENT_CAN_ACTIVATE) && (event != EVENT_ACTIVATE))
  {
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return 0;
    }
    amount = reshuffle_hand_into_deck(current_turn, 0);
    draw_cards(result, amount);
    player_00 = current_turn ^ 1;
    result = reshuffle_hand_into_deck(player_00, 0);
    draw_cards(player_00, result);
    return 0;
  }
  return generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 1, 0, 0, NULL, NULL);
}

int card_wild_research(int player, int card, event_t event){
  /* Wild Research |2|R
   * Enchantment, 2R (3)
   * {1}{W}: Search your library for an enchantment card and reveal that card. Put it into your hand, then discard a card at random.
   * 		Then shuffle your library.
   * {1}{U}: Search your library for an instant card and reveal that card. Put it into your hand, then discard a card at random.
   * 		Then shuffle your library. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	enum{
		CHOICE_TUTOR_ENCHANTMENT = 1,
		CHOICE_TUTOR_INSTANT
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, 0, MANACOST_XW(1, 1), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, 0, MANACOST_XU(1, 1), 0, NULL, NULL);
	}

	if(event == EVENT_ACTIVATE ){
		instance->info_slot = 0;
		ai_modifier += ((hand_count[player]*5)-10);
		int choice = CHOICE_TUTOR_ENCHANTMENT;
		if( has_mana_for_activated_ability(player, card, MANACOST_XW(1, 1)) ){
			if( has_mana_for_activated_ability(player, card, MANACOST_XU(1, 1)) ){
				choice = DIALOG(player, card, event, DLG_RANDOM,
								"Tutor an Enchantment", 1, 1,
								"Tutor an Instant", 	1, 1);
				if( ! choice ){
					spell_fizzled = 1;
					return 0;
				}
			}
		}
		else{
			choice = CHOICE_TUTOR_INSTANT;
		}
		instance->info_slot = choice;
		int blue = choice == CHOICE_TUTOR_INSTANT ? 1 : 0;
		int white = choice == CHOICE_TUTOR_ENCHANTMENT ? 1 : 0;
		charge_mana_for_activated_ability(player, card, 1, 0, blue, 0, 0, white);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_INSTANT | TYPE_INTERRUPT, "Select an Instant card.");
		if( instance->info_slot == CHOICE_TUTOR_ENCHANTMENT ){
			new_default_test_definition(&this_test, TYPE_ENCHANTMENT, "Select an Enchantment card.");
			this_test.type_flag = F1_NO_PWALKER;
		}
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		discard(player, DISC_RANDOM, player);
	}

	return 0;
}
