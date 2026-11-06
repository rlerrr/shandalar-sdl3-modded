#include "manalink.h"

/* Canonical recovered helper declarations. */
const char *colored_damage_source_that_damages_me(int who_chooses, int player, int card,
                                                 int targeting_player, int targeting_card);

// Functions

static char *target_must_be_damage_targeting_this_revered_elder(int who_chooses, int player,
                                                                int card, int targeting_player,
                                                                int targeting_card);
static int creatures_target_player_controls_attack_this_turn_if_able(int player, int card,
                                                                     event_t event);
static int are_blocked_creatures(int unused, int unused2, int player, int card);
static int target_lands_become_forests(int player, int card, event_t event);
int destroy_target_blocking_creature_at_end_of_combat(int player, int card, event_t event);

static int wishmonger_effect(int player, int card, event_t event);
static int scandalmonger_effect(int player, int card, event_t event);
int effect_squallmonger(int player, int card, event_t event);
int effect_warmonger(int player, int card, event_t event);

static void whenever_a_creature_blocks_black_red(int color_test, int val, int t_player, int t_card);

static int cast_creature_and_enchantment_spells_as_they_had_flash(int player, int card,
                                                                  event_t event);

static int mercadian_storage_land(int player, int card, event_t event, color_t color);

int generic_subtype_searcher(int player, int card, event_t event, int subtype, int val, int mode){
	// mode = 0 --> CMC < val
	// mode = 1 --> CMC <= val
	// mode = 2 --> any CMC

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		card_instance_t *parent = get_card_instance(instance->parent_controller, instance->parent_card);

		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_PERMANENT, "");
		this_test.subtype = subtype;
		if( mode != 2 ){
			this_test.cmc = val+mode;
			this_test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
			if (ai_is_speculating != 1){
				scnprintf(this_test.message, 100, "Select %s permanent card with CMC %d or less.", get_subtype_text("%a", subtype), val + mode - 1);
			}
		} else if (ai_is_speculating != 1){
			strcpy(this_test.message, get_subtype_text("Select %a permanent card.", subtype));
		}
		parent->targets[2].card = 2+new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &this_test);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DECK_SEARCHER, MANACOST_X(val), 0, NULL, NULL);
}

static int part_of_ramos(int player, int card, event_t event, int manacolor)
{

  if (!IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)))
  {
    return mana_producer(player, card, event);
  }

  if (event == EVENT_COUNT_MANA && affect_me(player, card))
  {
    if (!is_tapped(player, card) && !is_animated_and_sick(player, card))
    {
      declare_mana_available(player, manacolor, 2);
    }
    else
    {
      declare_mana_available(player, manacolor, 1);
    }
  }

  if (event == EVENT_CAN_ACTIVATE)
  {
    return can_produce_mana(player, card);
  }

  if (event == EVENT_ACTIVATE)
  {
    int choice = 0;
    if (!is_tapped(player, card) && !is_animated_and_sick(player, card))
    {
      if (player != AI)
      {
        choice = do_dialog(player, player, card, -1, -1,
                           " Tap to add mana\n Sac to add mana\n Cancel", 0);
      }
    }
    else
    {
      choice = 1;
    }

    if (choice == 0)
    {
      return mana_producer(player, card, event);
    }

    else if (choice == 1)
    {
      ai_modifier -= 36;

      produce_mana(player, manacolor, 1);
      tapped_for_mana_color = -2;
      kill_card(player, card, KILL_SACRIFICE);
    }
    else
    {
      spell_fizzled = 1;
    }
  }

  return 0;
}

static int depletion_land(int player, int card, event_t event, int manacolor)
{
  /* Enters tapped with two depletion counters. Spending the depletion ability
   * produces two mana and sacrifices the land when its last counter is gone. */
  comes_into_play_tapped(player, card, event);
  enters_the_battlefield_with_counters(player, card, event, COUNTER_DEPLETION, 2);
  if (event == EVENT_RESOLVE_SPELL)
    play_land_sound_effect(player, card);
  if (event == EVENT_CAN_ACTIVATE)
    return CAN_TAP_FOR_MANA(player, card) &&
           (count_counters(player, card, COUNTER_DEPLETION) || mana_colors_added[player]);
  if (event == EVENT_ACTIVATE)
  {
    if (is_what(player, card, TYPE_LAND) && mana_colors_added[player])
    {
      int choice = 0;
      if (count_counters(player, card, COUNTER_DEPLETION) >= 1)
        choice = do_dialog(player, player, card, -1, -1,
                           " Basic land ability\n Depletion Mana ability\n Cancel", 1);
      if (choice == 0)
      {
        produce_mana_tapped_all_one_color_impl(player, card, mana_colors_added[player], 1, 0);
        return 0;
      }
      if (choice != 1)
      {
        if (choice == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (count_counters(player, card, COUNTER_DEPLETION) == 0)
          kill_card(player, card, KILL_SACRIFICE);
        return 0;
      }
    }
    if (count_counters(player, card, COUNTER_DEPLETION) == 1)
      ai_modifier -= 36;
    remove_counters(player, card, player, card, COUNTER_DEPLETION, 1);
    produce_mana_tapped_impl(player, card, manacolor, 2, 1, 1);
    if (count_counters(player, card, COUNTER_DEPLETION) == 0)
      kill_card(player, card, KILL_SACRIFICE);
  }
  if (event == EVENT_COUNT_MANA && affect_me(player, card) && CAN_TAP_FOR_MANA(player, card) &&
      count_counters(player, card, COUNTER_DEPLETION))
  {
    int color_test = color_to_color_test(manacolor);
    int num_mana;
    if (!check_special_flags2(player, card, SF2_CONTAMINATION))
    {
      num_mana = 2;
      color_test |= mana_colors_added[player];
    }
    else
    {
      num_mana = mana_quantity_override[player][manacolor];
      if (num_mana < 0)
        num_mana = 2;
      color_test = mana_color_override[player][manacolor];
    }
    if (multi_mana_transmutation[player][0] && num_mana > 1 && is_what(player, card, TYPE_LAND))
    {
      num_mana = multi_mana_transmutation[player][1];
      declare_mana_available(
          player, single_color_test_bit_to_color(multi_mana_transmutation[player][0]), num_mana);
    }
    else
    {
      declare_mana_available_maybe_hex(player, color_test, num_mana);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216d7f0; original cards/mercadian_masques.c:176. */
static int mercadian_storage_land(int player, int card, event_t event, color_t color)
{
  enum
  {
    CHOICE_COUNTERS = 1,
    CHOICE_REMOVE_COUNTERS = 2
  };

  int choice;
  int storage_or_available;
  card_instance_t *instance;
  int selected_or_priority;
  int add_available_or_priority;

  comes_into_play_tapped(player, card, event);
  if (event == EVENT_RESOLVE_SPELL)
  {
    play_land_sound_effect(player, card);
  }
  else
  {
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
      if (mana_producer(player, card, EVENT_CAN_ACTIVATE) == 0)
      {
        return 0;
      }

      storage_or_available = count_counters(player, card, COUNTER_STORAGE);
      if (storage_or_available < 1)
      {
        return 0;
      }
      declare_mana_available(player, color, storage_or_available);
      return 0;
    }
    if (event == EVENT_CAN_ACTIVATE)
    {
      storage_or_available = paying_mana();
      if ((storage_or_available == 0) &&
          (storage_or_available = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0,
                                                            0, 0, 0, 0, 0, 0, NULL, NULL),
           storage_or_available != 0))
      {
        return 1;
      }
      storage_or_available = count_counters(player, card, COUNTER_STORAGE);
      if (storage_or_available < 1)
      {
        return 0;
      }
      return mana_producer(player, card, EVENT_CAN_ACTIVATE);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance = get_card_instance(player, card);
      instance->info_slot = 0;
      storage_or_available = count_counters(player, card, COUNTER_STORAGE);
      selected_or_priority = paying_mana();
      if (selected_or_priority == 0 && storage_or_available == 0)
      {
        add_available_or_priority = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1,
                                                              0, 0, 0, 0, 0, 0, 0, NULL, NULL);
        selected_or_priority = 5;
        add_available_or_priority =
            ((add_available_or_priority == 0) ? 2U : 10U);
      }
      else
      {
        add_available_or_priority = 2;
        selected_or_priority = (unsigned int)(1 < storage_or_available) * 5 + 5;
      }
      choice =
          dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Add a counter", 1,
                      add_available_or_priority, "Remove X counters/produce X mana",
                      (unsigned int)(0 < storage_or_available), selected_or_priority, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice != CHOICE_COUNTERS)
      {
        if (choice != CHOICE_REMOVE_COUNTERS)
        {
          return 0;
        }
        if (generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                      NULL) == 0)
        {
          return 0;
        }
        do
        {

          selected_or_priority =
              choose_a_number(player, "Remove how many counters?", storage_or_available);
        } while (storage_or_available < selected_or_priority);
        if (selected_or_priority != -1)
        {
          remove_counters(player, card, player, card, COUNTER_STORAGE, selected_or_priority);
          produce_mana_tapped_impl(player, card, color, selected_or_priority, 0, 1);
          return 0;
        }
        untap_card_no_event(player, card);
        spell_fizzled = 1;
        return 0;
      }
      add_state(player, card, 0x10);
      if (generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) != 0)
      {
        instance = get_card_instance(player, card);
        instance->info_slot = 1;
        ai_modifier = ai_modifier + 0x30;
      }
      if (spell_fizzled != 1)
      {
        return 0;
      }
      untap_card_no_event(player, card);
      return 0;
    }
  }

  if ((event == EVENT_RESOLVE_ACTIVATION) &&
      (instance = get_card_instance(player, card), instance->info_slot == 1))
  {
    add_counters(player, card, instance->parent_controller, instance->parent_card, COUNTER_STORAGE,
                 1);
    return 0;
  }
  return 0;
}

static int rishadan_delinquents(int player, int card, event_t event, int mana_req){

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int sac = 1;
		if( has_mana(1-player, COLOR_COLORLESS, mana_req) ){
			charge_mana(1-player, COLOR_COLORLESS, mana_req);
			if( spell_fizzled != 1 ){
				sac = 0;
			}
		}
		if( sac == 1 ){
			test_definition_t test;
			new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice.");
			new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
		}
	}

	return 0;
}

// Cards

int card_alley_grifters(int player, int card, event_t event ){
/* CARD_ID_ALLEY_GRIFTERS	4382
Alley Grifters	|1|B|B
Creature - Human Mercenary 2/2
Whenever ~ becomes blocked, defending player discards a card. */

	if( event == EVENT_DECLARE_BLOCKERS && is_attacking(player, card) && !is_unblocked(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		discard(1-player, 0, player);
	}
	return 0;
}

int card_ancestral_mask(int player, int card, event_t event ){
/* CARD_ID_ANCESTRAL_MASK	4383
Ancestral Mask	|2|G
Enchantment - Aura
Enchant creature
Enchanted creature gets +2/+2 for each other enchantment on the battlefield. */

	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance( player, card );
		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(instance->damage_target_player, instance->damage_target_card) ){
			int plus = 2*(count_subtype(2, TYPE_ENCHANTMENT, -1)-1);
			if( plus < 0 ){
				plus = 0;
			}
			event_result+=plus;
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return vanilla_aura(player, card, event, player);
}

int card_arms_dealer(int player, int card, event_t event ){
  /* Arms Dealer |2|R
   * Creature — Goblin Rogue 1/1
   * {1}{R}, Sacrifice a Goblin: Arms Dealer deals 4 damage to target creature. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	test_definition_t test;
	new_default_test_definition(&test, TYPE_PERMANENT, "Select a Goblin to sacrifice.");
	test.subtype = SUBTYPE_GOBLIN;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 4);
		}
	}

	return generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET, MANACOST_XR(1, 1), 0,
															&td, "TARGET_CREATURE", 0, &test, 0);
}

int card_arrest(int player, int card, event_t event)
{
  /* CARD_ID_ARREST	4386
   * Arrest	|2|W
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature can't attack or block, and its activated abilities can't be activated. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (((instance != NULL) && (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      (instance = get_card_instance(player, card), -1 < instance->damage_target_player))
  {
    instance = get_card_instance(player, card);
    cannot_attack((int)instance->damage_target_player, instance->damage_target_card, event);
  }
  if (event != EVENT_CAN_CAST &&
      !(event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller) &&
      event != EVENT_RESOLVE_SPELL && event != EVENT_CAN_CHANGE_TARGET &&
      event != EVENT_CHANGE_TARGET &&
      (event & ~EVENT_SET_LEGACY_EFFECT_NAME) != EVENT_CAN_MOVE_AURA && (uint32_t)(event - EVENT_POWER) > 2 &&
      event != EVENT_RESOLVE_MOVING_AURA && event != EVENT_CHANGE_TYPE &&
      event != EVENT_SET_COLOR && event != EVENT_GET_SELECTED_CARD && event != EVENT_STATIC_EFFECTS)
    return 0;
  disable_attached(player, card, event, 0x38);
  attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_CANNOT_BLOCK);
  result = aura(player, card, event, NULL, NULL);
  disabling_aura_ai(player, card, event);
  return result;
}

int card_bargaining_table(int player, int card, event_t event ){
/* CARD_ID_BARGAINING_TABLE	4391
Bargaining Table	|5
Artifact
|X, |T: Draw a card. X is the number of cards in an opponent's hand. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(hand_count[1-player]), 0, NULL, NULL);
}

int card_bifurcate(int player, int card, event_t event){
/* CARD_ID_BIFURCATE	4394
Bifurcate	|3|G
Sorcery
Search your library for a permanent card with the same name as target nontoken creature and put that card onto the battlefield. Then shuffle your library. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_type = TARGET_TYPE_TOKEN;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nontoken creature", 1, NULL);
		if( spell_fizzled != 1 ){
			if( get_card_name(instance->targets[0].player, instance->targets[0].card) == -1 ){
				ai_modifier -= 128;
			}
		}
		return 0;
	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			tutor_card_with_the_same_name(instance->targets[0].player, instance->targets[0].card, player, player, TUTOR_FROM_DECK,
											TUTOR_PLAY, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select target nontoken creature", 1, NULL);
}

int card_black_market(int player, int card, event_t event)
{
  /* Black Market	|3|B|B
   * Enchantment
   * Whenever a creature dies, put a charge counter on ~.
   * At the beginning of your precombat main phase, add |B to your mana pool for each charge counter on ~. */

  unsigned int pending_deaths;
  int result;

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability(player, card, 0x77, 2, 2, NULL);
  }
  pending_deaths = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (pending_deaths != 0)
  {
    for (pending_deaths = pending_deaths & 0xff; pending_deaths != 0;
         pending_deaths = pending_deaths - 1)
    {
      add_counters(player, card, player, card, COUNTER_CHARGE, 1);
    }
  }
  result = beginning_of_phase(player, card, event, player, card, 0x14);
  if (result != 0)
  {
    result = count_counters(player, card, COUNTER_CHARGE);
    produce_mana(player, 1, result);
  }
  return global_enchantment(player, card, event);
}

int card_blood_oath(int player, int card, event_t event)
{
  /* CARD_ID_BLOOD_OATH	4399
   * Blood Oath	|3|R
   * Instant
   * Choose a card type. Target opponent reveals his or her hand. ~ deals 3 damage to that player for each card of the chosen type revealed this way.
   * (Artifact, creature, enchantment, instant, land, planeswalker, sorcery, and tribal are card types.) */

  int choice;
  unsigned int test_type;
  card_instance_t *instance;
  int hand_player;
  card_instance_t *hand_card;
  int matches_type;
  int card_00;
  int matching_cards;
  int types[8] = {1, 2, 4, 8, 16, 64, 256, 512};
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td) != 0)
    {
      choice = do_dialog(player, player, card, -1, -1,
                         " Land\n Creature\n Enchantment\n Sorcery\n Instant\n Artifact\n "
                         "Planeswalker\n Tribal",
                         internal_rand(8));
      test_type = types[choice];
      hand_player = instance->targets[0].player;
      if (active_cards_count[hand_player] < 1)
      {
        reveal_target_player_hand(hand_player);
      }
      else
      {
        matching_cards = 0;
        card_00 = 0;
        do
        {
          hand_card = in_hand(hand_player, card_00);
          if (hand_card != NULL)
          {
            if ((test_type & 0x100) == 0)
            {
              if ((test_type & 0x200) == 0)
              {
                matches_type = is_what(hand_player, card_00, test_type);
                matching_cards = (matching_cards + 1) - (unsigned int)(matches_type == 0);
              }
              else
              {
                if (is_what(hand_player, card_00, 2) == 0)
                {
                  matches_type = is_tribal(hand_player, card_00);
                  matching_cards = (matching_cards + 1) - (unsigned int)(matches_type == 0);
                }
              }
            }
            else
            {
              matches_type = is_planeswalker(hand_player, card_00);
              matching_cards = (matching_cards + 1) - (unsigned int)(matches_type == 0);
            }
          }
          card_00 = card_00 + 1;
        } while (card_00 < active_cards_count[hand_player]);
        reveal_target_player_hand(instance->targets[0].player);
        if (0 < matching_cards)
        {
          damage_player(instance->targets[0].player, matching_cards * 3, player, card);
        }
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x40, &td, NULL, 1, NULL);
}

int card_bog_witch(int player, int card, event_t event ){
/* CARD_ID_BOG_WITCH	4402
Bog Witch	|2|B
Creature - Human Spellshaper 1/1
|B, |T, Discard a card: Add |B|B|B to your mana pool. */

	if( ! IS_GAA_EVENT(event) && !(event == EVENT_COUNT_MANA && affect_me(player, card)) ){
		return 0;
	}

	int active = can_produce_mana(player, card) && has_mana(player, COLOR_BLACK, 1) && ! is_tapped(player, card) &&
				! is_sick(player, card);

	if( event == EVENT_CAN_ACTIVATE && active && hand_count[player] > 0 ){
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		card_instance_t* instance = get_card_instance(player, card);
		instance->state |= STATE_TAPPED;
		charge_mana(player, COLOR_BLACK, 1);
		if( spell_fizzled == 1 ){
			instance->state &= ~STATE_TAPPED;
		}
		else{
			discard(player, 0, player);
			produce_mana_tapped(player, card, COLOR_BLACK, 3);
		}
	}

	if( event == EVENT_COUNT_MANA ){
		if( affect_me(player, card) && active && hand_count[player] > 0 ){
			declare_mana_available(player, COLOR_BLACK, 3);
		}
	}

	return 0;
}

int card_briar_patch(int player, int card, event_t event)
{
/* CARD_ID_BRIAR_PATCH	4404
Briar Patch	|1|G|G
Enchantment
Whenever a creature attacks you, it gets -1/-0 until end of turn. */

	store_attackers(player, card, event, DAT_TRACK, 1-player, -1, NULL);
	dat_ability(player, card, event, DAT_TRACK | DAT_RESOLVE_TRIGGER_MANDATORY, 1);
	if (event == EVENT_DAT_ABILITY){
		card_instance_t* instance = get_card_instance(player, card);
		unsigned char* attackers = (unsigned char*)&instance->targets[2].player;
		for (int i = instance->info_slot - 1; i >= 0; --i){
			if (in_play(current_turn, attackers[i])){
				pump_until_eot(player, card, current_turn, attackers[i], -1, 0);
			}
		}
	}else if (event == EVENT_CHECK_PUMP && current_turn == 1-player && current_phase < PHASE_BEFORE_BLOCKING){
		pumpable_power[1-player] -= 1;
	}
	return global_enchantment(player, card, event);
}

int card_bribery(int player, int card, event_t event){
/* CARD_ID_BRIBERY	985
Bribery	|3|U|U
Sorcery
Search target opponent's library for a creature card and put that card onto the battlefield under your control. Then that player shuffles his or her library. */

	// original code : 0040F730
	if( IS_GS_EVENT(player, card, event) ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card");
		return steal_permanent_from_target_opponent_deck(player, card, event, &this_test);
	}
	return 0;
}

int card_cackling_witch(int player, int card, event_t event){
/* CARD_ID_CACKLING_WITCH	4406
Cackling Witch	|1|B
Creature - Human Spellshaper 1/1
|X|B, |T, Discard a card: Target creature gets +X/+0 until end of turn. */

	if (event == EVENT_CHECK_ABILITIES){
		if (affected_card_controller == player && card_cackling_witch(player, card, EVENT_CAN_ACTIVATE)){
			int available = has_mana(player, COLOR_ANY, 1);
			if (available - 1 > 0){ check_abilities_power += available - 1; }
		}
		return 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card,
							instance->targets[0].player, instance->targets[0].card, instance->info_slot, 0);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_XB(-1, 1), 0,
									&td, "TARGET_CREATURE");
}

int card_cateran_brute(int player, int card, event_t event ){
/* CARD_ID_CATERAN_BRUTE	4408
Cateran Brute	|2|B
Creature - Horror Mercenary 2/2
|2, |T: Search your library for a Mercenary permanent card with converted mana cost 2 or less and put it onto the battlefield. Then shuffle your library. */

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 2, 1);
}

int card_cateran_enforcer(int player, int card, event_t event ){
/*CARD_ID_CATERAN_ENFORCER	4409
Cateran Enforcer	|3|B|B
Creature - Horror Mercenary 4/3
Fear (This creature can't be blocked except by artifact creatures and/or black creatures.)
|4, |T: Search your library for a Mercenary permanent card with converted mana cost 4 or less and put it onto the battlefield. Then shuffle your library. */

	fear(player, card, event);

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 4, 1);
}

int card_cateran_kidnapper(int player, int card, event_t event ){
/* CARD_ID_CATERAN_KIDNAPPERS	4410
Cateran Kidnappers	|2|B|B
Creature - Human Mercenary 4/2
|3, |T: Search your library for a Mercenary permanent card with converted mana cost 3 or less and put it onto the battlefield. Then shuffle your library. */

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 3, 1);
}

int card_cateran_overlord(int player, int card, event_t event ){
  /* Cateran Overlord |4|B|B|B
   * Creature — Horror Mercenary 7/5
   * Sacrifice a creature: Regenerate Cateran Overlord.
   * {6}, {T}: Search your library for a Mercenary permanent card with converted mana cost 6 or less and put it onto the battlefield.
   * 		Then shuffle your library. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_REGENERATE = 1,
		CHOICE_TUTOR
	};

	if( event == EVENT_CAN_ACTIVATE ){
		int result = regeneration(player, card, event, MANACOST0);
		if( result && new_can_sacrifice_as_cost(player, card, &test) ){
			if( ! IS_AI(player) ){
				return 99;
			}
			else{
				CYCLE_ALL_CARDS(p, c,	{
											if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
												if( c != card && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
													return 99;
												}
											}
										}
				);
			}
		}
		return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 6, 0);
	}

	if( event == EVENT_ACTIVATE ){
		int abils[3] = {0, 0, 0};
		abils[1] = regeneration(player, card, EVENT_CAN_ACTIVATE, MANACOST0) && new_can_sacrifice_as_cost(player, card, &test) ? 1 : 0;
		if( IS_AI(player) && abils[1] ){
			abils[1] = 0;
			CYCLE_ALL_CARDS(p, c,	{
										if( p == player && in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
											if( c != card && ! check_special_flags2(player, card, SF2_CANNOT_BE_SACRIFICED) ){
												abils[1] = 1;
											}
										}
									}
			);
		}
		abils[2] = generic_subtype_searcher(player, card, EVENT_CAN_ACTIVATE, SUBTYPE_MERCENARY, 6, 0);
		int choice = CHOICE_TUTOR;
		if( abils[1] ){
			choice = CHOICE_REGENERATE;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Regenerate", 		abils[1], 1,
							"Tutor Mercenary", 	abils[2], 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_REGENERATE ){
			if( IS_AI(player) ){
				state_untargettable(player, card, 1);
			}
			generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
			if( IS_AI(player) ){
				state_untargettable(player, card, 0);
			}
		}
		if( choice == CHOICE_TUTOR ){
			return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 6, 0);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_REGENERATE && can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
		if( instance->info_slot == CHOICE_TUTOR ){
			return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 6, 0);
		}
	}

	return 0;
}

int card_cateran_persuader(int player, int card, event_t event ){
/* CARD_ID_CATERAN_PERSUADER	4412
Cateran Persuader	|B|B
Creature - Human Mercenary 2/1
|1, |T: Search your library for a Mercenary permanent card with converted mana cost 1 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 1, 1);
}

int card_cateran_slavers(int player, int card, event_t event ){
/* CARD_ID_CATERAN_SLAVER	4413
Cateran Slaver	|4|B|B
Creature - Horror Mercenary 5/5
|H2Swampwalk (This creature can't be blocked as long as defending player controls |Ha Swamp.)
|5, |T: Search your library for a Mercenary permanent card with converted mana cost 5 or less and put it onto the battlefield. Then shuffle your library. */

	return generic_subtype_searcher(player, card, event, SUBTYPE_MERCENARY, 5, 1);
}

int card_cateran_summons(int player, int card, event_t event ){
/* CARD_ID_CATERAN_SUMMONS	4414
Cateran Summons	|B
Sorcery
Search your library for a Mercenary card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (56-(hand_count[player]*8));
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_ANY, "Select a Mercenary card.");
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_cave_in(int player, int card, event_t event){
/* CARD_ID_CAVE_IN	4417
Cave-In	|3|R|R
Sorcery
You may exile a |Sred card from your hand rather than pay ~'s mana cost.
~ deals 2 damage to each creature and each player. */

	al_pitchspell(player, card, event, COLOR_RED, 0);

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! casting_al_pitchspell(player, card, event, COLOR_RED, 0) ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		new_damage_all(player, card, ANYBODY, 2, NDA_ALL_CREATURES | NDA_PLAYER_TOO, NULL);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_chameleon_spirit(int player, int card, event_t event)
{
  /* CARD_ID_CHAMELEON_SPIRIT	4421
   * Chameleon Spirit	|3|U
   * Creature - Illusion Spirit 100/100
   * As ~ enters the battlefield, choose a color.
   * ~'s power and toughness are each equal to the number of permanents of the chosen color your opponents control. */

  card_instance_t *instance;
  int result;
  test_definition_t test;
  if (card != -1)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      result = choose_a_color_and_show_legacy(player, card, 1 - player, -1);
      instance = get_card_instance(player, card);
      instance->targets[1].player = 1 << ((unsigned char)result & 0x1f);
      get_card_instance(player, card);
      return 0;
    }
    instance = get_card_instance(player, card);
    if ((((0 < instance->targets[1].player) && ((uint32_t)(event - EVENT_POWER) < 2)) &&
         (card == affected_card)) &&
        (player == affected_card_controller))
    {
      default_test_definition(&test, 0x47);
      instance = get_card_instance(player, card);
      test.color = instance->targets[1].player;
      result = check_battlefield_for_special_card(player, card, 1 - player, 4, &test);
      set_characteristic_pt(result);
    }
  }
  return 0;
}

int card_charisma(int player, int card, event_t event){
/* Charisma	|U|U|U
 * Enchantment - Aura
 * Enchant creature
 * Whenever enchanted creature deals damage to a creature, gain control of the other creature for as long as ~ remains on the battlefield. */

	 if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) && get_card_instance(player, card)->damage_target_player != -1 ){
		card_instance_t *instance = get_card_instance(player, card);
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		if( event == EVENT_DEAL_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card != -1 && damage->damage_source_player == t_player &&
					damage->damage_source_card == t_card && damage->info_slot > 0 &&
					is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) )
				{
					int pos = instance->info_slot;
					instance->targets[pos].player = damage->damage_target_player;
					instance->targets[pos].card = damage->damage_target_card;
					instance->info_slot++;
				}
			}
		}
		if( instance->info_slot != -1 && trigger_condition == TRIGGER_DEAL_DAMAGE && affect_me(player, card) &&
			reason_for_trigger_controller == player )
		{
			if(event == EVENT_TRIGGER){
				event_result |= RESOLVE_TRIGGER_MANDATORY;
			}
			else if(event == EVENT_RESOLVE_TRIGGER){
					int i;
					for(i=0;i<instance->info_slot;i++){
						gain_control_until_source_is_in_play_and_tapped(player, card, instance->targets[i].player, instance->targets[i].card, GCUS_SOURCE_IN_PLAY);
					}
					instance->info_slot = 0;
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return vanilla_aura(player, card, event, player);
}

int card_cho_arrim_alchemist(int player, int card, event_t event){
/* CARD_ID_CHO_ARRIM_ALCHEMIST	4425
Cho-Arrim Alchemist	|W
Creature - Human Spellshaper 1/1
|1|W|W, |T, Discard a card: The next time a source of your choice would deal damage to you this turn, prevent that damage.
You gain life equal to the damage prevented this way. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			gain_life(player, target->info_slot);
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DAMAGE_PREVENTION|GAA_DISCARD,
									MANACOST_XW(1, 2), 0, &td, "TARGET_DAMAGE");
}

int card_cho_arrim_bruiser(int player, int card, event_t event)
{
/* CARD_ID_CHO_ARRIM_BRUISER	4426
Cho-Arrim Bruiser	|5|W
Creature - Ogre Rebel 3/4
Whenever ~ attacks, you may tap up to two target creatures. */

	dat_ability(player, card, event, duh_mode(player) ? DAT_RESOLVE_TRIGGER_MANDATORY : DAT_RESOLVE_TRIGGER_OPTIONAL, 0);
	if (event == EVENT_DAT_ABILITY)
	{
		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_CREATURE);
		td.allow_cancel = 3;

		card_instance_t* instance = get_card_instance(player, card);

		pick_up_to_n_targets(&td, "TARGET_CREATURE", 2);
		int i;
		for (i = 0; i < instance->number_of_targets; ++i)
			tap_card(instance->targets[i].player, instance->targets[i].card);
		instance->number_of_targets = 0;
	}

  return 0;
}

int card_cho_manno_revolutionary(int player, int card, event_t event)
{
  /* CARD_ID_CHO_MANNO_REVOLUTIONARY	4429
Cho-Manno, Revolutionary	|2|W|W
Legendary Creature - Human Rebel 2/2
Prevent all damage that would be dealt to ~. */

  if (event == EVENT_CHECK_ABILITIES && affect_me(player, card) && current_phase < PHASE_AFTER_BLOCKING &&
      !is_humiliated(player, card, AL_STATIC))
    check_abilities_toughness += 0x3fff;
  check_legend_rule(player, card, event);
  prevent_all_damage(player, card, event);
  return 0;
}

int card_cinder_elemental(int player, int card, event_t event ){
	/* Cinder Elemental	|3|R	0x2009922
	 * Creature - Elemental 2/2
	 * |X|R, |T, Sacrifice ~: ~ deals X damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, instance->info_slot);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_SACRIFICE_ME|GAA_CAN_TARGET, MANACOST_XR(-1, 1), 0,
									&td, "TARGET_ANY");
}

int card_collective_unconscious(int player, int card, event_t event){
  /* Collective Unconscious |4|G|G
   * 	  Sorcery
   * 	  Draw a card for each creature you control. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1)*8);
		ai_modifier -= (hand_count[player]*8);
	}

	if(event == EVENT_RESOLVE_SPELL){
		draw_cards(player, count_subtype(player, TYPE_CREATURE, -1));
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_coastal_piracy(int player, int card, event_t event){
  /* Coastal Piracy	|2|U|U
   * Enchantment
   * Whenever a creature you control deals combat damage to an opponent, you may draw a card. */

	// original code : 00405650
	/*
	  Coastal Piracy |2|U|U
	  Enchantment
	  Whenever a creature you control deals combat damage to an opponent, you may draw a card.
	*/
	// Used externally
	if( event == EVENT_DEAL_DAMAGE ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE,
							player, card, 0, &test);
	}

	if( resolve_damage_trigger(player, card, event, RESOLVE_TRIGGER_AI(player) | DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS |
								DDBM_MUST_BE_COMBAT_DAMAGE, player, card) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int times_damaged[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		APNAP(p,{
					if( times_damaged[p] ){
						draw_some_cards_if_you_want(player, card, player, times_damaged[p]);

					}
				};
		);
	}

	return global_enchantment(player, card, event);
}

int card_common_cause(int player, int card, event_t event ){
  /* Common Cause |2|W
   * Enchantment
   * Nonartifact creatures get +2/+2 as long as they all share a color. */

	if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( !is_what(affected_card_controller, affected_card, TYPE_ARTIFACT) ){
			int color_mask = 0;
			CYCLE_ALL_CARDS(p, c, 	{
										if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) && ! is_what(p, c, TYPE_ARTIFACT) ){
											int this_color = get_color(p, c);
											if( this_color == 0 ){
												return 0;
											}
											if( color_mask == 0 ){
												color_mask = this_color;
											}
											if( !( this_color & color_mask) ){
												return 0;
											}
										}
									};
			);
			event_result+=2;
		}
	}

	return global_enchantment(player, card, event);
}

int card_crackdown(int player, int card, event_t event ){
/* CARD_ID_CRACKDOWN	4438
Crackdown	|2|W
Enchantment
Non|Swhite creatures with power 3 or greater don't untap during their controllers' untap steps. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			if( !(get_color(affected_card_controller, affected_card) & get_sleighted_color_test(player, card, COLOR_TEST_WHITE)) &&
				get_power(affected_card_controller, affected_card) > 2 )
			{
				does_not_untap(affected_card_controller, affected_card, event);
			}
		}
	}

	return global_enchantment(player, card, event);
}

int card_crooked_scales(int player, int card, event_t event)
{
/* Crooked Scales	|4
 * Artifact
 * |4, |T: Flip a coin. If you win the flip, destroy target creature an opponent controls. If you lose the flip, destroy target creature
 * 		you control unless you pay |3 and repeat this process. */

  if (!IS_ACTIVATING(event))
	return 0;

  target_definition_t td_opp;
  default_target_definition(player, card, &td_opp, TYPE_CREATURE);
  td_opp.allowed_controller = td_opp.preferred_controller = 1-player;

  target_definition_t td_own;
  default_target_definition(player, card, &td_own, TYPE_CREATURE);
  td_own.allowed_controller = td_own.preferred_controller = player;

  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CAN_ACTIVATE)
	return CAN_TAP(player, card) && CAN_ACTIVATE(player, card, MANACOST_X(4)) && can_target(&td_opp) && can_target(&td_own);

  if (event == EVENT_ACTIVATE)
	{
	  instance->number_of_targets = 0;
	  if (charge_mana_for_activated_ability(player, card, MANACOST_X(4))
		  && pick_target(&td_opp, "TARGET_CREATURE_OPPONENT_CONTROLS")
		  && new_pick_target(&td_own, "ASHNODS_BATTLEGEAR", 1, 1))	// "Select target creature you control."
		tap_card(player, card);
	  else
		instance->number_of_targets = 0;	// in case it was cancelled during second target choice
	}

  if (event == EVENT_RESOLVE_ACTIVATION)
	{
	  int valid_opp = valid_target(&td_opp), valid_own = validate_target(player, card, &td_own, 1);
	  if (!valid_opp && !valid_own)
		{
		  spell_fizzled = 1;
		  return 0;
		}

	  while (1)
		{
		  if (flip_a_coin(player, card))
			{
			  if (valid_opp)
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			  break;
			}
		  else if (has_mana(player, COLOR_ANY, 3)
				   && do_dialog(player, player, card, valid_own ? instance->targets[1].player : -1, valid_own ? instance->targets[1].card : -1,
								" Pay 3\n Destroy creature you control", 0) == 0
				   && charge_mana_while_resolving(player, card, EVENT_RESOLVE_ACTIVATION, player, COLOR_COLORLESS, 3))
			continue;
		  else
			{
			  if (valid_own)
				kill_card(instance->targets[1].player, instance->targets[1].card, KILL_DESTROY);
			  break;
			}
		}
	}

  return 0;
}

int card_crumbling_sanctuary(int player, int card, event_t event ){
/* CARD_ID_CRUMBLING_SANCTUARY	4444
Crumbling Sanctuary	|5
Artifact
If damage would be dealt to a player, that player exiles that many cards from the top of his or her library instead. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card ){
				if( damage->damage_target_card == -1 && damage->info_slot > 0 && !damage_is_to_planeswalker(damage) ){
					int amount = MIN(count_deck(damage->damage_target_player), damage->info_slot);
					int result = prevent_damage(affected_card_controller, affected_card, amount);
					int i;
					for(i=0; i<result; i++){
						rfg_top_card_of_deck(damage->damage_target_player);
					}
				}
			}
		}
	}

	return 0;
}

int card_custom_depot(int player, int card, event_t event ){
/* CARD_ID_CUSTOMS_DEPOT	4445
Customs Depot	|1|U
Enchantment
Whenever you cast a creature spell, you may pay |1. If you do, draw a card, then discard a card. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_AI(player), TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		if (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1)){
			draw_cards(player, 1);
			discard(player, 0, player);
		}
	}
	return global_enchantment(player, card, event);
}

int card_dawnstrider(int player, int card, event_t event ){
/* CARD_ID_DAWNSTRIDER	4447
Dawnstrider	|1|G
Creature - Dryad Spellshaper 1/1
|G, |T, Discard a card: Prevent all combat damage that would be dealt this turn. */

	if( event == EVENT_ACTIVATE ){
		if( current_turn == 1-player ){
			ai_modifier += (count_attackers(current_turn)*8);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance( player, card );
		fog_effect(instance->parent_controller, instance->parent_card);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_G(1), 0, NULL, NULL);
}

static void destroy_nonblack_at_end_of_combat(int player, int card, int t_player, int t_card)
{
  if (!(get_color(t_player, t_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK)))
	create_legacy_effect_exe(player, card, LEGACY_EFFECT_STONING, t_player, t_card);
}

int card_deathgazer(int player, int card, event_t event)
{
/* CARD_ID_DEATHGAZER	1010
Deathgazer	|3|B
Creature - Lizard 2/2
Whenever ~ blocks or becomes blocked by a non|Sblack creature, destroy that creature at end of combat. */

  // Used externally
  card_instance_t* instance = get_card_instance(player, card);

  if (event == EVENT_CHANGE_TYPE && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG))
	instance->destroys_if_blocked |= DIFB_ASK_CARD;

  if (event == EVENT_CHECK_DESTROY_IF_BLOCKED && affect_me(player, card) && !is_humiliated(player, card, AL_TRIG)
	  && !(get_color(attacking_card_controller, attacking_card) & get_sleighted_color_test(player, card, COLOR_TEST_BLACK)))
	event_result |= 1;

  if (event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG))
	{
	  if (player == current_turn && (instance->state & STATE_ATTACKING))
		for_each_creature_blocking_me(player, card, destroy_nonblack_at_end_of_combat, player, card);

	  if (player == 1-current_turn && (instance->state & STATE_BLOCKING))
		for_each_creature_blocked_by_me(player, card, destroy_nonblack_at_end_of_combat, player, card);
	}

  return 0;
}

int card_deepwood_drummer(int player, int card, event_t event){
/* CARD_ID_DEEPWOOD_DRUMMER	4448
Deepwood Drummer	|1|G
Creature - Human Spellshaper 1/1
|G, |T, Discard a card: Target creature gets +2/+2 until end of turn. */

	if (event == EVENT_CHECK_ABILITIES){
		if (affected_card_controller == player && card_deepwood_drummer(player, card, EVENT_CAN_ACTIVATE)){
			check_abilities_power += 2;
			check_abilities_toughness += 2;
		}
		return 0;
	}

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			pump_until_eot(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 2, 2);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD|GAA_CAN_TARGET, MANACOST_G(1), 0, &td, "TARGET_CREATURE");
}

int card_deepwood_ghoul(int player, int card, event_t event){
  /* Deepwood Ghoul |2|B
   * 	  Creature - Zombie 2/1
   * 	  Pay 2 life: Regenerate Deepwood Ghoul. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);
		if( can_regenerate(instance->parent_controller, instance->parent_card) ){
			regenerate_target(instance->parent_controller, instance->parent_card);
		}
	}
	return generic_activated_ability(player, card, event, GAA_REGENERATION, MANACOST0, 2, NULL, NULL);
}

int card_dehydration(int player, int card, event_t event)
{
/* CARD_ID_DEHYDRATION	1014
Dehydration	|3|U
Enchantment - Aura
Enchant creature (Target a creature as you cast this. This card enters the battlefield attached to that creature.)
Enchanted creature doesn't untap during its controller's untap step. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) && get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t* instance = get_card_instance(player, card);
		does_not_untap(instance->damage_target_player, instance->damage_target_card, event);
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){
		return 0;
	}

	return disabling_aura(player, card, event);
}

int card_delraich(int player, int card, event_t event){
  /* Delraich |6|B
   * 	  Creature - Horror 6/6
   * 	  Trample
   * 	  You may sacrifice three black creatures rather than pay Delraich's mana cost. */

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
		test.qty = 3;

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
				choice = do_dialog(player, player, card, -1, -1, " Sac 3 black creatures\n Pay mana cost\n Cancel", 1);
			}

			if( choice == 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_CREATURE, "Select a black creature to sacrifice.");
				test.color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);
				int ts = 0;
				int sacced[3];
				while( ts < 3 ){
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
					if( ts == 3 ){
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

int card_devout_witness(int player, int card, event_t event ){
/* CARD_ID_DEVOUT_WITNESS	4454
Devout Witness	|2|W
Creature - Human Spellshaper 2/2
|1|W, |T, Discard a card: Destroy target artifact or enchantment. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ENCHANTMENT | TYPE_ARTIFACT);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD|GAA_CAN_TARGET, MANACOST_XW(1, 1), 0, &td, "DISENCHANT");
}

int card_diplomatic_immunity(int player, int card, event_t event ){
  /* Diplomatic Immunity |1|U
   * 	  Enchantment - Aura
   * 	  Enchant creature
   * 	  Shroud (A permanent with shroud can't be the target of spells or abilities.)
   * 	  Enchanted creature has shroud. */

	return generic_aura(player, card, event, player, 0, 0, KEYWORD_SHROUD, 0, 0, 0, 0);
}

int card_distorting_lens(int player, int card, event_t event){
  /* Distorting Lens	|2
   * Artifact
   * |T: Target permanent becomes the color of your choice until end of turn. */

	/*
	  Distorting Lens English
	  Artifact, 2
	  {T}: Target permanent becomes the color of your choice until end of turn.
	*/
	if (!IS_GAA_EVENT(event)  ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);

	if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td) ){
		card_instance_t *instance = get_card_instance(player, card);
		change_color(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card,
					1<<(choose_a_color(player, get_deck_color(player, 1-player))), 2 | 4);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_PERMANENT");
}

int card_dust_bowl(int player, int card, event_t event){
/* Dust Bowl	""
 * Land
 * |T: Add |1 to your mana pool.
 * |3, |T, Sacrifice a land: Destroy target nonbasic land. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	card_instance_t* instance = get_card_instance(player, card);

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.required_subtype = SUBTYPE_NONBASIC;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");

	enum{
		CHOICE_MANA = 1,
		CHOICE_KILL_LAND,
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() ){
			if( generic_activated_ability_with_sacrifice_as_cost(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED,
																MANACOST_X(4), 0, &td, NULL, 0, &test, 0) )
			{
				choice = CHOICE_KILL_LAND;
			}
		}
		instance->info_slot = choice;
		if( choice == CHOICE_KILL_LAND ){
			if( count_subtype(player, TYPE_LAND, -1) >= 6 || count_subtype(1-player, TYPE_LAND, -1) <= 2 ){
				ai_modifier += 48;
			}
		}
		if( ! IS_AI(player) && choice != CHOICE_MANA ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 1,
							"Destroy a nonbasic land ", 1, 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}
		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_KILL_LAND ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability_with_sacrifice_as_cost(player, card, event, GAA_CAN_TARGET | GAA_LITERAL_PROMPT,
															MANACOST_X(3), 0, &td, "Select target nonbasic land.", 0, &test, 0);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_KILL_LAND ){
			if( valid_target(&td) ){
				kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
			}
		}
	}

	return 0;
}

int card_embargo(int player, int card, event_t event ){
/* CARD_ID_EMBARGO	4458
Embargo	|3|U
Enchantment
Nonland permanents don't untap during their controllers' untap steps.
At the beginning of your upkeep, you lose 2 life. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( current_phase == PHASE_UNTAP && event == EVENT_UNTAP && ! is_what(affected_card_controller, affected_card, TYPE_LAND) ){
			does_not_untap(affected_card_controller, affected_card, event);
		}
	}

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		lose_life(player, 2);
	}

	return global_enchantment(player, card, event);
}

int card_enslaved_horror(int player, int card, event_t event ){
/* CARD_ID_ENSLAVED_HORROR	4459
Enslaved Horror	|3|B
Creature - Horror 4/4
When ~ enters the battlefield, each other player may return a creature card from his or her graveyard to the battlefield. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int count = 0;
		int max_cmc = 0;
		while( get_grave(1-player)[count] != -1 ){
				int iid = get_grave(1-player)[count];
				if( is_what(-1, iid, TYPE_CREATURE) ){
					int this_cmc = get_cmc_by_internal_id(iid);
					if( this_cmc > max_cmc ){
						max_cmc = this_cmc;
					}
				}
				count++;
		}
		ai_modifier += (max_cmc > 3 ? -128 : 24);
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "Select a creature card.");
		if( count_graveyard_by_type(1-player, TYPE_CREATURE) ){
			new_global_tutor(1-player, 1-player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
		}
	}

	return 0;
}

int card_eye_of_ramos(int player, int card, event_t event ){
/* CARD_ID_EYE_OF_RAMOS	4463
Eye of Ramos	|3
Artifact
|T: Add |U to your mana pool.
Sacrifice ~: Add |U to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_BLUE);
}

int card_forced_march(int player, int card, event_t event){
/* CARD_ID_FORCED_MARCH	4470
Forced March	|X|B|B|B
Sorcery
Destroy all creatures with converted mana cost X or less. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance( player, card );

		test_definition_t test;
		default_test_definition(&test, TYPE_CREATURE);
		test.cmc = instance->info_slot+1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

static int foster_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_FOSTER)
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
        }
        count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount && has_mana(player, COLOR_ANY, 1))
    {
      int choice = DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                          "Reveal cards", 1, count_deck(player) >= 20 ? 10 : 1, "Decline", 1, 5);
      if (choice == 1)
      {
        if (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 1))
        {
          test_definition_t this_test;
          default_test_definition(&this_test, TYPE_CREATURE);
          reveal_card_until_you_reveal_test(player, player, &this_test,
                                            RCUYRT_ADD_TEST_TO_HAND | RCUYRT_MILL_REST);
        }
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

int card_foster(int player, int card, event_t event)
{
  /* Foster |2|G|G
   *   Enchantment
   *   Whenever a creature you control dies, you may pay {1}. If you do, reveal cards from the top of
   *   your library until you reveal a creature card. Put that card into your hand and the rest into your
   *   graveyard. */

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    int c;
    for (c = 0; c < active_cards_count[player]; c++)
    {
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(player, c);
        if (inst->info_slot == (intptr_t)foster_effect)
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
      int legacy = create_targetted_legacy_effect(player, card, &foster_effect, player, card);
      card_instance_t *inst = get_card_instance(player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability(player, card, event, player, TYPE_CREATURE, NULL);
  }

  if (get_card_instance(player, card)->kill_code <= 0)
  {
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_AI(player));
    if (result)
    {
      int amount = BYTE0(result);
      while (amount && has_mana(player, COLOR_ANY, 1))
      {
        int choice =
            DIALOG(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
                   "Reveal cards", 1, count_deck(player) >= 20 ? 10 : 1, "Decline", 1, 5);
        if (choice == 1)
        {
          if (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY,
                                          1))
          {
            test_definition_t this_test;
            default_test_definition(&this_test, TYPE_CREATURE);
            reveal_card_until_you_reveal_test(player, player, &this_test,
                                              RCUYRT_ADD_TEST_TO_HAND | RCUYRT_MILL_REST);
          }
        }
        else
        {
          break;
        }
        amount--;
      }
    }
  }

  return global_enchantment(player, card, event);
}

int card_furious_assault(int player, int card, event_t event){
/* CARD_ID_FURIOUS_ASSAULT	4475
Furious Assault	|2|R
Enchantment
Whenever you cast a creature spell, ~ deals 1 damage to target player. */

	sp_ability_trigger(player, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, NULL, player, card);
	if (event == EVENT_SPA){
		target_definition_t td;
		default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
		td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
		td.allow_cancel = 0;
		card_instance_t* instance = get_card_instance(player, card);
		if (can_target(&td) && new_pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER", 0, 0)){
			damage_player(instance->targets[0].player, 1, player, card);
		}
		instance->number_of_targets = 0;
	}
	return global_enchantment(player, card, event);
}

int card_game_preserve(int player, int card, event_t event){
/* CARD_ID_GAME_PRESERVE	4476
Game Preserve	|2|G
Enchantment
At the beginning of your upkeep, each player reveals the top card of his or her library.
If all cards revealed this way are creature cards, put those cards onto the battlefield under their owners' control. */

	upkeep_trigger_ability(player, card, event, player);

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		int id1 = deck_ptr[player][0];
		if( id1 != -1 ){
			reveal_card_iid(player, card, id1);
		}
		int id2 = deck_ptr[1-player][0];
		if( id1 != -1 ){
			reveal_card_iid(player, card, id2);
		}
		if( is_what(-1, id1, TYPE_CREATURE) && is_what(-1, id2, TYPE_CREATURE) ){
			put_into_play_a_card_from_deck(player, player, 0);
			put_into_play_a_card_from_deck(1-player, 1-player, 0);
		}
	}

	return global_enchantment(player, card, event);
}

int card_generals_regalia(int player, int card, event_t event){
  /* General's Regalia |3
   * Artifact
   * {3}: The next time a source of your choice would deal damage to you this turn, that damage is dealt to target creature you control instead. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ANY);
	td.extra = damage_card;
	td.special = TARGET_SPECIAL_DAMAGE_PLAYER;

	target_definition_t td2;
	default_target_definition(player, card, &td2, TYPE_CREATURE);
	td2.allowed_controller = player;
	td2.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( can_target(&td) ){
			return generic_activated_ability(player, card, event, GAA_DAMAGE_PREVENTION | GAA_CAN_TARGET, MANACOST_X(3), 0, &td, NULL);
		}
	}
	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(3)) ){
			if( pick_target(&td, "TARGET_DAMAGE") ){
				new_pick_target(&td2, "Select target creature you control.", 1, 1 | GS_LITERAL_PROMPT);
				if( spell_fizzled != 1 ){
					int mod = -24;
					if( check_for_special_ability(instance->targets[1].player, instance->targets[1].card, SP_KEYWORD_INDESTRUCTIBLE) ){
						mod = 128;
					}
					if( get_id(instance->targets[1].player, instance->targets[1].card) == CARD_ID_CHO_MANNO_REVOLUTIONARY ){
						mod = 128;
					}
					int actual_toughness = get_toughness(instance->targets[1].player, instance->targets[1].card);
					actual_toughness -= get_card_instance(instance->targets[1].player, instance->targets[1].card)->damage_on_card;
					if( actual_toughness >  get_card_instance(instance->targets[0].player, instance->targets[0].card)->info_slot ){
						ai_modifier = 48;
					}
					ai_modifier += mod;
				}
			}
		}
	}
	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && validate_target(player, card, &td2, 1) ){
			card_instance_t *dmg = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			dmg->damage_target_card = instance->targets[1].card;
			if( get_id(instance->targets[1].player, instance->targets[1].card) == CARD_ID_CHO_MANNO_REVOLUTIONARY ){
				dmg->info_slot = 0;
			}
		}
	}
	return 0;
}

int card_gush(int player, int card, event_t event){
/* CARD_ID_GUSH	4482
Gush	|4|U
Instant
You may return two |H1Islands you control to their owner's hand rather than pay ~'s mana cost.
Draw two cards. */
	// Used externally

	if( event == EVENT_MODIFY_COST ){
		card_instance_t* instance = get_card_instance(player, card);

		target_definition_t td;
		default_target_definition(player, card, &td, TYPE_LAND);
		td.required_subtype = SUBTYPE_ISLAND;
		td.allowed_controller = player;
		td.preferred_controller = player;
		td.illegal_abilities = 0;
		if( target_available(player, card, &td) > 1 ){
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

	if(event == EVENT_CAST_SPELL  && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			target_definition_t td;
			default_target_definition(player, card, &td, TYPE_LAND);
			td.required_subtype = SUBTYPE_ISLAND;
			td.allowed_controller = player;
			td.preferred_controller = player;
			td.illegal_abilities = 0;

			int choice = 0;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Return 2 Islands\n Play normally\n Cancel", 0);
			}

			if( choice == 0 ){
				int trgs = 0;
				while( trgs < 2 ){
						if( new_pick_target(&td, "Select an Island to bounce.", trgs, GS_LITERAL_PROMPT) ){
							state_untargettable(instance->targets[trgs].player, instance->targets[trgs].card, 1);
							trgs++;
						}
						else{
							break;
						}
				}
				int i;
				for(i=0; i<trgs; i++){
					if( trgs == 2 ){
						bounce_permanent(instance->targets[i].player, instance->targets[i].card);
					}
					else{
						state_untargettable(instance->targets[i].player, instance->targets[i].card, 0);
					}
				}
				instance->number_of_targets = 0;
				if( trgs < 2 ){
					spell_fizzled = 1;
					return 0;
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

	if( event == EVENT_RESOLVE_SPELL ){
		draw_cards(player, 2);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_hammer_mage(int player, int card, event_t event){
/* CARD_ID_HAMMER_MAGE	4483
Hammer Mage	|1|R
Creature - Human Spellshaper 1/1
|X|R, |T, Discard a card: Destroy all artifacts with converted mana cost X or less. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		default_test_definition(&test, TYPE_ARTIFACT);
		test.cmc = instance->info_slot + 1;
		test.cmc_flag = F5_CMC_LESSER_THAN_VALUE;
		new_manipulate_all(instance->parent_controller, instance->parent_card, ANYBODY, &test, KILL_DESTROY);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_XR(-1, 1), 0, NULL, NULL);
}

int card_haunted_crossroads(int player, int card, event_t event){
/* CARD_ID_HAUNTED_CROSSROADS	4484
Haunted Crossroads	|2|B
Enchantment
|B: Put target creature card from your graveyard on top of your library. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");

	if( event == EVENT_ACTIVATE ){
		ai_modifier += (count_subtype(player, TYPE_CREATURE, -1) < 3 ? 48 : -48);
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_graveyard_to_deck(player, selected, 1);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, 0, MANACOST_B(1), 0, NULL, NULL,
															0, &this_test, GS_GRAVE_RECYCLER);
}

int card_heart_of_ramos(int player, int card, event_t event ){
/* CARD_ID_HEART_OF_RAMOS	4485
Heart of Ramos	|3
Artifact
|T: Add |R to your mana pool.
Sacrifice ~: Add |R to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_RED);
}

int card_hickory_woodlot(int player, int card, event_t event ){
	/* Hickory Woodlot	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |G|G to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_GREEN);
}

int card_high_market(int player, int card, event_t event){

	/* High Market	""
	 * Land
	 * |T: Add |1 to your mana pool.
	 * |T, Sacrifice a creature: You gain 1 life. */

	if( event == EVENT_RESOLVE_SPELL ){
		play_land_sound_effect(player, card);
	}

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");

	card_instance_t *instance = get_card_instance(player, card);

	enum{
		CHOICE_MANA = 1,
		CHOICE_SAC_AND_GAIN
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED | GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL) )
		{
			choice = CHOICE_SAC_AND_GAIN;
		}
		instance->info_slot = choice;
		if( ! IS_AI(player) && choice == CHOICE_SAC_AND_GAIN ){
			choice = DIALOG(player, card, event, DLG_RANDOM,
							"Get mana", 1, 5,
							"Sac & gain 1 life", 1, life[player] < 6 ? 10 : 1);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		instance->info_slot = choice;

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_SAC_AND_GAIN ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_SACRIFICE_CREATURE, MANACOST0, 0, NULL, NULL);
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SAC_AND_GAIN ){
			gain_life(player, 1);
		}
	}

	return 0;
}

int card_hired_giant(int player, int card, event_t event ){
/* CARD_ID_HIRED_GIANT	4492
Hired Giant	|3|R
Creature - Giant 4/4
When ~ enters the battlefield, each other player may search his or her library for a land card and put that card onto the battlefield.
Then each player who searched his or her library this way shuffles it. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		if( do_dialog(1-player, player, card, -1, -1, " Tutor a land\n Pass", (1-player == AI ? 0 : 1)) == 0 ){
			test_definition_t this_test;
			new_default_test_definition(&this_test, TYPE_LAND, "Select a land card.");
			new_global_tutor(1-player, 1-player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, 1, &this_test);
		}
	}

	return 0;
}

int card_honor_the_fallen(int player, int card, event_t event){
/* CARD_ID_HONOR_THE_FALLEN	4493
Honor the Fallen	|1|W
Instant
Exile all creature cards from all graveyards. You gain 1 life for each card exiled this way. */

	if( event == EVENT_RESOLVE_SPELL ){
		int i;
		int amount = 0;
		for(i=0; i<2; i++){
			const int *grave = get_grave(i);
			int count = count_graveyard(i)-1;
			while( count > -1 ){
					if( is_what(-1, grave[count], TYPE_CREATURE) ){
						rfg_card_from_grave(i, count);
						amount++;
					}
					count--;
			}
		}
		gain_life(player, amount);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

static int horn_of_plenty_legacy(int player, int card, event_t event){

	if( trigger_condition == TRIGGER_EOT && affect_me(player, card) && reason_for_trigger_controller == player ){
		if( trigger_cause_controller == player && trigger_cause == card ){
			card_instance_t *instance = get_card_instance( player, card );
			if( instance->targets[0].card > 0 ){
				instance->targets[0].card--;
			}
			else{
				if( eot_trigger(player, card, event) ){
					draw_cards(instance->targets[0].player, 1);
					kill_card(player, card, KILL_EXILE);
				}
			}
		}
	}

	return 0;
}

int card_horn_of_plenty(int player, int card, event_t event){
  /* Horn of Plenty |6
   * Artifact
   * Whenever a player casts a spell, he or she may pay {1}. If that player does, he or she draws a card at the beginning of the next end step. */

	sp_ability_trigger(ANYBODY, event, RESOLVE_TRIGGER_AI(player), TYPE_ANY, NULL, player, card);
	if (event == EVENT_SPA){
		int choice = 2;
		if( has_mana( trigger_cause_controller, COLOR_ANY, 1) ){
			choice = DIALOG(trigger_cause_controller, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_NO_STORAGE, DLG_NO_CANCEL,
							"Activate Horn of Plenty", 1, 14-(hand_count[player]*2),
							"Decline", 1, 5);
		}
		if( choice == 1 && charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, trigger_cause_controller, COLOR_ANY, 1) ){
			int legacy = create_legacy_effect(player, card, &horn_of_plenty_legacy);
			card_instance_t *instance = get_card_instance( player, legacy );
			instance->targets[0].player = trigger_cause_controller;
			if( current_phase == PHASE_DISCARD ){
				instance->targets[0].card = 1;
			}
		}
	}
	return 0;
}

int card_horn_of_ramos(int player, int card, event_t event ){
/* CARD_ID_HORN_OF_RAMOS	4496
Horn of Ramos	|3
Artifact
|T: Add |G to your mana pool.
Sacrifice ~: Add |G to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_GREEN);
}

int card_howling_wolf(int player, int card, event_t event ){
/* Howling Wolf	|2|G|G
Creature - Wolf 2/2
When ~ enters the battlefield, you may search your library for up to three cards named ~, reveal them, and put them into your hand.
If you do, shuffle your library.*/

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		char buffer[100];
		card_ptr_t* c = cards_ptr[ get_id(player, card) ];
		scnprintf(buffer, 100, "Select a card named %s.", c->name);;

		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, buffer);
		test.id = get_id(player, card);
		test.qty = MIN(count_deck(player), 3);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_FIRST_FOUND, &test);
	}

	return 0;
}

int card_hunted_wumpus(int player, int card, event_t event ){
  /* Hunted Wumpus |3|G
   * 	  Creature - Beast 6/6
   * 	  When Hunted Wumpus enters the battlefield, each other player may put a creature card from his or her hand onto the battlefield. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card to put into play.");
		this_test.zone = TARGET_ZONE_HAND;
		if( check_battlefield_for_special_card(player, card, 1-player, 0, &this_test) ){
			new_global_tutor(1-player, 1-player, TUTOR_FROM_HAND, TUTOR_PLAY, 0, AI_MAX_CMC, &this_test);
		}
	}

	return 0;
}

int card_intimidation(int player, int card, event_t event)
{
/* CARD_ID_INTIMIDATION	4502
Intimidation	|2|B|B|B
Enchantment
Creatures you control have fear. (They can't be blocked except by artifact creatures and/or black creatures.) */
	// Used externally
	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if (event == EVENT_ABILITIES && affected_card_controller == player &&
			is_what(affected_card_controller, affected_card, TYPE_CREATURE) )
		{
			fear(affected_card_controller, affected_card, event);
		}
	}

	return global_enchantment(player, card, event);
}

int card_invigorate(int player, int card, event_t event){
  /* Invigorate |2|G
   * 	  Instant
   * 	  If you control a Forest, rather than pay Invigorate's mana cost, you may have an opponent gain 3 life.
   * 	  Target creature gets +4/+4 until end of turn. */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance( player, card );
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST) && player_can_gain_life_as_cost(1-player) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CHECK_PUMP ){
		if( has_mana_to_cast_iid(player, EVENT_CAN_CAST, get_card_instance(player, card)->internal_card_id) ||
			check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_FOREST)
		  ){
			pumpable_power[player] += 4;
			pumpable_toughness[player] += 4;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Make opponent gain life\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				gain_life(1-player, 3);
				td.allow_cancel = 0;
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( spell_fizzled != 1 ){
			pick_target(&td, "TARGET_CREATURE");
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 4, 4);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_ivory_mask(int player, int card, event_t event){
/* CARD_ID_IVORY_MASK	1064
Ivory Mask	|2|W|W
Enchantment
You have shroud. (You can't be the target of spells or abilities.) */

	if (in_play(player, card) && !is_humiliated(player, card, AL_STATIC)){
		give_shroud_to_player(player, card, event);
	}
	return global_enchantment(player, card, event);
}

int card_karns_touch(int player, int card, event_t event){
/* CARD_ID_KARNS_TOUCH	4509
Karn's Touch	|U|U
Instant
Target noncreature artifact becomes an artifact creature with power and toughness each equal to its converted mana cost until end of turn.
(It retains its abilities.) */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.illegal_type = TYPE_CREATURE;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			artifact_animation(player, card, instance->targets[0].player, instance->targets[0].card, 1, -1, -1, 0, 0);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, "Select targer noncreature artifact", 1, NULL);
}

int card_kris_mage(int player, int card, event_t event){
	/* Kris Mage	|R	0x2004f94
	 * Creature - Human Spellshaper 1/1
	 * |R, |T, Discard a card: ~ deals 1 damage to any target. */
	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE|TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			damage_target0(player, card, 1);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_R(1), 0,
									&td, "TARGET_ANY");
}

int card_kyren_negotiations(int player, int card, event_t event){
	/* Kyren Negotiations	|2|R|R	0x2004f99
	 * Enchantment
	 * Tap an untapped creature you control: ~ deals 1 damage to target player or planeswalker. */
	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TARGET_TYPE_PLANESWALKER);
	td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.allowed_controller = player;
	td1.preferred_controller = player;
	td1.illegal_abilities = 0;
	td1.illegal_state = TARGET_STATE_TAPPED;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && can_target(&td1) ){
		return generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST0, 0, &td, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		instance->number_of_targets = 0;
		if( charge_mana_for_activated_ability(player, card, MANACOST0) ){
			if( new_pick_target(&td1, "Select an untapped creature you control.", 0, 1 | GS_LITERAL_PROMPT) ){
				instance->number_of_targets = 0;
				int to_tap = instance->targets[0].card;
				if( pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER") ){
					tap_card(player, to_tap);
				}
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

int card_kyren_toy(int player, int card, event_t event){
/* CARD_ID_KYREN_TOY	4516
Kyren Toy	|3
Artifact
|1, |T: Put a charge counter on ~.
|T, Remove X charge counters from ~: Add an amount of |C to your mana pool equal to X plus one. */

	return mana_battery_generic(player, card, event, COLOR_COLORLESS, 1);
}

int card_land_grant(int player, int card, event_t event){
/* CARD_ID_LAND_GRANT	4517
Land Grant	|1|G
Sorcery
If you have no land cards in hand, you may reveal your hand rather than pay ~'s mana cost.
Search your library for |Ha Forest card, reveal that card, and put it into your hand. Then shuffle your library. */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t* instance = get_card_instance(player, card);
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_LAND);
		this_test.zone = TARGET_ZONE_HAND;
		if( ! check_battlefield_for_special_card(player, card, player, 0, &this_test) ){
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

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t* instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, event, instance->internal_card_id) ){
				choice = do_dialog(player, player, card, -1, -1, " Reveal your hand\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				reveal_target_player_hand(player);
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_LAND, get_hacked_land_text(player, card, "Select %a card.", SUBTYPE_FOREST));
		this_test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &this_test);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_larceny(int player, int card, event_t event){
  /* Larceny |3|B|B
   * Enchantment
   * Whenever a creature you control deals combat damage to a player, that player discards a card. */

	test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	check_damage_test(player, -1, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE,
						player, card, 0, &test);

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_DAMAGE_PLAYER | DDBM_TRACE_DAMAGED_PLAYERS | DDBM_MUST_BE_COMBAT_DAMAGE,
								player, card) )
	{
		card_instance_t* instance = get_card_instance(player, card);
		int num[2] = { BYTE0(instance->targets[1].player), BYTE1(instance->targets[1].player) };
		instance->targets[1].player = 0;
		new_multidiscard(0, num[0], 0, player);
		new_multidiscard(1, num[1], 0, player);
	}

	return global_enchantment(player, card, event);
}

static int liability_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) != CARD_ID_LIABILITY)
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
        }
        count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_PERMANENT, NULL,
                                               GFPC_TRACK_OWNER, 0);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    APNAP(p, {
      int amount = p == 0 ? BYTE0(result) : BYTE1(result);
      while (amount > 0)
      {
        lose_life(p, 1);
        --amount;
      }
    };);
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_liability(int player, int card, event_t event)
{
  /* Liability |1|B|B
   *   Enchantment
   *   Whenever a nontoken permanent is put into a player's graveyard from the battlefield, that player
   *   loses 1 life. */

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    int c;
    for (c = 0; c < active_cards_count[player]; c++)
    {
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(player, c);
        if (inst->info_slot == (intptr_t)liability_effect)
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
      int legacy = create_targetted_legacy_effect(player, card, &liability_effect, player, card);
      card_instance_t *inst = get_card_instance(player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    if (!is_token(affected_card_controller, affected_card))
    {
      count_for_gfp_ability_and_store_values(player, card, event, ANYBODY, TYPE_PERMANENT, NULL,
                                             GFPC_TRACK_OWNER, 0);
    }
  }

  if (get_card_instance(player, card)->kill_code <= 0)
  {
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result)
    {
      APNAP(p, {
        int amount = p == 0 ? BYTE0(result) : BYTE1(result);
        while (amount > 0)
        {
          lose_life(p, 1);
          --amount;
        }
      };);
    }
  }

  return global_enchantment(player, card, event);
}

int card_lumbering_satyr(int player, int card, event_t event ){
/* CARD_ID_LUMBERING_SATYR	4524
Lumbering Satyr	|2|G|G
Creature - Satyr Beast 5/4
All creatures have |H2forestwalk. (They can't be blocked as long as defending player controls |Ha Forest.) */

	return boost_creature_type(player, card, event, -1, 0, 0, get_hacked_walk(player, card, KEYWORD_FORESTWALK), BCT_INCLUDE_SELF);
}

int card_lunge(int player, int card, event_t event){
	/* Lunge	|2|R	0x2004fb2
	 * Instant
	 * ~ deals 2 damage to target creature and 2 damage to target player or planeswalker. */
	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	target_definition_t td1;
	default_target_definition(player, card, &td1, TARGET_TYPE_PLANESWALKER);
	td1.zone = TARGET_ZONE_CREATURE_OR_PLAYER;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST && can_target(&td1) ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_CREATURE", 1, NULL);
		instance->targets[1].player = instance->targets[0].player;
		instance->targets[1].card = instance->targets[0].card;
		if( spell_fizzled != 1 ){
			pick_target(&td1, "TARGET_PLAYER_OR_PLANESWALKER");
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 1) ){
			damage_creature(instance->targets[1].player, instance->targets[1].card, 2, player, card);
		}
		if( validate_target(player, card, &td1, 0) ){
			damage_target0(player, card, 2);
		}

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_magistrates_scepter(int player, int card, event_t event){
/* Magistrate's Scepter	|3
 * Artifact
 * |4, |T: Put a charge counter on ~.
 * |T, Remove three charge counters from ~: Take an extra turn after this one. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	enum{
		CHOICE_COUNTER = 1,
		CHOICE_TIME_WALK
	};

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_ACTIVATE ){
		if( generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL) ){
			return 1;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3), NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		if( count_counters(player, card, COUNTER_CHARGE) < 3 ){
			ai_modifier += (current_turn == 1-player ? 48 : 24);
		}
		else{
			ai_modifier += (current_turn == player ? 128 : -128);
		}
		int abils[3] = {
						0,
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL),
						generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3),
												NULL, NULL)
		};
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Add a counter", 	abils[1], 5,
							"Time Walk", 		abils[2], 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_COUNTER ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST_X(4), 0, NULL, NULL);
		}
		if( choice == CHOICE_TIME_WALK ){
			return generic_activated_ability(player, card, event, GAA_UNTAPPED, MANACOST0, GVC_COUNTERS(COUNTER_CHARGE, 3), NULL, NULL);
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_COUNTER ){
			add_counters(player, card, player, card, COUNTER_CHARGE, 1);
		}
		if( instance->info_slot == CHOICE_TIME_WALK ){
			time_walk_effect(instance->parent_controller, instance->parent_card);
		}
	}

	return 0;
}

int card_megatherium(int player, int card, event_t event ){
/* CARD_ID_MEGATHERIUM	4529
Megatherium	|2|G
Creature - Beast 4/4
Trample
When ~ enters the battlefield, sacrifice it unless you pay |1 for each card in your hand. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! has_mana(player, COLOR_ANY, hand_count[player]) ){
			ai_modifier-=1000;
		}
	}

	etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);

	if( event == EVENT_ETB_ABILITY ){
		int kill_me = 1;
		if( has_mana(player, COLOR_COLORLESS, hand_count[player]) ){
			charge_mana(player, COLOR_COLORLESS, hand_count[player]);
			if( spell_fizzled != 1 ){
				kill_me = 0;
			}
		}
		if( kill_me == 1 ){
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

int card_mercadian_atlas(int player, int card, event_t event ){
/* CARD_ID_MERCADIAN_ATLAS	4531
Mercadian Atlas	|5
Artifact
At the beginning of your end step, if you didn't play a land this turn, you may draw a card. */

	if( current_turn == player && !(land_can_be_played & LCBP_LAND_HAS_BEEN_PLAYED) && ! is_humiliated(player, card, AL_TRIG) &&
		eot_trigger_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player)) )
	{
		draw_cards(player, 1);
	}

	return 0;
}

int card_mercadian_lift(int player, int card, event_t event)
{
  /* Mercadian Lift	|2
   * Artifact
   * |1, |T: Put a winch counter on ~.
   * |T, Remove X winch counters from ~: You may put a creature card with converted mana cost X from your hand onto the battlefield. */

  enum
  {
    CHOICE_COUNTER = 1,
    CHOICE_PUT_CREATURE_ON_BF = 2
  };

  int choice;
  card_instance_t *instance;
  int counters_or_mode;
  int add_available_or_index;
  int summon_available_or_cmc;
  card_instance_t *hand_card;
  int hand_value;
  uint32_t summon_priority;
  int best_value;
  int chosen_counters;
  char msg[100];
  test_definition_t test;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) != 0)
      {
        return 1;
      }
      if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0, 0, 0, 0, 0, 0, 0, NULL,
                                    NULL) == 0)
      {
        return 0;
      }
      return 2;
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->targets[0].player = 0;
      counters_or_mode = count_counters(player, card, COUNTER_WINCH);
      default_test_definition(&test, 2);
      test.zone = 0x100;
      test.cmc = counters_or_mode;
      if (check_battlefield_for_special_card(player, card, player, 0, &test) == 0)
      {
        ai_modifier = ai_modifier + (unsigned int)((current_turn != player) ? 0 : -24) + 0x30;
        add_available_or_index = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1,
                                                           0, 0, 0, 0, 0, 0, NULL, NULL);
        summon_available_or_cmc = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0,
                                                            0, 0, 0, 0, 0, 0, NULL, NULL);
        summon_priority = 1;
      }
      else
      {
        if (current_turn == player)
        {
          add_available_or_index = counters_or_mode << 3;
        }
        else
        {
          add_available_or_index = counters_or_mode * 0xc;
        }
        ai_modifier = ai_modifier + add_available_or_index;
        add_available_or_index = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 1,
                                                           0, 0, 0, 0, 0, 0, NULL, NULL);
        summon_available_or_cmc = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 1, 0,
                                                            0, 0, 0, 0, 0, 0, NULL, NULL);
        summon_priority = 10;
      }
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "Add a counter",
                           add_available_or_index, 5, "Put a creature on BF",
                           summon_available_or_cmc, summon_priority, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (choice == CHOICE_COUNTER)
      {
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 1, 1, 0, 0, 0, 0, 0, 0, NULL,
                                         NULL);
      }
      if (choice != CHOICE_PUT_CREATURE_ON_BF)
      {
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, 0, 0, 0, 0, 0, 0) == 0)
      {
        return 0;
      }
      if (active_cards_count[player] < 1)
      {
        chosen_counters = 0;
      }
      else
      {
        add_available_or_index = 0;
        chosen_counters = 0;
        best_value = 0;
        do
        {
          hand_card = in_hand(player, add_available_or_index);

          if (((hand_card != NULL) &&
               (summon_available_or_cmc = get_cmc(player, add_available_or_index),
                summon_available_or_cmc <= counters_or_mode)) &&
              (hand_value = get_base_value(player, add_available_or_index),
               best_value < hand_value))
          {
            best_value = hand_value;
            chosen_counters = summon_available_or_cmc;
          }
          add_available_or_index = add_available_or_index + 1;
        } while (add_available_or_index < active_cards_count[player]);
      }
      if ((ai_is_speculating != 1) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
      {
        chosen_counters =
            choose_a_number(player, "How many counters you\'ll remove?", chosen_counters);
        if (chosen_counters < 0)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (counters_or_mode < chosen_counters)
        {
          spell_fizzled = 1;
          return 0;
        }
      }
      remove_counters(player, card, player, card, COUNTER_WINCH, chosen_counters);
      add_state(player, card, 0x10);
      tapped_for_mana_color = -1;
      instance->targets[0].player = chosen_counters;
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
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    counters_or_mode = instance->info_slot;
    if (counters_or_mode == CHOICE_COUNTER)
    {
      add_counters(player, card, player, card, COUNTER_CHARGE, 1);
      counters_or_mode = instance->info_slot;
    }
    if (counters_or_mode == CHOICE_PUT_CREATURE_ON_BF)
    {
      scnprintf(msg, 100, "Select creature card with CMC %d.", instance->targets[0].player);
      new_default_test_definition(&test, 2, msg);
      test.cmc = instance->targets[0].player;
      new_global_tutor(player, player, 4, 4, 0, 1, &test);
    }
  }
  return 0;
}

int card_midnight_ritual(int player, int card, event_t event){
/* CARD_ID_MIDNIGHT_RITUAL	4534
Midnight Ritual	|X|2|B
Sorcery
Exile X target creature cards from your graveyard. For each creature card exiled this way, create a 2/2 |Sblack Zombie creature token. */

	if (event == EVENT_CAN_CAST){
		return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
	}

	if (event == EVENT_CAST_SPELL && affect_me(player, card)){
		card_instance_t *instance = get_card_instance(player, card);
		int max_cards = count_graveyard_by_type(player, TYPE_CREATURE);
		int generic_mana_available = has_mana(player, COLOR_ANY, 1);	// A useful trick from card_fireball().
		max_cards = MIN(max_cards, generic_mana_available);
		max_cards = MIN(max_cards, 19);

		if (max_cards <= 0 || graveyard_has_shroud(player)){
			ai_modifier -= 48;
			instance->info_slot = 0;
			return 0;
		}

		char buf[100];
		if (ai_is_speculating == 1){
			*buf = 0;
		} else if (max_cards == 1){
			strcpy(buf, "Select a creature card.");
		} else {
			sprintf(buf, "Select up to %d creature cards.", max_cards);
		}
		test_definition_t this_test;
		new_default_test_definition(&this_test, TYPE_CREATURE, buf);
		instance->info_slot = select_multiple_cards_from_graveyard(player, player, 0, AI_MIN_VALUE, &this_test, max_cards, &instance->targets[0]);

		if( instance->info_slot > 0 ){
			int diff = true_get_updated_casting_cost(player, card, -1, event, 0);
			int amount = instance->info_slot += diff;
			charge_mana(player, COLOR_COLORLESS, amount);
		}
	}

	if (event == EVENT_RESOLVE_SPELL){
		card_instance_t *instance = get_card_instance(player, card);
		int i, num_validated = 0;
		for (i = 0; i < instance->info_slot; ++i){
			int selected = validate_target_from_grave(player, card, player, i);
			if (selected != -1){
				rfg_card_from_grave(player, selected);
				++num_validated;
			}
		}
		if (num_validated > 0){
			generate_tokens_by_id(player, card, CARD_ID_ZOMBIE, num_validated);
		} else if (instance->info_slot > 0) {
			spell_fizzled = 1;
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_misstep(int player, int card, event_t event){
/* CARD_ID_MISSTEP	4537
Misstep	|1|U
Sorcery
Creatures target player controls don't untap during that player's next untap step. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		int c;
		for(c = 0; c < active_cards_count[1-player]; c++){
			if( in_play(1-player, c) && is_what(1-player, c, TYPE_CREATURE) && is_tapped(1-player, c) ){
				ai_modifier += (2*get_power(1-player, c));
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int p = instance->targets[0].player;
			int count = active_cards_count[p]-1;
			while( count > -1 ){
					if( in_play(p, count) && is_what(p, count, TYPE_CREATURE) ){
						does_not_untap_effect(player, card, p, count, 0, 1);
					}
					count--;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET, &td, "TARGET_PLAYER", 1, NULL);
}

int card_moment_of_silence(int player, int card, event_t event)
{
/* CARD_ID_MOMENT_OF_SILENCE	4539
Moment of Silence	|W
Instant
Target player skips his or her next combat phase this turn. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td_player;
	default_target_definition(player, card, &td_player, 0);
	td_player.zone = TARGET_ZONE_PLAYERS;

	if (event == EVENT_CAN_CAST)
		return can_target(&td_player);

	if (event == EVENT_CAST_SPELL && affect_me(player, card))
		pick_target(&td_player, "TARGET_PLAYER");


	if (event == EVENT_RESOLVE_SPELL)
	{
		card_instance_t* instance = get_card_instance(player, card);
		if (valid_target(&td_player))
			target_player_skips_his_next_attack_step(player, card, instance->targets[0].player, 1);

		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_monkey_cage(int player, int card, event_t event){
/* Monkey Cage	|5
 * Artifact
 * When a creature enters the battlefield, sacrifice ~ and put X 2/2 |Sgreen Ape creature tokens onto the battlefield, where X
 *		is that creature's converted mana cost. */

	if( trigger_condition == TRIGGER_COMES_INTO_PLAY && affect_me(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( new_specific_cip_global(ANYBODY, event, RESOLVE_TRIGGER_MANDATORY, TYPE_CREATURE, 0, player, card) ){		
			int cmc = get_cmc(trigger_cause_controller, trigger_cause);
			generate_tokens_by_id(player, card, CARD_ID_MONKEY, cmc);
			kill_card(player, card, KILL_SACRIFICE);
		}
	}

	return 0;
}

static int moonlit_wake_effect(int player, int card, event_t event)
{

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    if (instance->damage_target_player > -1)
    {
      if (get_id(instance->damage_target_player, instance->damage_target_card) !=
          CARD_ID_MOONLIT_WAKE)
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
        count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, NULL);
      }
    }
  }

  int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (result && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    int amount = BYTE0(result);
    while (amount > 0)
    {
      gain_life(player, 1);
      --amount;
    }
    kill_card(player, card, KILL_EXILE);
  }

  if (event == EVENT_CLEANUP && !check_status(player, card, STATUS_INVISIBLE_FX))
  {
    kill_card(player, card, KILL_EXILE);
  }

  return 0;
}

int card_monlit_wake(int player, int card, event_t event)
{
  /* Moonlit Wake |2|W
   *   Enchantment
   *   Whenever a creature dies, you gain 1 life. */

  if (event == EVENT_STATIC_EFFECTS && affect_me(player, card))
  {
    int found = 0;
    int c;
    for (c = 0; c < active_cards_count[player]; c++)
    {
      if (in_play(player, c) && is_what(player, c, TYPE_EFFECT))
      {
        card_instance_t *inst = get_card_instance(player, c);
        if (inst->info_slot == (intptr_t)moonlit_wake_effect)
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
      int legacy = create_targetted_legacy_effect(player, card, &moonlit_wake_effect, player, card);
      card_instance_t *inst = get_card_instance(player, legacy);
      inst->targets[0].player = player;
      inst->targets[0].card = card;
      inst->number_of_targets = 1;
      add_status(player, legacy, STATUS_INVISIBLE_FX);
    }
  }

  if (event == EVENT_GRAVEYARD_FROM_PLAY)
  {
    count_for_gfp_ability(player, card, event, ANYBODY, TYPE_CREATURE, 0);
  }

  if (get_card_instance(player, card)->kill_code <= 0)
  {
    int result = resolve_gfp_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
    if (result)
    {
      int amount = BYTE0(result);
      while (amount > 0)
      {
        gain_life(player, 1);
        --amount;
      }
    }
  }

  return global_enchantment(player, card, event);
}

int card_muzzle(int player, int card, event_t event){
/* CARD_ID_MUZZLE	4542
Muzzle	|1|W
Enchantment - Aura
Enchant creature
Prevent all damage that would be dealt by enchanted creature. */

	if (event == EVENT_CHECK_ABILITIES){
		card_instance_t* instance = get_card_instance(player, card);
		if (affect_me(instance->damage_target_player, instance->damage_target_card) && in_play(player, card)){
			check_abilities_power -= get_power(instance->damage_target_player, instance->damage_target_card);
		}
	}
	if( in_play(player, card) && get_card_instance( player, card )->damage_target_player != -1 && ! is_humiliated(player, card, AL_STATIC) ){
		card_instance_t *instance = get_card_instance( player, card );
		int t_player = instance->damage_target_player;
		int t_card = instance->damage_target_card;
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance( affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card && damage->info_slot > 0){
				if( damage->damage_source_player == t_player && damage->damage_source_card == t_card ){
					prevent_damage(affected_card_controller, affected_card, -1);
				}
			}
		}
	}

	if (!IS_AURA_EVENT(player, card, event) && event != EVENT_STATIC_EFFECTS){ return 0; }

	return disabling_aura(player, card, event);
}

int card_natural_affinity(int player, int card, event_t event){
/* CARD_ID_NATURAL_AFFINITY	1095
Natural Affinity	|2|G
Instant
All lands become 2/2 creatures until end of turn. They're still lands. */

	if(event == EVENT_RESOLVE_SPELL){
		test_definition_t this_test2;
		default_test_definition(&this_test2, TYPE_LAND);
		land_animation2(player, card, 2, -1, 1, 2, 2, 0, 0, 0, &this_test2);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_nether_spirit(int player, int card, event_t event){
/* CARD_ID_NETHER_SPIRIT	4543
Nether Spirit	|1|B|B
Creature - Spirit 2/2
At the beginning of your upkeep, if ~ is the only creature card in your graveyard, you may return ~ to the battlefield. */

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		if( count_graveyard_by_type(player, TYPE_CREATURE) == 1 ){
			int choice = 0;
			if( ! duh_mode(player) ){
				choice = do_dialog(player, player, card, -1, -1," Return Nether Spirit\n Pass", 0);
			}
			if( choice == 0 ){
				put_into_play(player, card);
				return -1;
			}
		}
		return -2;
	}

	return 0;
}

int card_noble_purpose(int player, int card, event_t event){
  /* Noble Purpose |3|W|W
   * Enchantment
   * Whenever a creature you control deals combat damage, you gain that much life. */

    test_definition_t test;
	default_test_definition(&test, TYPE_CREATURE);
	check_damage_test(player, -1, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT, player, card, 0, &test);

	if( resolve_damage_trigger(player, card, event, DDBM_MUST_BE_COMBAT_DAMAGE | DDBM_REPORT_DAMAGE_DEALT, player, card) ){
		int amount = get_card_instance(player, card)->targets[16].player;
		get_card_instance(player, card)->targets[16].player = 0;
		gain_life(player, amount);
	}

	return global_enchantment(player, card, event);
}

int card_notorious_assassin(int player, int card, event_t event){
/* CARD_ID_NOTORIOUS_ASSASSIN	4545
Notorious Assassin	|3|B
Creature - Human Spellshaper Assassin 2/2
|2|B, |T, Discard a card: Destroy target non|Sblack creature. It can't be regenerated. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD|GAA_LITERAL_PROMPT, MANACOST_XB(2, 1), 0,
									&td, get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK));
}

int card_orims_cure(int player, int card, event_t event){
	/* Orim's Cure	|1|W	0x20050b1
	 * Instant
	 * If you control |Ha Plains, you may tap an untapped creature you control rather than pay this spell's mana cost.
	 * Prevent the next 4 damage that would be dealt to any target this turn. */
	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance( player, card );

		target_definition_t td2;
		default_target_definition(player, card, &td2, TYPE_CREATURE);
		td2.allowed_controller = player;
		td2.preferred_controller = player;
		td2.illegal_abilities = 0;
		td2.illegal_state = TARGET_STATE_TAPPED;

		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_PLAINS) && can_target(&td2) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, 0);
	td.extra = damage_card;

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Tap a creature you control\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				target_definition_t td2;
				default_target_definition(player, card, &td2, TYPE_CREATURE);
				td2.allowed_controller = player;
				td2.preferred_controller = player;
				td2.illegal_abilities = 0;
				td2.illegal_state = TARGET_STATE_TAPPED;
				if( new_pick_target(&td2, "Select a creature you control to tap.", 0, 1 | GS_LITERAL_PROMPT) ){
					action_on_target(player, card, 0, ACT_TAP);
					td.allow_cancel = 0;
					instance->number_of_targets = 0;
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		if( spell_fizzled != 1 ){
			pick_target(&td, "TARGET_DAMAGE");
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			card_instance_t *target = get_card_instance(instance->targets[0].player, instance->targets[0].card);
			if( target->info_slot <= 4 ){
				target->info_slot = 0;
			}
			else{
				target->info_slot-=4;
			}
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_DAMAGE_PREVENTION, &td, "TARGET_DAMAGE", 1, NULL);
	}

	return 0;
}

int card_overtaker(int player, int card, event_t event){
/* CARD_ID_OVERTAKER	4547
Overtaker	|1|U
Creature - Merfolk Spellshaper 1/1
|3|U, |T, Discard a card: Untap target creature and gain control of it until end of turn. That creature gains haste until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			effect_act_of_treason(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_XU(3, 1), 0,
									&td, "TARGET_CREATURE");
}

int card_pangosaur(int player, int card, event_t event){
/* CARD_ID_PANGOSAUR	4549
Pangosaur	|2|G|G
Creature - Lizard 6/6
Whenever a player plays a land, return ~ to its owner's hand. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_TRIG) ){
		if( whenever_a_player_plays_a_land(player, card, event, ANYBODY, RESOLVE_TRIGGER_MANDATORY) ){
			bounce_permanent(player, card);
		}
	}

	return 0;
}

int card_peat_bog(int player, int card, event_t event){
	/* Peat Bog	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |B|B to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_BLACK);
}

int card_power_matrix(int player, int card, event_t event){
/* CARD_ID_POWER_MATRIX	4553
Power Matrix	|4
Artifact
|T: Target creature gets +1/+1 and gains flying, first strike, and trample until end of turn. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.preferred_controller = player;

	return vanilla_creature_pumper(player, card, event, MANACOST0, GAA_UNTAPPED, 1, 1, KEYWORD_FIRST_STRIKE | KEYWORD_TRAMPLE |
									KEYWORD_FLYING, 0, &td);
}

static int puffer_extract_effect(int player, int card, event_t event){

	if( get_card_instance(player, card)->damage_target_player > -1 ){
		card_instance_t *instance = get_card_instance(player, card);

		int p = instance->damage_target_player;
		int c = instance->damage_target_card;
		int boost = instance->targets[1].player;

		if( (event == EVENT_POWER || event == EVENT_TOUGHNESS) && affect_me(p, c) && boost > 0 ){
			event_result += boost;
		}
	}

	if( event == EVENT_CLEANUP ){
		card_instance_t *instance = get_card_instance(player, card);
		kill_card(instance->damage_target_player, instance->damage_target_card, KILL_DESTROY);
		kill_card(player, card, KILL_EXILE);
	}

	return 0;
}

int card_puffer_extract(int player, int card, event_t event){
/* CARD_ID_PUFFER_EXTRACT	4555
Puffer Extract	|5
Artifact
|X, |T: Target creature you control gets +X/+X until end of turn. Destroy it at the beginning of the next end step. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			int legacy = create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, &puffer_extract_effect,
														instance->targets[0].player, instance->targets[0].card);
			card_instance_t *leg = get_card_instance(instance->parent_controller, legacy);
			leg->targets[1].player = instance->info_slot;
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(-1), 0, &td, "TARGET_CREATURE");
}

int card_pulverize(int player, int card, event_t event){
  /* Pulverize |4|R|R
   * 	  Sorcery
   * 	  You may sacrifice two Mountains rather than pay Pulverize's mana cost.
   * 	  Destroy all artifacts. */

	if( event == EVENT_MODIFY_COST ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a Mountain to sacrifice.");
		test.subtype = SUBTYPE_MOUNTAIN;
		test.qty = 2;

		card_instance_t *instance = get_card_instance( player, card );

		if( new_can_sacrifice_as_cost(player, card, &test) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance( player, card );
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Sacrifice 2 Mountains\n Play normally\n Cancel", 0);
			}
			if( choice == 0 ){
				test_definition_t test;
				new_default_test_definition(&test, TYPE_PERMANENT, "Select a Mountain to sacrifice.");
				test.subtype = SUBTYPE_MOUNTAIN;
				int sacced[2];
				int sc = 0;
				int sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_AS_COST, &test);
				if( sac ){
					sacced[sc] = sac;
					sc++;
					sac = new_sacrifice(player, card, player, SAC_JUST_MARK | SAC_RETURN_CHOICE | SAC_AS_COST, &test);
					if( sac ){
						sacced[sc] = sac;
						int i;
						for(i=0; i<2; i++){
							state_untargettable(BYTE2(sacced[i]), BYTE3(sacced[i]), 0);
							kill_card(BYTE2(sacced[i]), BYTE3(sacced[i]), KILL_SACRIFICE);
						}
					}
					else{
						state_untargettable(BYTE2(sacced[0]), BYTE3(sacced[0]), 0);
						spell_fizzled = 1;
						return 0;
					}
				}
				else{
					spell_fizzled = 1;
					return 0;
				}
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		test_definition_t this_test;
		default_test_definition(&this_test, TYPE_ARTIFACT);
		new_manipulate_all(player, card, ANYBODY, &this_test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_puppets_verdict(int player, int card, event_t event)
{
  /* Puppet's Verdict	|1|R|R
   * Instant
   * Flip a coin. If you win the flip, destroy all creatures with power 2 or less. If you lose the flip, destroy all creatures with power 3 or greater. */

	// Flip a coin. If you win the flip, destroy all creatures with power 2 or less.
	// If you lose the flip, destroy all creatures with power 3 or greater.

	if (event == EVENT_RESOLVE_SPELL)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_CREATURE, "");

		if (flip_a_coin(player, card))
		{
			test.power = 3;
			test.power_flag = F5_POWER_LESSER_THAN_VALUE;
		}
		else
		{
			test.power = 2;
			test.power_flag = F5_POWER_GREATER_THAN_VALUE;
		}
		new_manipulate_all(player, card, ANYBODY, &test, KILL_DESTROY);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_ramosian_sergeant(int player, int card, event_t event){
/* CARD_ID_RAMOSIAN_SERGEANT	4564
Ramosian Sergeant	|W
Creature - Human Rebel 1/1
|3, |T: Search your library for a Rebel permanent card with converted mana cost 2 or less and put it onto the battlefield. Then shuffle your library.*/

  return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 3, 0);
}

int card_ramosian_captain(int player, int card, event_t event){
/* CARD_ID_RAMOSIAN_CAPTAIN	4560
Ramosian Captain	|1|W|W
Creature - Human Rebel 2/2
First strike
|5, |T: Search your library for a Rebel permanent card with converted mana cost 4 or less and put it onto the battlefield. Then shuffle your library.*/

  return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 5, 0);
}

int card_ramosian_commander(int player, int card, event_t event ){
/* CARD_ID_RAMOSIAN_COMMANDER	4561
Ramosian Commander	|2|W|W
Creature - Human Rebel 2/4
|6, |T: Search your library for a Rebel permanent card with converted mana cost 5 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 6, 0);
}

int card_ramosian_liutenant(int player, int card, event_t event ){
/* CARD_ID_RAMOSIAN_LIEUTENANT	4562
Ramosian Lieutenant	|1|W
Creature - Human Rebel 1/2
|4, |T: Search your library for a Rebel permanent card with converted mana cost 3 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 4, 0);
}

int card_ramosian_sky_marshall(int player, int card, event_t event ){
/* CARD_ID_RAMOSIAN_SKY_MARSHAL	4565
Ramosian Sky Marshal	|3|W|W
Creature - Human Rebel 3/3
Flying
|7, |T: Search your library for a Rebel permanent card with converted mana cost 6 or less and put it onto the battlefield. Then shuffle your library.*/

	return generic_subtype_searcher(player, card, event, SUBTYPE_REBEL, 7, 0);
}

int card_reverent_mantra(int player, int card, event_t event){
/* CARD_ID_REVERENT_MANTRA	4571
Reverent Mantra	|3|W
Instant
You may exile a |Swhite card from your hand rather than pay ~'s mana cost.
Choose a color. All creatures gain protection from the chosen color until end of turn. */

	al_pitchspell(player, card, event, COLOR_WHITE, 0);

	if( event == EVENT_CAN_CAST ){
		return basic_spell(player, card, event);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! casting_al_pitchspell(player, card, event, COLOR_WHITE, 0) ){
			spell_fizzled = 1;
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		int keyword = select_a_protection(player) | KEYWORD_RECALC_SET_COLOR;
		APNAP(p, { pump_subtype_until_eot(player, card, p, -1, 0, 0, keyword, 0); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_remote_farm(int player, int card, event_t event){
	/* Remote Farm	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |W|W to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_WHITE);
}

int card_renounce(int player, int card, event_t event)
{
  /* Renounce	|1|W
   * Instant
   * Sacrifice any number of permanents. You gain 2 life for each permanent sacrificed this way. */

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice");
		if( life[player] < 6 && new_can_sacrifice(player, card, player, &test) ){
			ai_modifier += 48;
		}
	}

	if (event == EVENT_RESOLVE_SPELL)
	{
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT, "Select a permanent to sacrifice");
		if (IS_AI(player))
		{
			int c, max_can_sac = 0;
			for (c = 0; c < active_cards_count[player]; ++c)
				if (in_play(player, c) && is_what(player, c, TYPE_PERMANENT))
					++max_can_sac;

			test.qty = recorded_rand(player, max_can_sac + 1);
		}
		else{
			test.qty = 151;
		}

		if (test.qty > 0){
			gain_life(player, 2 * new_sacrifice(player, card, player, SAC_NO_CANCEL|SAC_DONE, &test));
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_revive(int player, int card, event_t event){
/* CARD_ID_REVIVE	1130
Revive	|1|G
Sorcery
Return target |Sgreen card from your graveyard to your hand. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	test_definition_t this_test;
	new_default_test_definition(&this_test, TYPE_ANY, get_sleighted_color_text(player, card, "Select a %s card.", COLOR_GREEN));
	this_test.color = get_sleighted_color_test(player, card, COLOR_TEST_GREEN);

	if( event == EVENT_RESOLVE_SPELL ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_GRAVE_RECYCLER, NULL, NULL, 1, &this_test);
}

// Rishadan Airship --> Skywinder Drake

int card_rishadan_brigand(int player, int card, event_t event){
/* CARD_ID_RISHADAN_BRIGAND	4574
Rishadan Brigand	|4|U
Creature - Human Pirate 3/2
Flying
When ~ enters the battlefield, each opponent sacrifices a permanent unless he or she pays |3.
~ can block only creatures with flying. */

	return rishadan_delinquents(player, card, event, 3);
}

int card_rishadan_cutpurse(int player, int card, event_t event){
/* CARD_ID_RISHADAN_CUTPURSE	4575
Rishadan Cutpurse	|2|U
Creature - Human Pirate 1/1
When ~ enters the battlefield, each opponent sacrifices a permanent unless he or she pays |1. */

	return rishadan_delinquents(player, card, event, 1);
}

int card_rishadan_footpad(int player, int card, event_t event){
/* CARD_ID_RISHADAN_FOOTPAD	4576
Rishadan Footpad	|3|U
Creature - Human Pirate 2/2
When ~ enters the battlefield, each opponent sacrifices a permanent unless he or she pays |2. */

	return rishadan_delinquents(player, card, event, 2);
}

int card_rishadan_pawnshop(int player, int card, event_t event){

	/* Rishadan Pawnshop	|2
	 * Artifact
	 * |2, |T: Shuffle target nontoken permanent you control into its owner's library. */

	if (!IS_GAA_EVENT(event)){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_PERMANENT);
	td.illegal_type = TARGET_TYPE_TOKEN;
	td.allowed_controller = player;
	td.preferred_controller = player;

	card_instance_t* instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		if( IS_AI(player) ){
			td.required_state = TARGET_STATE_DESTROYED;
			return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_REGENERATION, MANACOST_X(2), 0, &td, NULL);
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PERMANENT");
	}

	if( event == EVENT_ACTIVATE ){
		if( IS_AI(player) ){
			td.required_state = TARGET_STATE_DESTROYED;
			ai_modifier += 24;
		}
		return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST_X(2), 0, &td, "TARGET_PERMANENT");
	}

	if( event == EVENT_RESOLVE_ACTIVATION && valid_target(&td)){
		shuffle_into_library(instance->targets[0].player, instance->targets[0].card);
	}

	return 0;
}

int card_rishadan_port(int player, int card, event_t event){
	/* Rishadan Port	""
	 * Land
	 * |T: Add |C to your mana pool.
	 * |1, |T: Tap target land. */

	if( ! IS_GAA_EVENT(event) || event == EVENT_CAN_ACTIVATE ){
		return mana_producer(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance( player, card );

	enum{
		CHOICE_MANA = 1,
		CHOICE_TAP_LAND
	};

	if( event == EVENT_ACTIVATE ){
		int choice = CHOICE_MANA;
		if( ! paying_mana() &&
			generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, GAA_CAN_TARGET | GAA_UNTAPPED, MANACOST_X(2), 0, &td, NULL) )
		{
			choice = CHOICE_TAP_LAND;
			ai_modifier += (current_turn == player ? -24 : 24);
			ai_modifier += (count_subtype(1-player, TYPE_LAND, -1) <= 3 ? -24 : 24);
		}
		instance->info_slot = choice;
		if( choice == CHOICE_TAP_LAND && ! IS_AI(player) ){
			choice = DIALOG(player, card, event, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1,
							"Get mana", 1, 5,
							"Tap a land", 1, 10);
			if( ! choice ){
				spell_fizzled = 1;
				return 0;
			}
		}

		if( choice == CHOICE_MANA){
			return mana_producer(player, card, event);
		}

		if( choice == CHOICE_TAP_LAND ){
			add_state(player, card, STATE_TAPPED);
			generic_activated_ability(player, card, event, GAA_CAN_TARGET, MANACOST_X(1), 0, &td, "TARGET_LAND");
			if( spell_fizzled == 1 ){
				remove_state(player, card, STATE_TAPPED);
			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_TAP_LAND ){
			if( valid_target(&td) ){
				tap_card(instance->targets[0].player, instance->targets[0].card);
			}
		}
	}

	return 0;
}

int card_robber_fly(int player, int card, event_t event){
  /* Robber Fly |2|R
   * 	Creature — Insect 1/1
   * 	Flying
   * 	Whenever Robber Fly becomes blocked, defending player discards all the cards in his or her hand, then draws that many cards. */

	if( event == EVENT_DECLARE_BLOCKERS && !is_humiliated(player, card, AL_TRIG) ){
		if( check_state(player, card, STATE_ATTACKING) && ! is_unblocked(player, card) ){
			int amount = hand_count[1-player];
			new_discard_all(1-player, player);
			draw_cards(1-player, amount);
		}
	}

	return 0;
}

int card_rushwood_elemental(int player, int card, event_t event){
/* CARD_ID_RUSHWOOD_ELEMENTAL	4582
Rushwood Elemental	|G|G|G|G|G
Creature - Elemental 4/4
Trample
At the beginning of your upkeep, you may put a +1/+1 counter on ~. */

	upkeep_trigger_ability_mode(player, card, event, player, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_UPKEEP_TRIGGER_ABILITY ){
		add_counters(player, card, player, card, COUNTER_P1_P1, 1);
	}

	return 0;
}

int card_rushwood_herbalist(int player, int card, event_t event){
/* CARD_ID_RUSHWOOD_HERBALIST	4584
Rushwood Herbalist	|2|G
Creature - Human Spellshaper 2/2
|G, |T, Discard a card: Regenerate target creature. */

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

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) && can_be_regenerated(instance->targets[0].player, instance->targets[0].card) ){
			regenerate_target( instance->targets[0].player, instance->targets[0].card );
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_REGENERATION|GAA_DISCARD, MANACOST_G(1), 0,
									&td, "TARGET_CREATURE");
}

int card_saber_ants(int player, int card, event_t event ){
	/* Saber Ants	|3|G
	 * Creature - Insect 2/3
	 * Whenever ~ is dealt damage, you may put that many 1/1 |Sgreen Insect creature tokens onto the battlefield. */
	// Used externally -> card_broodhatch_nantuko
	if( ! is_humiliated(player, card, AL_TRIG) && damage_dealt_to_me_arbitrary(player, card, event, DDBM_TRIGGER_OPTIONAL, player, card) ){
		token_generation_t token;
		default_token_definition(player, card, CARD_ID_INSECT, &token);
		token.qty = get_card_instance(player, card)->targets[7].card;
		generate_token(&token);
		get_card_instance(player, card)->targets[7].card = 0;
    }

	return 0;
}

int card_sand_squid(int player, int card, event_t event){
/* CARD_ID_SAND_SQUID	4589
Sand Squid	|3|U
Creature - Squid Beast 2/2
|H2Islandwalk (This creature can't be blocked as long as defending player controls |Han Island.)
You may choose not to untap ~ during your untap step.
|T: Tap target creature. That creature doesn't untap during its controller's untap step for as long as ~ remains tapped. */

	choose_to_untap(player, card, event);

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE );

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			tap_card(instance->targets[0].player, instance->targets[0].card);
			does_not_untap_until_im_tapped(instance->parent_controller, instance->parent_card, instance->targets[0].player, instance->targets[0].card);
			card_instance_t *parent = get_card_instance( player, instance->parent_card );
			parent->targets[1] = instance->targets[0];
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET, MANACOST0, 0, &td, "TARGET_CREATURE");
}

int card_sandstone_needle(int player, int card, event_t event){
	/* Sandstone Needle	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |R|R to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_RED);
}

/* ManalinkEh 2022: 0x2179e10; original cards/mercadian_masques.c:3971. */
int card_saprazzan_breaker(int player, int card, event_t event)
{
  /* Saprazzan Breaker	|4|U
   * Creature - Beast 3/3
   * |U: Mill a card. If a land card was milled this way, this creature can't be blocked this turn. */

  int result;
  card_instance_t *instance;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = *deck_ptr[player];
    if (result != -1)
    {
      mill(player, 1);
      result = is_what(-1, result, 1);
      if (result != 0)
      {
        pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                               instance->parent_controller, instance->parent_card, 0, 0, 0, 0x10);
      }
    }
  }
  result = generic_activated_ability(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, NULL, NULL);
  return result;
}

int card_saprazzan_heir(int player, int card, event_t event){
  /* Saprazzan Heir |1|U
   * Creature — Merfolk 1/1
   * Whenever Saprazzan Heir becomes blocked, you may draw three cards. */

	if( event == EVENT_CAN_ACTIVATE && current_phase == PHASE_AFTER_BLOCKING && is_attacking(player, card) &&
		! is_unblocked(player, card) && get_card_instance( player, card )->targets[1].player != 66 &&
		! is_humiliated(player, card, AL_TRIG) )
	{
		return 1;
	}

	if( event == EVENT_ACTIVATE ){
		get_card_instance( player, card )->targets[1].player = 66;
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		draw_cards(player, 3);
	}

	if( event == EVENT_CLEANUP ){
		get_card_instance( player, card )->targets[1].player = 0;
	}

	return 0;
}

int card_saprazzan_skerry(int player, int card, event_t event){
	/* Saprazzan Skerry	""
	 * Land
	 * ~ enters the battlefield tapped with two depletion counters on it.
	 * |T, Remove a depletion counter from ~: Add |U|U to your mana pool. If there are no depletion counters on ~, sacrifice it. */
	return depletion_land(player, card, event, COLOR_BLUE);
}

int card_seismic_mage(int player, int card, event_t event){
/* CARD_ID_SEISMIC_ASSAULT	1142
Seismic Assault	|R|R|R
Enchantment
Discard a land card: ~ deals 2 damage to target creature or player. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_XR(2, 1), 0, &td, "TARGET_LAND");
}

int card_sever_soul(int player, int card, event_t event){
/* CARD_ID_SEVER_SOUL	1144
Sever Soul	|3|B|B
Sorcery
Destroy target non|Sblack creature. It can't be regenerated. You gain life equal to its toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			int amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			gain_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_silverglade_elemental(int player, int card, event_t event){
  /* Silverglade Elemental |4|G
   * Creature — Elemental 4/4
   * When Silverglade Elemental enters the battlefield, you may search your library for a Forest card and put that card onto the battlefield.
   * 	If you do, shuffle your library. */

	etb_ability(player, card, event, RESOLVE_TRIGGER_AI(player));

	if( event == EVENT_ETB_ABILITY ){
		test_definition_t test;
		new_default_test_definition(&test, TYPE_PERMANENT,
									get_hacked_land_text(player, card, "Select a %s card.", SUBTYPE_FOREST));
		test.subtype = get_hacked_subtype(player, card, SUBTYPE_FOREST);
		new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
	}

	return 0;
}

int card_silverglade_pathfinder(int player, int card, event_t event){
/* Silverglade Pathfinder	|1|G
 * Creature - Dryad Spellshaper 1/1
 * |1|G, |T, Discard a card: Search your library for a basic land card and put that card onto the battlefield tapped.
 *		Then shuffle your library. */

	if( event == EVENT_RESOLVE_ACTIVATION ){
		tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 1);
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_DISCARD, MANACOST_XG(1, 1), 0, NULL, NULL);
}

int card_sizzle(int player, int card, event_t event){
/* CARD_ID_SIZZLE	1149
Sizzle	|2|R
Sorcery
~ deals 3 damage to each opponent. */

	if( event == EVENT_RESOLVE_SPELL ){
		damage_player(1-player, 3, player, card);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

// Skulking fugitive --> Skulking ghost

int card_skull_of_ramos(int player, int card, event_t event){
/* CARD_ID_SKULL_OF_RAMOS	4607
Skull of Ramos	|3
Artifact
|T: Add |B to your mana pool.
Sacrifice ~: Add |B to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_BLACK);
}

int card_snuff_out(int player, int card, event_t event){
  /* Snuff Out |3|B
   * 	  Instant
   * 	  If you control a Swamp, you may pay 4 life rather than pay Snuff Out's mana cost.
   * 	  Destroy target nonblack creature. It can't be regenerated. */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance( player, card );
		if( check_battlefield_for_subtype(player, TYPE_LAND, SUBTYPE_SWAMP) && can_pay_life(player, 4) ){
			instance->info_slot = 1;
			null_casting_cost(player, card);
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
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td, NULL, 1, NULL);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		instance->number_of_targets = 0;
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_iid(player, get_card_instance(player, card)->internal_card_id, event) ){
				choice = do_dialog(player, player, card, -1, -1, " Pay 4 life\n Play normally\n Cancel", life[player] >= 10 ? 0 : 1);
			}
			if( choice == 0 ){
				lose_life(player, 4);
				td.allow_cancel = 0;
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
				return 0;
			}
		}
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
							get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
	}

	if( event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td) ){
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	if( event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CHANGE_TARGET ){
		return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td, NULL, 1, NULL);
	}

	return 0;
}

int card_soothsaying(int player, int card, event_t event){
  /* Soothsaying |U
   * Enchantment
   * {3}{U}{U}: Shuffle your library.
   * {X}: Look at the top X cards of your library, then put them back in any order. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	enum{
		CHOICE_SHUFFLE = 1,
		CHOICE_REARRANGE
	};

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE ){
		return generic_activated_ability(player, card, event, 0, MANACOST_X(-1), 0, NULL, NULL);
	}

	if( event == EVENT_ACTIVATE ){
		int lands = count_subtype(player, TYPE_LAND, -1);
		if( lands <= 3 || lands > 6 ){
			ai_modifier += 48;
		}
		instance->targets[0].player = 0;
		int choice = DIALOG(player, card, event, DLG_RANDOM,
							"Shuffle", has_mana_for_activated_ability(player, card, MANACOST_XU(3, 2)), 5,
							"Rearrange", 1, 10);
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( choice == CHOICE_SHUFFLE ){
			return generic_activated_ability(player, card, event, 0, MANACOST_XU(3, 2), 0, NULL, NULL);
		}
		if( choice == CHOICE_REARRANGE ){
			if( charge_mana_for_activated_ability(player, card, MANACOST_X(-1)) ){
			     instance->targets[0].player = x_value;

			}
		}
	}

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( instance->info_slot == CHOICE_SHUFFLE ){
			shuffle(player);
		}
		if( instance->info_slot == CHOICE_REARRANGE ){
			rearrange_top_x(player, player, instance->targets[0].player);
		}
	}

	return 0;
}

int card_spidersilk_armor(int player, int card, event_t event){
/* CARD_ID_SPIDERSILK_ARMOR	1333
Spidersilk Armor	|2|G
Enchantment
Creatures you control get +0/+1 and have reach. (They can block creatures with flying.) */

	boost_creature_type(player, card, event, -1, 0, 1, KEYWORD_REACH, BCT_CONTROLLER_ONLY);

	return global_enchantment(player, card, event);
}

int card_spiritual_focus(int player, int card, event_t event){
/* CARD_ID_SPIRITUAL_FOCUS	4615
Spiritual Focus	|1|W
Enchantment
Whenever a spell or ability an opponent controls causes you to discard a card, you gain 2 life and you may draw a card. */

	if (discard_trigger(player, card, event, player, RESOLVE_TRIGGER_MANDATORY, DISCARD_NO_NORMAL)){
		gain_life(player, 2);
		draw_some_cards_if_you_want(player, card, player, 1);
	}

	return global_enchantment(player, card, event);
}

int card_spontaneous_generation(int player, int card, event_t event){
	/* Spontaneous Generation	|3|G
	 * Sorcery
	 * Put a 1/1 |Sgreen Saproling creature token onto the battlefield for each card in your hand. */

	if( event == EVENT_RESOLVE_SPELL ){
		generate_tokens_by_id(player, card, CARD_ID_SAPROLING, hand_count[player]);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

int card_squee_goblin_nabob(int player, int card, event_t event){
/* CARD_ID_SQUEE_GOBLIN_NABOB	4619
Squee, Goblin Nabob	|2|R
Legendary Creature - Goblin 1/1
At the beginning of your upkeep, you may return ~ from your graveyard to your hand. */

	check_legend_rule(player, card, event);

	if( event == EVENT_GRAVEYARD_ABILITY_UPKEEP ){
		int choice = 0;
		if( ! duh_mode(player) ){
			choice = do_dialog(player, player, card, -1, -1," Return Squee to hand\n Leave Squee be\n", 0);
		}
		if( choice == 0 ){
			card_instance_t *instance= get_card_instance(player, card);
			instance->state &= ~STATE_INVISIBLE;
			hand_count[player]++;
			return -1;
		}
		else{
			return -2;
		}
	}

	return 0;
}

int card_squeeze(int player, int card, event_t event){
/* CARD_ID_SQUEEZE	4620
Squeeze	|3|U
Enchantment
Sorcery spells cost |3 more to cast. */

	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_MODIFY_COST_GLOBAL && is_what(affected_card_controller, affected_card, TYPE_SORCERY) ){
			COST_COLORLESS+=3;
		}
	}

	return global_enchantment(player, card, event);
}

int card_statecraft(int player, int card, event_t event ){
  /* Statecraft |3|U
   * Enchantment
   * Prevent all combat damage that would be dealt to and dealt by creatures you control. */

	if (event == EVENT_CHECK_ABILITIES && affected_card_controller == player){
		check_abilities_power -= get_power(player, affected_card);
		check_abilities_toughness += 0x3fff;
	}
	if( in_play(player, card) && ! is_humiliated(player, card, AL_STATIC) ){
		if( event == EVENT_PREVENT_DAMAGE ){
			card_instance_t *damage= get_card_instance(affected_card_controller, affected_card);
			if( damage->internal_card_id == damage_card &&
				(damage->token_status & (STATUS_COMBAT_DAMAGE|STATUS_FIRST_STRIKE_DAMAGE)) )
			{
				if( damage->damage_target_card != -1 && damage->damage_target_player == player &&
					damage->info_slot > 0 && is_what(damage->damage_target_player, damage->damage_target_card, TYPE_CREATURE) )
				{
					prevent_damage(affected_card_controller, affected_card, -1);
				}
				if( damage->damage_source_card != -1 && damage->damage_source_player == player &&
					damage->info_slot > 0 && is_what(damage->damage_source_player, damage->damage_source_card, TYPE_CREATURE) )
				{
					prevent_damage(affected_card_controller, affected_card, -1);
				}
			}
		}
	}

	return global_enchantment(player, card, event);
}

const char *colored_damage_source_that_damages_me(int who_chooses, int player, int card,
                                                  int targeting_player, int targeting_card)
{
  card_instance_t *damage_or_source;
  card_instance_t *source_in_play;
  int result;
  unsigned int damage_colors;

  damage_or_source = get_card_instance(player, card);
  if (((damage_or_source->internal_card_id == damage_card) &&
       (damage_or_source->damage_target_player == who_chooses)) &&
      (damage_or_source->damage_target_card == -1))
  {
    damage_colors = (unsigned int)damage_or_source->initial_color;
    source_in_play =
        in_play((int)damage_or_source->damage_source_player, damage_or_source->damage_source_card);
    if (source_in_play != NULL)
    {
      result = check_special_flags((int)damage_or_source->damage_source_player,
                                   damage_or_source->damage_source_card, 0x40);
      if (result != 0)
      {
        return "damaging a Planeswalker.";
      }
      damage_colors = get_color((int)damage_or_source->damage_source_player,
                                damage_or_source->damage_source_card);
    }
    damage_or_source = get_card_instance(targeting_player, targeting_card);
    if ((damage_or_source->info_slot & damage_colors) != 0)
    {
      return NULL;
    }
  }
  return (char *)gs_target_error_color;
}

int card_story_circle(int player, int card, event_t event){
/* CARD_ID_STORY_CIRCLE	1163
Story Circle	|1|W|W
Enchantment
As ~ enters the battlefield, choose a color.
|W: The next time a source of your choice of the chosen color would deal damage to you this turn, prevent that damage. */


	if( event == EVENT_RESOLVE_SPELL ){
		get_card_instance(player, card)->info_slot = 1<<choose_a_color_and_show_legacy(player, card, 1-player, -1);
	}

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_EFFECT);
	td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
	td.extra = target_predicate_extra(colored_damage_source_that_damages_me);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
		}
	}

	return generic_activated_ability(player, card, event, GAA_CAN_TARGET | GAA_DAMAGE_PREVENTION, MANACOST_W(1), 0, &td, "TARGET_DAMAGE");
}

int card_task_force(int player, int card, event_t event){
/* CARD_ID_TASK_FORCE	4628
Task Force	|2|W
Creature - Human Rebel 1/3
Whenever ~ becomes the target of a spell or ability, it gets +0/+3 until end of turn. */

  if (in_play(player, card)
	  && !is_humiliated(player, card, AL_TRIG)
	  && becomes_target_of_spell_or_effect(player, card, event, player, card, ANYBODY))
	pump_until_eot(player, card, player, card, 0, 3);

  return 0;
}

int card_tectonic_break(int player, int card, event_t event){
/* CARD_ID_TECTONIC_BREAK	4629
Tectonic Break	|X|R|R
Sorcery
Each player sacrifices X lands. */

	if( event == EVENT_RESOLVE_SPELL ){
		card_instance_t *instance = get_card_instance(player, card);

		test_definition_t test;
		new_default_test_definition(&test, TYPE_LAND, "Select a land to sacrifice.");
		test.qty = instance->info_slot;
		APNAP(p, { new_sacrifice(player, card, p, SAC_CAUSED | SAC_NO_CANCEL, &test); };);
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_X_SPELL, NULL, NULL, 0, NULL);
}

int card_thrashing_wumpus(int player, int card, event_t event){
/* CARD_ID_THRASHING_WUMPUS	1259
Thrashing Wumpus	|3|B|B
Creature - Beast 3/3
|B: ~ deals 1 damage to each creature and each player. */

  return pestilence_impl(player, card, event, 0, COLOR_BLACK);
}

int card_thwart(int player, int card, event_t event){
	/* Thwart	|2|U|U
	 * Instant
	 * You may return three |H1Islands you control to their owner's hand rather than pay ~'s mana cost.
	 * Counter target spell. */

	if( event == EVENT_MODIFY_COST ){
		card_instance_t *instance = get_card_instance(player, card);
		if( count_subtype(player, TYPE_LAND, SUBTYPE_ISLAND) > 2 ){
			null_casting_cost(player, card);
			instance->info_slot = 1;
		}
		else{
			instance->info_slot = 0;
		}
	}

	if( event == EVENT_CAN_CAST ){
		return counterspell(player, card, event, NULL, 0);
	}

	if( event == EVENT_CAST_SPELL && affect_me(player, card) ){
		card_instance_t *instance = get_card_instance(player, card);
		if( ! played_for_free(player, card) && ! is_token(player, card) && instance->info_slot == 1 ){
			int choice = 0;
			if( has_mana_to_cast_id(player, event, get_id(player, card)) ){
				choice = do_dialog(player, player, card, -1, -1, " Bounce 3 Islands\n Cast normally\n Cancel", 0);
			}
			if( choice == 0 ){
				target_definition_t td;
				default_target_definition(player, card, &td, TYPE_LAND);
				td.allowed_controller = player;
				td.preferred_controller = player;
				td.required_subtype = SUBTYPE_ISLAND;
				td.illegal_abilities = 0;

				instance->number_of_targets = 0;

				int trgs = 0;
				while( trgs < 3 ){
						if( new_pick_target(&td, "Select an Island to return to your hand.", trgs, GS_LITERAL_PROMPT) ){
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
					if( trgs == 3 ){
						bounce_permanent(instance->targets[i].player, instance->targets[i].card);
					}
				}
				if( trgs < 3 ){
					spell_fizzled = 1;
					return 0;
				}
				instance->number_of_targets = 0;
			}
			else if( choice == 1 ){
					charge_mana_from_id(player, card, event, get_id(player, card));
			}
			else{
				spell_fizzled = 1;
			}
		}
		if( spell_fizzled != 1 ){
			return counterspell(player, card, event, NULL, 0);
		}
	}

	if( event == EVENT_RESOLVE_SPELL ){
		return counterspell(player, card, event, NULL, 0);
	}

	return 0;
}

int card_tooth_of_ramos(int player, int card, event_t event){
/* CARD_ID_TOOTH_OF_RAMOS	4637
Tooth of Ramos	|3
Artifact
|T: Add |W to your mana pool.
Sacrifice ~: Add |W to your mana pool. */

	return part_of_ramos(player, card, event, COLOR_WHITE);
}

/* ManalinkEh 2022: 0x217b7e0; original cards/mercadian_masques.c:4568. */
int card_tower_of_the_magistrate(int player, int card, event_t event)
{
  /* Tower of the Magistrate	""
   * Land
   * |T: Add |C.
   * |1, |T: Target creature gains protection from artifacts until end of turn. */

  enum
  {
    CHOICE_MANA = 1,
    CHOICE_PROTECTION_ARTIFACTS = 2
  };

  int result;
  card_instance_t *instance;
  bool not_speculating;
  target_definition_t td;

  if (event != EVENT_CAN_ACTIVATE)
  {
    if (event == EVENT_ACTIVATE)
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);

      result = paying_mana();
      if ((result == 0) && (result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5,
                                                               2, 0, 0, 0, 0, 0, 0, &td, NULL),
                            result != 0))
      {
        not_speculating = ai_is_speculating != 1;
        instance->info_slot = 2;
        if ((not_speculating) && ((player != 1 || (((unsigned char)trace_mode & 2) != 0))))
        {
          result = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, DLG_AUTOCHOOSE_IF_1, "Get mana", 1,
                               1, "Give protection from artifact", 1, 1, 0, 0, 0, 0);
          if (result == 0)
          {
            spell_fizzled = 1;
            return 0;
          }
          if (result == CHOICE_MANA)
            return mana_producer(player, card, EVENT_ACTIVATE);
          if (result != CHOICE_PROTECTION_ARTIFACTS)
          {
            return 0;
          }
        }
        add_state(player, card, 0x10);
        generic_activated_ability(player, card, EVENT_ACTIVATE, 5, 1, 0, 0, 0, 0, 0, 0, &td,
                                  "TARGET_CREATURE");
        if (spell_fizzled == 1)
        {
          remove_state(player, card, 0x10);
        }
        return 0;
      }
      instance->info_slot = 1;
      return mana_producer(player, card, EVENT_ACTIVATE);
    }
    if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if (event != EVENT_RESOLVE_ACTIVATION)
      {
        return 0;
      }
      if (instance->info_slot != 2)
      {
        return 0;
      }
      result = valid_target(&td);
      if (result == 0)
      {
        return 0;
      }
      pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                             0, 0, 0x10000, 0);
      return 0;
    }
  }
  return mana_producer(player, card, event);
}

int card_toymaker(int player, int card, event_t event){
  /* CARD_ID_TOYMAKER	4639
   * Toymaker	|2
   * Artifact Creature - Spellshaper 1/1
   * |1, |T, Discard a card: Target noncreature artifact becomes an artifact creature with power and toughness each equal to its converted mana cost until end of turn.
   * (It retains its abilities.) */

//UNUSEDCARD


	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_ARTIFACT);
	td.illegal_type = TYPE_CREATURE;
	td.preferred_controller = player;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			artifact_animation(player, instance->parent_card, instance->targets[0].player, instance->targets[0].card, 1, -1, -1, 0, 0);
		}
	}
	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_X(1), 0, &td, "TARGET_ARTIFACT");
}

int card_trade_routes(int player, int card, event_t event){
  /* Trade Routes |1|U
   * 	  Enchantment
   * 	  {1}: Return target land you control to its owner's hand.
   * 	  {1}, Discard a land card: Draw a card. */

	if( ! IS_GAA_EVENT(event) ){
		return global_enchantment(player, card, event);
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_LAND);
	td.preferred_controller = player;
	td.allowed_controller = player;
	td.illegal_abilities = 0;

	test_definition_t test;
	new_default_test_definition(&test, TYPE_LAND, "Select a land card to discard.");
	test.zone = TARGET_ZONE_HAND;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_ACTIVATE && generic_activated_ability(player, card, event, 0, MANACOST_X(1), 0, NULL, NULL) ){
		if( can_target(&td) ){
			return 1;
		}
		return check_battlefield_for_special_card(player, card, player, 0, &test);
	}

	if( event == EVENT_ACTIVATE ){
		int choice = DIALOG(player, card, event, DLG_RANDOM, DLG_NO_STORAGE,
							"Return a land to your hand", can_target(&td), 5,
							"Discard a land card and draw", check_battlefield_for_special_card(player, card, player, 0, &test), 5 + (7-hand_count[player]));
		if( ! choice ){
			spell_fizzled = 1;
			return 0;
		}
		if( charge_mana_for_activated_ability(player, card, MANACOST_X(1)) ){
			if( choice == 1 ){
				new_pick_target(&td, "Select target land you control.", 0, 1 | GS_LITERAL_PROMPT);
			}
			if( choice == 2 ){
				int selected = new_select_a_card(player, player, TUTOR_FROM_HAND, 0, AI_MIN_VALUE, -1, &test);
				if( selected != -1 ){
					discard_card(player, selected);
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
		if( instance->info_slot == 1 && valid_target(&td) ){
			action_on_target(player, card, 0, ACT_BOUNCE);
		}
		if( instance->info_slot == 2 ){
			draw_cards(player, 1);
		}
	}

	return 0;
}

int card_two_headed_dragon(int player, int card, event_t event){
	/* Two-Headed Dragon	|4|R|R
	 * Creature - Dragon 4/4
	 * Flying
	 * Menace
	 * ~ can block an additional creature each combat.
	 * |1|R: ~ gets +2/+0 until end of turn. */

	menace(player, card, event);

	creature_can_block_additional(player, card, event, 1);

	return generic_shade(player, card, event, 0, MANACOST_XR(1, 1), 2, 0, 0, 0);
}

int card_undertaker(int player, int card, event_t event){
/* CARD_ID_UNDERTAKER	4641
Undertaker	|1|B
Creature - Human Spellshaper 1/1
|B, |T, Discard a card: Return target creature card from your graveyard to your hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	test_definition_t test;
	new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");

	if( event == EVENT_RESOLVE_ACTIVATION ){
		int selected = validate_target_from_grave_source(player, card, player, 0);
		if( selected != -1 ){
			from_grave_to_hand(player, selected, TUTOR_HAND);
		}
	}

	return generic_activated_ability_with_target_in_grave(player, card, event, GAA_UNTAPPED | GAA_DISCARD, MANACOST_B(1), 0, NULL, NULL,
															0, &test, GS_GRAVE_RECYCLER);
}

int card_unmask(int player, int card, event_t event){
/* CARD_ID_UNMASK	4642
Unmask	|3|B
Sorcery
You may exile a |Sblack card from your hand rather than pay ~'s mana cost.
Target player reveals his or her hand. You choose a nonland card from it. That player discards that card. */

	al_pitchspell(player, card, event, COLOR_BLACK, 0);

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td1;
	default_target_definition(player, card, &td1, TYPE_CREATURE);
	td1.zone = TARGET_ZONE_PLAYERS;

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_CAN_CAST ){
		return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);
	}

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
			if( casting_al_pitchspell(player, card, event, COLOR_BLACK, 0) ){
				return generic_spell(player, card, event, GS_CAN_TARGET, &td1, "TARGET_PLAYER", 1, NULL);
			}
			else{
			      spell_fizzled = 1;
		    }

	}

	if(event == EVENT_RESOLVE_SPELL ){
		if( valid_target(&td1) ){
			test_definition_t this_test;
			default_test_definition(&this_test, TYPE_LAND);
			this_test.type_flag = DOESNT_MATCH;

			ec_definition_t this_definition;
			default_ec_definition(instance->targets[0].player, player, &this_definition);
			new_effect_coercion(&this_definition, &this_test);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return 0;
}

int card_uphill_battle(int player, int card, event_t event){

	/* Uphill Battle	|2|R
	 * Enchantment
	 * Creatures played by your opponents enter the battlefield tapped. */

	permanents_enters_battlefield_tapped(player, card, event, 1-player, TYPE_CREATURE, NULL);

	return global_enchantment(player, card, event);
}

int card_vendetta(int player, int card, event_t event){
/* CARD_ID_VENDETTA	4645
Vendetta	|B
Instant
Destroy target non|Sblack creature. It can't be regenerated. You lose life equal to that creature's toughness. */

	if( ! IS_GS_EVENT(player, card, event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);
	td.illegal_color = get_sleighted_color_test(player, card, COLOR_TEST_BLACK);

	card_instance_t *instance = get_card_instance( player, card );

	if( event == EVENT_RESOLVE_SPELL ){
		if( validate_target(player, card, &td, 0) ){
			int amount = get_toughness(instance->targets[0].player, instance->targets[0].card);
			kill_card(instance->targets[0].player, instance->targets[0].card, KILL_BURY);
			lose_life(player, amount);
		}
		kill_card(player, card, KILL_DESTROY);
	}

	return generic_spell(player, card, event, GS_CAN_TARGET | GS_LITERAL_PROMPT, &td,
						get_sleighted_color_text(player, card, "Select target non%s creature.", COLOR_BLACK), 1, NULL);
}

int card_vine_dryad(int player, int card, event_t event){
/* CARD_ID_VINE_DRYAD	4648
Vine Dryad	|3|G
Creature - Dryad 1/3
Flash
|H2Forestwalk (This creature can't be blocked as long as defending player controls |Ha Forest.)
You may exile a |Sgreen card from your hand rather than pay ~'s mana cost. */

	al_pitchspell(player, card, event, COLOR_GREEN, 0);

	if(event == EVENT_CAST_SPELL && affect_me(player, card) ){
		if( ! casting_al_pitchspell(player, card, event, COLOR_GREEN, 0) ){
			spell_fizzled = 1;
		}
	}

	return flash(player, card, event);
}

static int war_tax_legacy(int player, int card, event_t event){
	if( event == EVENT_CLEANUP ){
		kill_card(player, card, KILL_EXILE);
	}
	return 0;
}

int card_war_tax(int player, int card, event_t event)
{
  /* CARD_ID_WAR_TAX	4652
   * War Tax	|2|U
   * Enchantment
   * |X|U: This turn, creatures can't attack unless their controller pays |X for each attacking creature he or she controls. */

  card_instance_t *instance;
  card_instance_t *tax_record;
  int result;
  card_instance_t *parent;

  tax_record = in_play(player, card);
  if (((tax_record != NULL) && (result = is_humiliated(player, card, AL_STATIC), result == 0)) &&
      (tax_record = get_card_instance(player, card), 0 < tax_record->targets[1].player))
  {
    tax_attack(player, card, event);
  }
  if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    instance->targets[1].player = 0;
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 1, 0, 0, 0, 0,
                                         NULL, NULL);
      }
      if (event == EVENT_ACTIVATE)
      {
        if ((current_turn == player) || (0x14 < current_phase))
        {
          ai_modifier = ai_modifier + -0x32;
        }
        else
        {
          ai_modifier = ai_modifier + 0xf;
        }
        result = charge_mana_for_activated_ability(player, card, 0, 0, 1, 0, 0, 0);
        if (result == 0)
        {
          return 0;
        }
        charge_mana(player, COLOR_COLORLESS, -1);
        if (spell_fizzled == 1)
        {
          return 0;
        }
        instance->info_slot = x_value;
        return 0;
      }
      return 0;
    }
    if (event != EVENT_RESOLVE_ACTIVATION)
    {
      return global_enchantment(player, card, event);
    }
  }
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }
  parent = get_card_instance(instance->parent_controller, instance->parent_card);
  parent->targets[1].player = instance->info_slot;
  create_targetted_legacy_effect(instance->parent_controller, instance->parent_card, war_tax_legacy,
                                 instance->parent_controller, instance->parent_card);
  return 0;
}

int card_waterfront_bouncer(int player, int card, event_t event){
/* CARD_ID_WATERFRONT_BOUNCER	4655
Waterfront Bouncer	|1|U
Creature - Merfolk Spellshaper 1/1
|U, |T, Discard a card: Return target creature to its owner's hand. */

	if( ! IS_GAA_EVENT(event) ){
		return 0;
	}

	target_definition_t td;
	default_target_definition(player, card, &td, TYPE_CREATURE);

	card_instance_t *instance = get_card_instance(player, card);

	if( event == EVENT_RESOLVE_ACTIVATION ){
		if( valid_target(&td) ){
			bounce_permanent(instance->targets[0].player, instance->targets[0].card);
		}
	}

	return generic_activated_ability(player, card, event, GAA_UNTAPPED|GAA_CAN_TARGET|GAA_DISCARD, MANACOST_U(1), 0, &td, "TARGET_CREATURE");
}

int card_wave_of_reckoning(int player, int card, event_t event){
  /* Wave of Reckoning |4|W
   * Sorcery
   * Each creature deals damage to itself equal to its power. */

	if( event == EVENT_RESOLVE_SPELL ){
		APNAP(p,{
					int c;
					for(c = active_cards_count[p]-1; c>-1; c--){
						if( in_play(p, c) && is_what(p, c, TYPE_CREATURE) ){
							damage_creature(p, c, get_power(p, c), p, c);
						}
					}
				}
		);
		kill_card(player, card, KILL_DESTROY);
	}

	return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x217cd40; original cards/mercadian_masques.c:4976. */
int card_cowardice(int player, int card, event_t event)
{
  /* Cowardice	|3|U|U
   * Enchantment
   * Whenever a creature becomes the target of a spell or ability, return that creature to its owner's hand. (It won't be affected by the spell or ability.) */

  const int32_t *target_card;
  const target_t *target_cursor;
  int result;

  target_cursor = any_creature_becomes_target_of_spell_or_effect(player, card, event, 2);
  if (target_cursor != NULL)
  {
    result = target_cursor->player;
    while (result != -1)
    {
      target_card = &target_cursor->card;
      target_cursor = target_cursor + 1;
      bounce_permanent(result, *target_card);
      result = target_cursor->player;
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x217cdc0; original cards/mercadian_masques.c:4996. */
int card_aerial_caravan(int player, int card, event_t event)
{
  /* Aerial Caravan	|4|U|U
   * Creature - Human Soldier 4/3
   * Flying
   * |1|U|U: Exile the top card of your library. Until end of turn, you may play that card. (Reveal the card as you exile it.) */

  int result;
  if (!IS_GAA_EVENT(event))
    return 0;
  if ((event == EVENT_RESOLVE_ACTIVATION) && (*deck_ptr[player] != -1))
  {
    reveal_card_iid(player, card, *deck_ptr[player]);
    create_may_play_card_from_exile_effect(
        player, card, player, (unsigned int)cards_data[*deck_ptr[player]].id, MPCFE_UNTIL_EOT);
    rfg_top_card_of_deck(player);
  }
  result = generic_activated_ability(player, card, event, 0, 1, 0, 2, 0, 0, 0, 0, NULL, NULL);
  return result;
}

/* ManalinkEh 2022: 0x217cec0; original cards/mercadian_masques.c:5018. */
int card_assembly_hall(int player, int card, event_t event)
{
  /* Assembly Hall	|5
   * Artifact
   * |4, |T: Reveal a creature card in your hand. Search your library for a card with the same name as that card, reveal it, put it into your hand, then shuffle. */

  int selected_card;
  int csvid;
  char buffer[100];
  test_definition_t test;
  test_definition_t test2;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    new_default_test_definition(&test, 2, "Select a creature card.");
    test.zone = 0x100;
    if (check_battlefield_for_special_card(player, card, player, 0, &test) != 0)
    {
      selected_card = new_select_a_card(player, player, 4, 1, 1, -1, &test);
      csvid = get_id(player, selected_card);
      if (selected_card != -1)
      {
        reveal_card(player, card, player, selected_card);
        scnprintf(buffer, 100, "Select a creature card named %s", cards_ptr[csvid]->name);
        new_default_test_definition(&test2, 0xff, buffer);
        test2.id = csvid;
        new_global_tutor(player, player, 1, 1, 0, 5, &test2);
      }
    }
  }
  return generic_activated_ability(player, card, event, 1, 4, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x217d090; original cards/mercadian_masques.c:5047. */
int card_ballista_squad(int player, int card, event_t event)
{
  /* Ballista Squad	|3|W
   * Creature - Human Rebel 2/2
   * |X|W, |T: This creature deals X damage to target attacking or blocking creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_IN_COMBAT;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    td.required_state = TARGET_STATE_IN_COMBAT;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      damage_target0(player, card, instance->info_slot);
    }
  }
  result = generic_activated_ability(player, card, event, 7, -1, 0, 0, 0, 0, 1, 0, &td,
                                     "Select target attacking or blocking creature.");
  return result;
}

/* ManalinkEh 2022: 0x217d1e0; original cards/mercadian_masques.c:5072. */
int card_balloon_peddler(int player, int card, event_t event)
{
  /* Balloon Peddler	|2|U
   * Creature - Human Spellshaper 2/2
   * |U, |T, Discard a card: Target creature gains flying until end of turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                           instance->targets[0].player, instance->targets[0].card, 0, 0, 0x20, 0);
  }
  result = generic_activated_ability(player, card, event, 0x25, 0, 0, 1, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  return result;
}

/* ManalinkEh 2022: 0x216b970; original cards/mercadian_masques.c:5097. */
static char *damage_must_be_from_barbed_wire(int who_chooses, int player, int card,
                                             int targeting_player, int targeting_card)
{
  card_instance_t *instance;
  char *requirement_error;

  instance = get_card_instance(player, card);
  requirement_error = "Must be damage made by Barbed Wire.";
  if (((instance->internal_card_id == damage_card) &&
       (instance->damage_source_player == targeting_player)) &&
      (instance->damage_source_card == targeting_card))
  {
    requirement_error = NULL;
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x217d350; original cards/mercadian_masques.c:5108. */
int card_barbed_wire(int player, int card, event_t event)
{
  /* Barbed Wire	|3
   * Artifact
   * At the beginning of each player's upkeep, this artifact deals 1 damage to that player.
   * |2: Prevent the next 1 damage that would be dealt by this artifact this turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_CAST_SPELL)
  {
    if (((card == affected_card) && (player == affected_card_controller)) &&
        ((ai_is_speculating == 1 || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))))
    {
      ai_modifier = ai_modifier + (life[player] - life[1 - player]) * 8;
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 0);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)damage_must_be_from_barbed_wire;
    instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      result = valid_target(&td);
      if (result != 0)
      {
        prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
      }
    }
    result = generic_activated_ability(player, card, event, 0x1000006, 2, 0, 0, 0, 0, 0, 0, &td,
                                       "Select a damage card from damage dealt by Barbed Wire.");
    return result;
  }
  result = card_copper_tablet(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x217d4f0; original cards/mercadian_masques.c:5140. */
int card_blaster_mage(int player, int card, event_t event)
{
  /* Blaster Mage	|2|R
   * Creature - Human Spellshaper 2/2
   * |R, |T, Discard a card: Destroy target Wall. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x47);
    td.required_subtype = 0xd1;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x47);
    td.required_subtype = 0xd1;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
  }
  result =
      generic_activated_ability(player, card, event, 0x25, 0, 0, 0, 0, 1, 0, 0, &td, "TARGET_WALL");
  return result;
}

/* ManalinkEh 2022: 0x217d640; original cards/mercadian_masques.c:5165. */
int card_blockade_runner(int player, int card, event_t event)
{
  /* Blockade Runner	|3|U
   * Creature - Merfolk 2/2
   * |U: This creature can't be blocked this turn. */

  int result;
  result = generic_shade(player, card, event, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0x10);
  return result;
}

/* ManalinkEh 2022: 0x217d6c0; original cards/mercadian_masques.c:5174. */
int card_blood_hound(int player, int card, event_t event)
{
  /* Blood Hound	|2|R
   * Creature - Dog 1/1
   * Whenever you're dealt damage, you may put that many +1/+1 counters on this creature.
   * At the beginning of your end step, remove all +1/+1 counters from this creature. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_DEAL_DAMAGE)
  {
    check_damage(2, -1, EVENT_DEAL_DAMAGE,
                 DDBM_MUST_DAMAGE_TRIGGERING_PLAYER | DDBM_REPORT_DAMAGE_DEALT, player, card);
  }
  result = resolve_damage_trigger(player, card, event,
                                  DDBM_MUST_DAMAGE_TRIGGERING_PLAYER | DDBM_REPORT_DAMAGE_DEALT |
                                      DDBM_TRIGGER_OPTIONAL,
                                  player, card);
  if (result != 0)
  {

    instance = get_card_instance(player, card);
    result = instance->targets[0x10].player;
    instance->targets[0x10].player = 0;
    add_counters(player, card, player, card, COUNTER_P1_P1, result);
  }
  if ((current_turn == player) && (result = eot_trigger(player, card, event), result != 0))
  {
    remove_all_counters(player, card, player, card, COUNTER_P1_P1);
  }
  return 0;
}

/* ManalinkEh 2022: 0x217d7d0; original cards/mercadian_masques.c:5200. */
int card_boa_constrictor(int player, int card, event_t event)
{
  /* Boa Constrictor	|4|G
   * Creature - Snake 3/3
   * |T: This creature gets +3/+3 until end of turn. */

  int result;
  result = generic_shade_tap(player, card, event, 0, 0, 0, 0, 0, 0, 3, 3);
  return result;
}

/* ManalinkEh 2022: 0x217d840; original cards/mercadian_masques.c:5214. */
int card_caller_of_the_hunt(int player, int card, event_t event)
{
  /* Caller of the Hunt	|2|G
   * Creature - Human * / *
   * As an additional cost to cast this spell, choose a creature type.
   * ~'s power and toughness are each equal to the number of creatures of the chosen type on the battlefield. */

  card_instance_t *instance;
  int result;

  if (card != -1)
  {
    if (event == EVENT_RESOLVE_SPELL)
    {
      instance = get_card_instance(player, card);
      result = select_a_subtype(player, card);
      instance->targets[1].player = result;
      get_card_instance(player, card);
      return 0;
    }
    instance = get_card_instance(player, card);
    if ((((0 < instance->targets[1].player) && ((uint32_t)(event - EVENT_POWER) < 2)) &&
         (card == affected_card)) &&
        (player == affected_card_controller))
    {
      instance = get_card_instance(player, card);
      result = count_subtype(2, 2, instance->targets[1].player);
      set_characteristic_pt(result);
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x217d900; original cards/mercadian_masques.c:5237. */
int card_cave_sense(int player, int card, event_t event)
{
  /* Cave Sense	|1|R
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature gets +1/+1 and has mountainwalk. (It can't be blocked as long as defending player controls a Mountain.) */

  keyword_t keyword;
  int result;

  attached_gets_pt(player, card, event, 1, 1);
  keyword = get_hacked_walk(player, card, 8);
  attached_gets_kw(player, card, event, keyword, SP_KEYWORD_0);
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x217d990; original cards/mercadian_masques.c:5250. */
int card_ceremonial_guard(int player, int card, event_t event)
{
  /* Ceremonial Guard	|2|R
   * Creature - Human Soldier 3/4
   * When this creature attacks or blocks, destroy it at end of combat. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY &&
      (!blocking(player, card, event) || is_humiliated(player, card, AL_TRIG)))
    return 0;
  create_targetted_legacy_effect(player, card, die_at_end_of_combat, player, card);
  return 0;
}

/* ManalinkEh 2022: 0x217da30; original cards/mercadian_masques.c:5263. */
int card_chambered_nautilus(int player, int card, event_t event)
{
  /* Chambered Nautilus	|2|U
   * Creature - Nautilus Beast 2/2
   * Whenever this creature becomes blocked, you may draw a card. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((event == EVENT_DECLARE_BLOCKERS) && (result == 0))
    {
      result = is_attacking(player, card);
      if (result != 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          result = count_deck(player);
          result = do_dialog(player, player, card, -1, -1, " Draw a card\n Pass",
                             (unsigned int)(10 < result));
          if (result == 0)
          {
            draw_cards(player, 1);
          }
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x217daf0; original cards/mercadian_masques.c:5280. */
int card_charm_peddler(int player, int card, event_t event)
{
  /* Charm Peddler	|W
   * Creature - Human Spellshaper 1/1
   * |W, |T, Discard a card: The next time a source of your choice would deal damage to target creature this turn, prevent that damage. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td, 0x80);
    td.extra = damage_card;
    td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0x80);
    td.extra = damage_card;
    td.special = TARGET_SPECIAL_DAMAGE_CREATURE;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
    {
      prevent_damage(instance->targets[0].player, instance->targets[0].card, -1);
    }
  }
  result = generic_activated_ability(player, card, event, 0x1000025, 0, 0, 0, 0, 0, 1, 0, &td,
                                     "TARGET_DAMAGE");
  return result;
}

/* ManalinkEh 2022: 0x217dc50; original cards/mercadian_masques.c:5307. */
int card_charmed_griffin(int player, int card, event_t event)
{
  /* Charmed Griffin	|3|W
   * Creature - Griffin 3/3
   * Flying
   * When this creature enters, each other player may put an artifact or enchantment card onto the battlefield from their hand. */

  int result;
  int t_player;
  test_definition_t this_test;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(
        &this_test, 0x44, "Select an Artifact or Enchantment card to put onto the battlefield.");
    t_player = 1 - player;
    this_test.zone = 0x100;
    result = check_battlefield_for_special_card(player, card, t_player, 0, &this_test);
    if (result != 0)
    {
      new_global_tutor(t_player, t_player, 4, 4, 0, 2, &this_test);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x217dd30; original cards/mercadian_masques.c:5328. */
int card_cho_arrim_legate(int player, int card, event_t event)
{
  /* Cho-Arrim Legate	|2|W
   * Creature - Human Soldier 1/2
   * If an opponent controls a Swamp and you control a Plains, you may cast this spell without paying its mana cost.
   * Protection from black */

  int result;
  card_instance_t *instance;

  if (event != EVENT_MODIFY_COST)
  {
    if ((((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (result = nemesis_freespell(player, card, 0x6c), result == 0))
    {
      spell_fizzled = 1;
    }
    return 0;
  }
  result = get_hacked_subtype(player, card, 0x5009);
  result = check_battlefield_for_subtype(1 - player, 1, result);
  if (result != 0)
  {
    result = get_hacked_subtype(player, card, 0x5007);
    result = check_battlefield_for_subtype(player, 1, result);
    if (result != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  instance->info_slot = 0;
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x217de50; original cards/mercadian_masques.c:5356. */
int card_cho_mannos_blessing(int player, int card, event_t event)
{
  /* Cho-Manno's Blessing	|W|W
   * Enchantment - Aura
   * Flash
   * Enchant creature
   * As this Aura enters, choose a color.
   * Enchanted creature has protection from the chosen color. This effect doesn't remove this Aura. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_RESOLVE_SPELL)
  {
    instance = get_card_instance(player, card);
    result = select_a_protection(player);
    instance->targets[1].card = result;
    result = this_effect_doesnt_remove_this(player, card, EVENT_RESOLVE_SPELL);
    if (result != 0)
    {
      return 0;
    }
  }
  else
  {
    result = this_effect_doesnt_remove_this(player, card, event);
    if (result != 0)
    {
      return 0;
    }

    if ((((event == EVENT_ABILITIES) && (instance = in_play(player, card), instance != NULL)) &&
         (-1 < instance->damage_target_card)) &&
        (((result = is_humiliated(player, card, AL_STATIC),
           result == 0 && (instance->damage_target_card == affected_card)) &&
          (instance->damage_target_player == affected_card_controller))))
    {
      event_result = event_result | instance->targets[1].card;
    }
  }
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

#ifndef SHANDALAR
static int each_player_reveal_top_library_type_cards_put_in_play_exile_rest(int player, int card,
                                                                            int type, int cards_to_look)
{
  for (int phase = 0; phase < 2; ++phase)
  {
    int p = phase ^ current_turn;
    if (deck_ptr[p][0] == -1)
      continue;
    show_deck(p, deck_ptr[p], cards_to_look, "Here are the revealed cards from controller's deck.", 0, gs_done_008b40e0);
    show_deck(1 - p, deck_ptr[p], cards_to_look, "Here are the revealed cards from controller's opponent's deck.", 0, gs_done_008b40e0);
    int count = count_deck(p);
    int amount = MIN(cards_to_look, count);
    if (count < 1)
      break;
    int *deck = deck_ptr[p];
    for (int pos = amount - 1; pos >= 0; --pos)
      if (is_what(-1, deck[pos], type))
      {
        int added = add_card_to_hand(p, deck[pos]);
        put_into_play(p, added);
        add_state(p, added, STATE_TAPPED);
        remove_card_from_deck(p, pos);
        --amount;
      }
    if (amount > 0)
      rfg_top_n_cards_of_deck(p, amount);
  }
  return 0;
}
#endif

/* ManalinkEh 2022: 0x217df70; original cards/mercadian_masques.c:5418. */
int card_clear_the_land(int player, int card, event_t event)
{
  /* Clear the Land	|2|G
   * Sorcery
   * Each player reveals the top five cards of their library, puts all land cards revealed this way onto the battlefield tapped, and exiles the rest. */

  int *library;
  int library_size_or_index;
  int land_card;
  unsigned int player_00;
  int remaining_top_cards;
  unsigned int pass;

  if (event == EVENT_RESOLVE_SPELL)
  {

    pass = 0;

    do
    {
      player_00 = pass ^ current_turn;
      if (*deck_ptr[player_00] != -1)
      {
        show_deck(player_00, deck_ptr[player_00], 5,
                  "Here are the revealed cards from controller\'s deck.", 0, gs_done_008b40e0);
        show_deck(1 - player_00, deck_ptr[player_00], 5,
                  "Here are the revealed cards from controller\'s opponent\'s deck.", 0,
                  gs_done_008b40e0);
        library_size_or_index = count_deck(player_00);

        remaining_top_cards = library_size_or_index;
        if (5 < library_size_or_index)
        {
          remaining_top_cards = 5;
        }
        if (library_size_or_index < 1)
          break;
        library = deck_ptr[player_00];
        library_size_or_index = remaining_top_cards + -1;
        if (-1 < library_size_or_index)
        {
          do
          {
            if (is_what(-1, library[library_size_or_index], 1) != 0)
            {

              land_card = add_card_to_hand(player_00, library[library_size_or_index]);
              put_into_play(player_00, land_card);
              add_state(player_00, land_card, 0x10);
              remove_card_from_deck(player_00, library_size_or_index);
              remaining_top_cards = remaining_top_cards + -1;
            }
            library_size_or_index = library_size_or_index + -1;
          } while (library_size_or_index != -1);
        }
        if (0 < remaining_top_cards)
        {
          rfg_top_n_cards_of_deck(player_00, remaining_top_cards);
        }
      }
      pass = pass + 1;
    } while (pass != 2);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x217e120; original cards/mercadian_masques.c:5432. */
int card_close_quarters(int player, int card, event_t event)
{
  /* Close Quarters	|2|R|R
   * Enchantment
   * Whenever a creature you control becomes blocked, this enchantment deals 1 damage to any target. */

  int c;
  int humiliated;
  card_instance_t *instance;
  card_instance_t *attacker;
  int eligible_or_available;
  target_definition_t td;

  if (((event == EVENT_DECLARE_BLOCKERS) && (current_turn == player)) &&
      (humiliated = is_humiliated(player, card, AL_TRIG), humiliated == 0))
  {

    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    td.allow_cancel = 0;
    instance = get_card_instance(player, card);
    c = 0;
    if (0 < active_cards_count[player])
    {
      do
      {
        attacker = in_play(player, c);
        if (((attacker != NULL) && ((attacker->state & 4) != 0)) &&
            ((eligible_or_available = is_what(player, c, 2),
              eligible_or_available != 0 &&
                  (eligible_or_available = is_unblocked(player, c), eligible_or_available == 0))))
        {
          eligible_or_available = can_target(&td);
          if ((eligible_or_available != 0) &&
              (eligible_or_available = pick_target(&td, "TARGET_ANY"), eligible_or_available != 0))
          {
            damage_target0(player, card, 1);
          }
          instance->number_of_targets = '\0';
        }
        c = c + 1;
      } while (c < active_cards_count[player]);
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x217e260; original cards/mercadian_masques.c:5460. */
int card_conspiracy(int player, int card, event_t event)
{
  /* Conspiracy	|3|B|B
   * Enchantment
   * As this enchantment enters, choose a creature type.
   * Creatures you control are the chosen type. The same is true for creature spells you control and creature cards you own that aren't on the battlefield. */

  uint32_t data;
  card_instance_t *instance;
  inst_info_t chosen_subtype;

  if (event == EVENT_RESOLVE_SPELL)
  {
    data = select_a_subtype(player, card);
    instance = get_card_instance(player, card);
    set_inst_info(instance, 0x1153, data, 0);
  }
  else if (event == EVENT_CHANGE_TYPE)
  {
    instance = in_play(player, card);
    if ((instance != NULL) && (affected_card_controller == player))
    {
      instance = in_play(player, affected_card);
      if (instance != NULL)
      {
        if (is_what(affected_card_controller, affected_card, 2) != 0)
        {
          if (is_humiliated(player, card, AL_STATIC) == 0)
          {

            instance = get_card_instance(player, card);
            chosen_subtype = inst_info(instance, 0x1153);
            force_a_subtype_not_in_play(player, EVENT_CHANGE_TYPE, chosen_subtype.data);
            chosen_subtype = inst_info(instance, 0x1153);
            force_a_subtype(affected_card_controller, affected_card, chosen_subtype.data);
          }
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x217e3b0; original cards/mercadian_masques.c:5484. */
int card_corrupt_official(int player, int card, event_t event)
{
  /* Corrupt Official	|4|B
   * Creature - Human Minion 3/1
   * |2|B: Regenerate this creature.
   * Whenever this creature becomes blocked, defending player discards a card at random. */

  int result;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_unblocked(player, card);
      if (result == 0)
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          discard(1 - player, 1, player);
        }
      }
    }
  }
  result = regeneration(player, card, event, 2, 1, 0, 0, 0, 0);
  return result;
}

/* ManalinkEh 2022: 0x217e470; original cards/mercadian_masques.c:5498. */
int card_crag_saurian(int player, int card, event_t event)
{
  /* Crag Saurian	|R|R|R
   * Creature - Lizard 4/4
   * Whenever a source deals damage to this creature, that source's controller gains control of this creature. */

  card_instance_t *instance;
  int previous_damage;
  int damage_count;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (is_humiliated(player, card, AL_TRIG) == 0)
    {

      instance = damage_being_dealt(event);
      if ((((instance != NULL) && (instance->damage_target_card == card)) &&
           (instance->damage_target_player == player)) &&
          ((0 < instance->info_slot && ((int)instance->damage_source_player == 1 - player))))
      {

        instance = get_card_instance(player, card);
        previous_damage = instance->targets[8].player;
        damage_count = 0;
        if (0 < previous_damage)
        {
          damage_count = previous_damage;
        }
        instance->targets[8].player = damage_count + 1;
      }
    }
  }
  if (resolve_damage_trigger(player, card, event, DDBM_0, player, card) == 0)
  {
    return 0;
  }
  give_control_of_self(player, card);
  return 0;
}

/* ManalinkEh 2022: 0x217e550; original cards/mercadian_masques.c:5524. */
int card_crash(int player, int card, event_t event)
{
  /* Crash	|2|R
   * Instant
   * You may sacrifice a Mountain rather than pay this spell's mana cost.
   * Destroy target artifact. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_MODIFY_COST)
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x5006;
    result = new_can_sacrifice_as_cost(player, card, &test);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      null_casting_cost(player, card);
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 0x40);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
    {
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
      default_target_definition(player, card, &td, 0x40);
      get_card_instance(player, card);
      return 0;
    }
    default_target_definition(player, card, &td, 0x40);
    instance = get_card_instance(player, card);
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }
    result = valid_target(&td);
    if (result != 0)
    {
      kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
    }
    kill_card(player, card, KILL_DESTROY);
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
  default_target_definition(player, card, &td, 0x40);
  instance = get_card_instance(player, card);
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  result = played_for_free(player, card);
  if (((result == 0) && (result = is_token(player, card), result == 0)) &&
      (instance->info_slot == 1))
  {

    result = has_mana_to_cast_iid(player, EVENT_CAST_SPELL, instance->internal_card_id);
    if (result != 0)
    {
      result = count_subtype(player, 1, -1);
      result =
          dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                      "Sacrifice a mountain", 1, result * 2, "Cast normally", 1, 5, 0, 0, 0, 0);
      if (result != 1)
      {
        if (result == 2)
        {
          result = get_id(player, card);
          charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
        }
        else
        {
          spell_fizzled = 1;
        }
        pick_target(&td, "TARGET_ARTIFACT");
        return 0;
      }
    }
    new_default_test_definition(&test, 0x47, "Select a Mountain to sacrifice.");
    test.subtype = 0x5006;
    new_sacrifice(player, card, player, SAC_AS_COST, &test);
  }
  pick_target(&td, "TARGET_ARTIFACT");
  return 0;
}

/* ManalinkEh 2022: 0x217e950; original cards/mercadian_masques.c:5593. */
int card_crenellated_wall(int player, int card, event_t event)
{
  /* Crenellated Wall	|4
   * Artifact Creature - Wall 0/4
   * Defender (This creature can't attack.)
   * |T: Target creature gets +0/+4 until end of turn. */

  if (event == EVENT_CHECK_PUMP)
  {
    if (card_crenellated_wall(player, card, EVENT_CAN_ACTIVATE))
      pumpable_toughness[player] += 4;
  }
  else if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, TYPE_CREATURE);
    td.preferred_controller = player;
    card_instance_t *instance = get_card_instance(player, card);
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
      pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 0, 4);
    return generic_activated_ability(player, card, event, 5, 0, 0, 0, 0, 0, 0, 0, &td,
                                       "TARGET_CREATURE");
  }
  return 0;
}

/* ManalinkEh 2022: 0x216ee00; original cards/mercadian_masques.c:5625. */
static int target_lands_become_forests(int player, int card, event_t event)
{
  int basic_land_type_to_become;
  card_instance_t *instance;
  if (event == EVENT_CHANGE_TYPE)
  {
    instance = get_card_instance(player, card);
    if ((instance->damage_target_card == affected_card) &&
        (instance->damage_target_player == affected_card_controller))
    {
      basic_land_type_to_become = get_hacked_subtype(player, card, 0x5001);
      event_result = land_into_new_basic_land_type(event_result, basic_land_type_to_become);
    }
  }
  else if (event == EVENT_CLEANUP)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->targets[0].player)
    {
      kill_card(player, card, KILL_EXILE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x217eab0; original cards/mercadian_masques.c:5643. */
int card_deepwood_elder(int player, int card, event_t event)
{
  /* Deepwood Elder	|G|G
   * Creature - Dryad Spellshaper 2/2
   * |X|G|G, |T, Discard a card: X target lands become Forests until end of turn. */

  card_instance_t *instance;
  int target_or_mana_available;
  card_instance_t *land_effect;
  int i;
  target_t *target_cursor;
  target_t *next_target_cursor;
  target_definition_t td;

  if (((event == EVENT_ACTIVATE) || ((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2)) ||
      (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 1);
    if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
    {
      td.required_subtype = get_hacked_subtype(player, card, 0x5001);
      td.special = TARGET_SPECIAL_ILLEGAL_SUBTYPE;
      td.preferred_controller = 1 - player;
    }
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 1, 0, 0, 2, 0, 0, 0,
                                       NULL, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      i = 0;
      while (target_or_mana_available = can_target(&td), target_or_mana_available != 0)
      {
        target_or_mana_available =
            has_mana_for_activated_ability(player, card, i + 1, 0, 0, 2, 0, 0);
        if (((target_or_mana_available == 0) || (hand_count[player] < 1)) ||
            (target_or_mana_available = new_pick_target(&td, "TARGET_LAND", i, 0),
             target_or_mana_available == 0))
          break;
        state_untargettable(instance->targets[i].player, instance->targets[i].card, 1);
        i = i + 1;
      }
      if (0 < i)
      {
        target_cursor = instance->targets;
        do
        {
          next_target_cursor = target_cursor + 1;
          state_untargettable(target_cursor->player, target_cursor->card, 0);
          target_cursor = next_target_cursor;
        } while (instance->targets + i != next_target_cursor);
        generic_activated_ability(player, card, EVENT_ACTIVATE, 0x21, i, 0, 0, 2, 0, 0, 0, NULL,
                                  NULL);
        if ((spell_fizzled != 1) && (1 - player == current_turn))
        {
          ai_modifier = ai_modifier + 0x60;
        }
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) && (instance->number_of_targets != '\0'))
    {
      i = 0;
      do
      {
        if (validate_target(player, card, &td, i) != 0)
        {
          land_effect =
              get_card_instance(player, create_targetted_legacy_effect(instance->parent_controller,
                                                                       instance->parent_card,
                                                                       target_lands_become_forests,
                                                                       instance->targets[i].player,
                                                                       instance->targets[i].card));
          land_effect->targets[0].player = instance->parent_controller;
          land_effect->targets[0].card = instance->parent_card;
          land_effect->number_of_targets = '\x01';
        }
        i = i + 1;
      } while (i < (int)(unsigned int)instance->number_of_targets);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x217ee30; original cards/mercadian_masques.c:5710. */
int card_deepwood_legate(int player, int card, event_t event)
{
  /* Deepwood Legate	|3|B
   * Creature - Shade 1/1
   * If an opponent controls a Forest and you control a Swamp, you may cast this spell without paying its mana cost.
   * |B: This creature gets +1/+1 until end of turn. */

  card_instance_t *instance;
  int result;

  if (event == EVENT_MODIFY_COST)
  {
    result = get_hacked_subtype(player, card, 0x5001);
    result = check_battlefield_for_subtype(1 - player, 1, result);
    if (result != 0)
    {
      result = get_hacked_subtype(player, card, 0x5009);
      result = check_battlefield_for_subtype(player, 1, result);
      if (result != 0)
      {
        null_casting_cost(player, card);
        instance = get_card_instance(player, card);
        instance->info_slot = 1;
        return card_nantuko_shade(player, card, event);
      }
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
  }
  else if (((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
           (player == affected_card_controller))
  {
    result = nemesis_freespell(player, card, 0x6c);
    if (result == 0)
    {
      spell_fizzled = 1;
    }
  }
  return card_nantuko_shade(player, card, event);
}

/* ManalinkEh 2022: 0x217ef40; original cards/mercadian_masques.c:5738. */
int card_deepwood_tantiv(int player, int card, event_t event)
{
  /* Deepwood Tantiv	|4|G
   * Creature - Beast 2/4
   * Whenever this creature becomes blocked, you gain 2 life. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if ((event == EVENT_DECLARE_BLOCKERS) && (result == 0))
    {
      result = is_attacking(player, card);
      if (result != 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          gain_life(player, 2);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x217efc0; original cards/mercadian_masques.c:5752. */
int card_deepwood_wolverine(int player, int card, event_t event)
{
  /* Deepwood Wolverine	|G
   * Creature - Wolverine 1/1
   * Whenever this creature becomes blocked, it gets +2/+0 until end of turn. */

  int result;
  card_instance_t *instance;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_humiliated(player, card, AL_TRIG);
    if (result == 0)
    {

      instance = get_card_instance(player, card);
      if ((instance->state & 4) != 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          pump_until_eot_merge_previous(player, card, player, card, 2, 0);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216ba40; original cards/mercadian_masques.c:5767. */
static char *targets_a_creature(int who_chooses, int player, int card)
{
  card_instance_t *instance;
  int i;

  instance = get_card_instance(player, card);
  if (instance->number_of_targets != '\0')
  {
    i = 0;
    do
    {
      if (instance->targets[0].player == who_chooses)
      {
        if (is_what(who_chooses, instance->targets[0].card, 2) != 0)
        {
          return NULL;
        }
      }
      i = i + 1;
    } while (i < (int)(unsigned int)instance->number_of_targets);
  }
  return "Must target a creature.";
}

/* ManalinkEh 2022: 0x217f060; original cards/mercadian_masques.c:5779. */
int card_diplomatic_escort(int player, int card, event_t event)
{
  /* Diplomatic Escort	|1|U
   * Creature - Human Spellshaper 1/1
   * |U, |T, Discard a card: Counter target spell or ability that targets a creature. */

  unsigned int activation_flags;
  card_instance_t *instance;
  int result;
  target_definition_t td_effect;
  target_definition_t td_spell;
  target_definition_t td_trig;

  if (((event == EVENT_ACTIVATE) || ((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2)) ||
      (event == EVENT_CLEANUP))
  {
    counter_activated_target_definition(player, card, &td_effect, 0);
    td_effect.special = td_effect.special | 0x8000;
    td_effect.extra = (intptr_t)targets_a_creature;
    counterspell_target_definition(player, card, &td_spell, 0);
    td_spell.special = td_spell.special | 0x8000;
    td_spell.extra = (intptr_t)targets_a_creature;
    counter_triggered_target_definition(player, card, &td_trig, 0);
    td_trig.special = td_trig.special | 0x8000;
    td_trig.extra = (intptr_t)targets_a_creature;
    instance = get_card_instance(player, card);
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x21, 0, 0, 1, 0, 0, 0,
                                         0, NULL, NULL);
      if (result != 0)
      {
        result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x400, 0, 0, 0, 0, 0,
                                           0, 0, &td_spell, NULL);
        if (result != 0)
        {
          instance->info_slot = 1;
          return 99;
        }
        result = can_counter_activated_ability(player, card, EVENT_CAN_ACTIVATE, &td_effect);
        if (result == 0)
        {
          result = can_counter_triggered_ability(player, card, EVENT_CAN_ACTIVATE, &td_trig);
          if (result == 0)
          {
            instance->info_slot = 0;
            return 0;
          }
          instance->info_slot = 3;
          return 99;
        }
        instance->info_slot = 2;
        return 99;
      }
    }
    else if (event == EVENT_ACTIVATE)
    {
      instance->number_of_targets = '\0';
      activation_flags = instance->info_slot;
      if ((activation_flags & 1) != 0)
      {
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0x421, 0, 0, 1, 0, 0, 0, 0,
                                         &td_spell, NULL);
      }
      if ((activation_flags & 2) == 0)
      {
        if (((activation_flags & 3) != 0) &&
            (result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x21, 0, 0, 1, 0, 0,
                                                0, 0, NULL, NULL),
             result != 0))
        {
          cast_counter_triggered_ability(player, card, 0);
        }
      }
      else
      {
        result = generic_activated_ability(player, card, EVENT_ACTIVATE, 0x21, 0, 0, 1, 0, 0, 0, 0,
                                           NULL, NULL);
        if (result != 0)
        {
          cast_counter_activated_ability(player, card, 0);
        }
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      activation_flags = instance->info_slot;
      if ((activation_flags & 1) == 0)
      {
        if ((activation_flags & 2) == 0)
        {
          if ((activation_flags & 3) != 0)
          {
            resolve_counter_triggered_ability(player, card, &td_trig, 0);
          }
        }
        else
        {
          resolve_counter_activated_ability(player, card, &td_effect, 0);
        }
      }
      else
      {
        result = counterspell_validate(player, card, &td_spell, 0);
        if (result != 0)
        {
          real_counter_a_spell(instance->parent_controller, instance->parent_card,
                               instance->targets[0].player, instance->targets[0].card);
        }
      }
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x217f560; original cards/mercadian_masques.c:5861. */
int card_erithizon(int player, int card, event_t event)
{
  /* Erithizon	|2|G|G
   * Creature - Beast 4/4
   * Whenever this creature attacks, put a +1/+1 counter on target creature of defending player's choice. */

  card_instance_t *instance;
  int result;
  target_definition_t td1;

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  default_target_definition(player, card, &td1, 2);
  td1.who_chooses = 1 - current_turn;
  td1.allow_cancel = 0;
  td1.preferred_controller = td1.who_chooses;
  result = can_target(&td1);
  if (result != 0)
  {
    result =
        pick_next_target_noload(&td1, "Select target creature card, put a +1/+1 counter on it.");
    if (result != 0)
    {
      add_counters(player, card, instance->targets[0].player, instance->targets[0].card,
                               COUNTER_P1_P1, 1);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x217f660; original cards/mercadian_masques.c:5886. */
int card_extortion(int player, int card, event_t event)
{
  /* Extortion	|3|B|B
   * Sorcery
   * Look at target player's hand and choose up to two cards from it. That player discards those cards. */

  card_instance_t *instance;
  int result;
  int discard_limit;
  int discarded_count;
  target_definition_t td;
  test_definition_t this_test;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = valid_target(&td);
    if (result != 0)
    {

      discard_limit = do_dialog(player, player, card, -1, -1,
                                " choose 0 cards\n choose 1 card\n choose 2 cards\n Cancel", 2);
      if (2 < discard_limit)
      {
        discard_limit = 0;
      }
      new_default_test_definition(&this_test, 0xff, "Select a card to be discarded.");
      discarded_count = 0;
      while (discarded_count < discard_limit)
      {

        result = new_select_a_card(player, instance->targets[0].player, 4, 0, 1, -1, &this_test);
        if (result != -1)
        {
          new_discard_card(instance->targets[0].player, result, player, 0);
          discarded_count = discarded_count + 1;
        }
      }
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

/* ManalinkEh 2022: 0x217f8c0; original cards/mercadian_masques.c:5927. */
int card_extravagant_spirit(int player, int card, event_t event)
{
  /* Extravagant Spirit	|3|U
   * Creature - Spirit 4/4
   * Flying
   * At the beginning of your upkeep, sacrifice this creature unless you pay |1 for each card in your hand. */

  int argument3;
  int result;

  upkeep_trigger_ability(player, card, event, player);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }

  argument3 = hand_count[player];
  result = has_mana(player, 0, argument3);
  if ((result != 0) && (charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player,
                                                    COLOR_ANY, argument3),
                        spell_fizzled != 1))
  {
    return 0;
  }
  kill_card(player, card, KILL_SACRIFICE);
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x217f970; original cards/mercadian_masques.c:5953. */
int card_ferocity(int player, int card, event_t event)
{
  /* Ferocity	|1|G
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature blocks or becomes blocked, you may put a +1/+1 counter on it. */

  char attached_player;
  card_instance_t *instance;
  int attached_card;
  int eligible_or_choice;
  int player_00;

  instance = get_card_instance(player, card);
  attached_player = instance->damage_target_player;
  player_00 = (int)attached_player;
  attached_card = instance->damage_target_card;
  if (event == EVENT_CHECK_ABILITIES)
  {
    if ((((attached_card == affected_card) && (affected_card_controller == player_00)) &&
         (player == player_00)) &&
        (instance = in_play(player, card), instance != NULL))
    {
      check_abilities_power = check_abilities_power + 1;
      check_abilities_toughness = check_abilities_toughness + 1;
    }
    in_play(player, card);
  }
  else
  {
    instance = in_play(player, card);
    if ((((event == EVENT_DECLARE_BLOCKERS & (unsigned char)(attached_player >> 7 ^ 0xffU) >> 7) !=
          0) &&
         (instance != NULL)) &&
        (eligible_or_choice = is_humiliated(player, card, AL_TRIG), eligible_or_choice == 0))
    {
      if (current_turn == player_00)
      {
        eligible_or_choice = is_attacking(player_00, attached_card);
        if (((eligible_or_choice != 0) &&
             (eligible_or_choice = is_unblocked(player_00, attached_card),
              eligible_or_choice == 0)) &&
            (eligible_or_choice =
                 do_dialog(player, player, card, -1, -1, " Put a +1/+1 counter\n Pass", 0),
             eligible_or_choice == 0))
        {
          add_counters(player, card, player_00, attached_card, COUNTER_P1_P1, 1);
        }
        if (current_turn == player_00)
          return aura_pref_controller(player, card, event, player, NULL);
      }
      eligible_or_choice = blocking(player_00, attached_card, EVENT_DECLARE_BLOCKERS);
      if ((eligible_or_choice != 0) &&
          (eligible_or_choice =
               do_dialog(player, player, card, -1, -1, " Put a +1/+1 counter\n Pass", 0),
           eligible_or_choice == 0))
      {
        add_counters(player, card, player_00, attached_card, COUNTER_P1_P1, 1);
      }
    }
  }
  return aura_pref_controller(player, card, event, player, NULL);
}
#endif

/* ManalinkEh 2022: 0x216ce60; original cards/mercadian_masques.c:5987. */
static int flailing_manticore_legacy(int player, int card, event_t event)
{
  enum
  {
    CHOICE_P1_P1 = 0,
    CHOICE_M1_M1 = 1
  };

  int p;
  bool source_found;
  card_instance_t *instance;
  int selected_mode;
  card_instance_t *source_record;
  int source_player_or_id;
  unsigned int choice;
  int c;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (((card == affected_card) && (player == affected_card_controller)) && (stack_size == 0))
    {

      instance = get_card_instance(player, card);
      source_player_or_id = instance->targets[0].player;
      if (-1 < source_player_or_id)
      {

        source_player_or_id = get_id(source_player_or_id, instance->targets[0].card);
        if (source_player_or_id != 0x1171)
        {
          instance->targets[0].card = -1;
          instance->targets[0].player = -1;
          kill_card(player, card, KILL_EXILE);
        }
        source_found = false;
        p = 0;
        do
        {
          c = 0;
          if (-1 < active_cards_count[p])
          {
            do
            {
              source_record = in_play(p, c);
              if (((source_record != NULL) && (instance->targets[0].player == p)) &&
                  (instance->targets[0].card == c))
              {
                source_found = true;
                break;
              }
              c = c + 1;
            } while (c <= active_cards_count[p]);
          }
          p = p + 1;
        } while (p != 2);
        if (!source_found)
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
      source_player_or_id = instance->targets[0].player;
      c = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) != 0)
        {
          return 1;
        }
      }
      else if (event == EVENT_ACTIVATE)
      {

        choice = do_dialog(player, player, card, -1, -1, " Add +1/+1\n Add -1/-1\n Cancel", 1);
        instance->targets[1].player = choice;
        if (choice == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice < 2)
        {
          charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        selected_mode = instance->targets[1].player;
        if (selected_mode == CHOICE_P1_P1)
        {
          pump_until_eot_merge_previous(source_player_or_id, c, source_player_or_id, c, 1, 1);
          selected_mode = instance->targets[1].player;
        }
        if (selected_mode == CHOICE_M1_M1)
        {
          pump_until_eot_merge_previous(source_player_or_id, c, source_player_or_id, c, -1, -1);
          return 0;
        }
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x217fba0; original cards/mercadian_masques.c:6060. */
int card_flailing_manticore(int player, int card, event_t event)
{
  /* Flailing Manticore	|3|R
   * Creature - Manticore 3/3
   * Flying, first strike
   * |1: This creature gets +1/+1 until end of turn. Any player may activate this ability.
   * |1: This creature gets -1/-1 until end of turn. Any player may activate this ability. */

  enum
  {
    CHOICE_P1_P1 = 1,
    CHOICE_M1_M1 = 2
  };

  int p;
  int c;
  int choice;
  bool legacy_found;
  card_instance_t *instance;
  card_instance_t *legacy_record;
  int value;
  uint32_t pump_priority;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if (((legacy_record != NULL) && (is_what(p, c, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn == flailing_manticore_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  return 0;
                }
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if ((!legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, flailing_manticore_legacy, player, card);
      }
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          return 0;
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
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      value = is_attacking(player, card);
      if ((value == 0) || (value = is_unblocked(player, card), value == 0))
      {
        pump_priority = 0;
      }
      else
      {
        pump_priority = 10;
      }
      value = has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "+1/+1",
                           has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0),
                           pump_priority, "-1/-1", value, 0, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, (unsigned int)(choice - 1U < 2), 0, 0, 0,
                                            0, 0) != 0)
      {
        instance->info_slot = choice;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      value = instance->info_slot;
      if (value == CHOICE_P1_P1)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->parent_controller, instance->parent_card, 1, 1);
        value = instance->info_slot;
      }
      if (value == CHOICE_M1_M1)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->parent_controller, instance->parent_card, -1, -1);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216d190; original cards/mercadian_masques.c:6145. */
static int flailing_ogre_legacy(int player, int card, event_t event)
{
  enum
  {
    CHOICE_P1_P1 = 0,
    CHOICE_M1_M1 = 1
  };

  int p;
  bool source_found;
  card_instance_t *instance;
  int selected_mode;
  card_instance_t *source_record;
  int source_player_or_id;
  unsigned int choice;
  int c;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (((card == affected_card) && (player == affected_card_controller)) && (stack_size == 0))
    {

      instance = get_card_instance(player, card);
      source_player_or_id = instance->targets[0].player;
      if (-1 < source_player_or_id)
      {

        source_player_or_id = get_id(source_player_or_id, instance->targets[0].card);
        if (source_player_or_id != 0x1172)
        {
          instance->targets[0].card = -1;
          instance->targets[0].player = -1;
          kill_card(player, card, KILL_EXILE);
        }
        source_found = false;
        p = 0;
        do
        {
          c = 0;
          if (-1 < active_cards_count[p])
          {
            do
            {
              source_record = in_play(p, c);
              if (((source_record != NULL) && (instance->targets[0].player == p)) &&
                  (instance->targets[0].card == c))
              {
                source_found = true;
                break;
              }
              c = c + 1;
            } while (c <= active_cards_count[p]);
          }
          p = p + 1;
        } while (p != 2);
        if (!source_found)
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
      source_player_or_id = instance->targets[0].player;
      c = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) != 0)
        {
          return 1;
        }
      }
      else if (event == EVENT_ACTIVATE)
      {

        choice = do_dialog(player, player, card, -1, -1, " Add +1/+1\n Add -1/-1\n Cancel", 1);
        instance->targets[1].player = choice;
        if (choice == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice < 2)
        {
          charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        selected_mode = instance->targets[1].player;
        if (selected_mode == CHOICE_P1_P1)
        {
          pump_until_eot_merge_previous(source_player_or_id, c, source_player_or_id, c, 1, 1);
          selected_mode = instance->targets[1].player;
        }
        if (selected_mode == CHOICE_M1_M1)
        {
          pump_until_eot_merge_previous(source_player_or_id, c, source_player_or_id, c, -1, -1);
          return 0;
        }
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2180010; original cards/mercadian_masques.c:6216. */
int card_flailing_ogre(int player, int card, event_t event)
{
  /* Flailing Ogre	|2|R
   * Creature - Ogre 3/3
   * |1: This creature gets +1/+1 until end of turn. Any player may activate this ability.
   * |1: This creature gets -1/-1 until end of turn. Any player may activate this ability. */

  enum
  {
    CHOICE_P1_P1 = 1,
    CHOICE_M1_M1 = 2
  };

  int p;
  int c;
  int choice;
  bool legacy_found;
  card_instance_t *instance;
  card_instance_t *legacy_record;
  int value;
  uint32_t pump_priority;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if (((legacy_record != NULL) && (is_what(p, c, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn == flailing_ogre_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  return 0;
                }
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if ((!legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, flailing_ogre_legacy, player, card);
      }
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          return 0;
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
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      value = is_attacking(player, card);
      if ((value == 0) || (value = is_unblocked(player, card), value == 0))
      {
        pump_priority = 0;
      }
      else
      {
        pump_priority = 10;
      }
      value = has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "+1/+1",
                           has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0),
                           pump_priority, "-1/-1", value, 0, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, (unsigned int)(choice - 1U < 2), 0, 0, 0,
                                            0, 0) != 0)
      {
        instance->info_slot = choice;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      value = instance->info_slot;
      if (value == CHOICE_P1_P1)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->parent_controller, instance->parent_card, 1, 1);
        value = instance->info_slot;
      }
      if (value == CHOICE_M1_M1)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->parent_controller, instance->parent_card, -1, -1);
        return 0;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216d4c0; original cards/mercadian_masques.c:6300. */
static int flailing_soldier_legacy(int player, int card, event_t event)
{
  enum
  {
    CHOICE_P1_P1 = 0,
    CHOICE_M1_M1 = 1
  };

  int p;
  bool source_found;
  card_instance_t *instance;
  int selected_mode;
  card_instance_t *source_record;
  int source_player_or_id;
  unsigned int choice;
  int c;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (((card == affected_card) && (player == affected_card_controller)) && (stack_size == 0))
    {

      instance = get_card_instance(player, card);
      source_player_or_id = instance->targets[0].player;
      if (-1 < source_player_or_id)
      {

        source_player_or_id = get_id(source_player_or_id, instance->targets[0].card);
        if (source_player_or_id != 0x1173)
        {
          instance->targets[0].card = -1;
          instance->targets[0].player = -1;
          kill_card(player, card, KILL_EXILE);
        }
        source_found = false;
        p = 0;
        do
        {
          c = 0;
          if (-1 < active_cards_count[p])
          {
            do
            {
              source_record = in_play(p, c);
              if (((source_record != NULL) && (instance->targets[0].player == p)) &&
                  (instance->targets[0].card == c))
              {
                source_found = true;
                break;
              }
              c = c + 1;
            } while (c <= active_cards_count[p]);
          }
          p = p + 1;
        } while (p != 2);
        if (!source_found)
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
      source_player_or_id = instance->targets[0].player;
      c = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) != 0)
        {
          return 1;
        }
      }
      else if (event == EVENT_ACTIVATE)
      {

        choice = do_dialog(player, player, card, -1, -1, " Add +1/+1\n Add -1/-1\n Cancel", 1);
        instance->targets[1].player = choice;
        if (choice == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        if (choice < 2)
        {
          charge_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
        }
      }
      else if (event == EVENT_RESOLVE_ACTIVATION)
      {
        selected_mode = instance->targets[1].player;
        if (selected_mode == CHOICE_P1_P1)
        {
          pump_until_eot_merge_previous(source_player_or_id, c, source_player_or_id, c, 1, 1);
          selected_mode = instance->targets[1].player;
        }
        if (selected_mode == CHOICE_M1_M1)
        {
          pump_until_eot_merge_previous(source_player_or_id, c, source_player_or_id, c, -1, -1);
          return 0;
        }
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2180480; original cards/mercadian_masques.c:6371. */
int card_flailing_soldier(int player, int card, event_t event)
{
  /* Flailing Soldier	|R
   * Creature - Human Soldier 2/2
   * |1: This creature gets +1/+1 until end of turn. Any player may activate this ability.
   * |1: This creature gets -1/-1 until end of turn. Any player may activate this ability. */

  enum
  {
    CHOICE_P1_P1 = 1,
    CHOICE_M1_M1 = 2
  };

  int p;
  int c;
  int choice;
  bool legacy_found;
  card_instance_t *instance;
  card_instance_t *legacy_record;
  int value;
  uint32_t pump_priority;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if (((legacy_record != NULL) && (is_what(p, c, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn == flailing_soldier_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  return 0;
                }
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if ((!legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, flailing_soldier_legacy, player, card);
      }
    }
  }
  else
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 1, 0, 0, 0, 0, 0, 0,
                                      NULL, NULL) == 0)
        {
          return 0;
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
      instance = get_card_instance(player, card);
    }
    if (event == EVENT_ACTIVATE)
    {

      instance->info_slot = 0;
      value = is_attacking(player, card);
      if ((value == 0) || (value = is_unblocked(player, card), value == 0))
      {
        pump_priority = 0;
      }
      else
      {
        pump_priority = 10;
      }
      value = has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0);
      choice = dialog_impl(player, card, EVENT_ACTIVATE, DLG_RANDOM, "+1/+1",
                           has_mana_for_activated_ability(player, card, 1, 0, 0, 0, 0, 0),
                           pump_priority, "-1/-1", value, 0, 0, 0, 0, 0);
      if (choice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      if (charge_mana_for_activated_ability(player, card, (unsigned int)(choice - 1U < 2), 0, 0, 0,
                                            0, 0) != 0)
      {
        instance->info_slot = choice;
      }
    }
    else if (event == EVENT_RESOLVE_ACTIVATION)
    {
      value = instance->info_slot;
      if (value == CHOICE_P1_P1)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->parent_controller, instance->parent_card, 1, 1);
        value = instance->info_slot;
      }
      if (value == CHOICE_M1_M1)
      {
        pump_until_eot_merge_previous(instance->parent_controller, instance->parent_card,
                                      instance->parent_controller, instance->parent_card, -1, -1);
        return 0;
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21808f0; original cards/mercadian_masques.c:6455. */
int card_flaming_sword(int player, int card, event_t event)
{
  /* Flaming Sword	|1|R
   * Enchantment - Aura
   * Flash
   * Enchant creature
   * Enchanted creature gets +1/+0 and has first strike. */

  int result;

  attached_gets_pt(player, card, event, 1, 0);
  attached_gets_kw(player, card, event, KEYWORD_FIRST_STRIKE, SP_KEYWORD_0);
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x2180970; original cards/mercadian_masques.c:6474. */
int card_fountain_watch(int player, int card, event_t event)
{
  /* Fountain Watch	|3|W|W
   * Creature - Human Cleric 2/4
   * Artifacts and enchantments you control have shroud. (They can't be the targets of spells or abilities.) */

  if (!in_play(player, card))
    return 0;

  int humiliated = is_humiliated(player, card, AL_STATIC);
  if (event == EVENT_ABILITIES && !humiliated && affected_card_controller == player &&
      (is_what(player, affected_card, TYPE_ENCHANTMENT) ||
       is_what(affected_card_controller, affected_card, TYPE_ARTIFACT)))
    event_result |= KEYWORD_SHROUD;
  return 0;
}

/* ManalinkEh 2022: 0x2180a20; original cards/mercadian_masques.c:6491. */
int card_fountain_of_cho(int player, int card, event_t event)
{
  /* Fountain of Cho	""
   * Land
   * This land enters tapped.
   * |T: Put a storage counter on this land.
   * |T, Remove any number of storage counters from this land: Add |W for each storage counter removed this way. */

  int result;

  result = mercadian_storage_land(player, card, event, COLOR_WHITE);
  return result;
}

/* ManalinkEh 2022: 0x2180a40; original cards/mercadian_masques.c:6502. */
int card_ghouls_feast(int player, int card, event_t event)
{
  /* Ghoul's Feast	|1|B
   * Instant
   * Target creature gets +X/+0 until end of turn, where X is the number of creature cards in your graveyard. */

  card_instance_t *instance;
  int result;
  card_instance_t *spell_record;
  target_definition_t td;

  if (event == EVENT_CHECK_PUMP)
  {
    instance = get_card_instance(player, card);
    spell_record = get_card_instance(player, card);
    result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, spell_record->internal_card_id);
    if ((((result != 0) && (instance->number_of_targets == '\x01')) &&
         (-1 < instance->targets[0].player)) &&
        (-1 < instance->targets[0].card))
    {
      result = count_graveyard_by_type(player, 2);
      pumpable_power[player] = pumpable_power[player] + result;
    }
    return 0;
  }
  if (event != EVENT_CAN_CAST)
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
    else
    {
      if ((event == EVENT_RESOLVE_SPELL) || (event == EVENT_CAN_CHANGE_TARGET))
      {
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        instance = get_card_instance(player, card);
        if (event == EVENT_RESOLVE_SPELL)
        {
          result = valid_target(&td);
          if (result != 0)
          {
            result = count_graveyard_by_type(player, 2);
            pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                           result, 0);
          }
          kill_card(player, card, KILL_DESTROY);
        }
        return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
      }
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
    }
  }
  default_target_definition(player, card, &td, 2);
  td.preferred_controller = player;
  get_card_instance(player, card);
  return generic_spell(player, card, event, 1, &td, "TARGET_CREATURE", 1, NULL);
}

/* ManalinkEh 2022: 0x2180c50; original cards/mercadian_masques.c:6539. */
int card_glowing_anemone(int player, int card, event_t event)
{
  /* Glowing Anemone	|3|U
   * Creature - Jellyfish Beast 1/3
   * When this creature enters, you may return target land to its owner's hand. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  default_target_definition(player, card, &td, 1);
  instance = get_card_instance(player, card);
  result = can_target(&td);
  if (result != 0)
  {
    result = new_pick_target(&td, "Select target land, return it to its owner\'s hand.", 0, 0x80);
    if (result != 0)
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    }
  }
  instance->number_of_targets = '\0';
  return 0;
}

/* ManalinkEh 2022: 0x2180d10; original cards/mercadian_masques.c:6562. */
int card_high_seas(int player, int card, event_t event)
{
  /* High Seas	|2|U
   * Enchantment
   * Red creature spells and green creature spells cost |1 more to cast. */

  card_instance_t *instance;
  unsigned int spell_color;
  unsigned int affected_colors;

  if (event == EVENT_MODIFY_COST_GLOBAL)
  {
    if (is_what(affected_card_controller, affected_card, 2))
    {
      instance = in_play(player, card);
      if (instance != NULL)
      {
        if (!is_humiliated(player, card, AL_STATIC))
        {
          spell_color = get_color(affected_card_controller, affected_card);
          affected_colors = get_sleighted_color_test(player, card, 0x18);
          if ((spell_color & affected_colors) != 0)
          {
            COST_COLORLESS = COST_COLORLESS + '\x01';
          }
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2180dd0; original cards/mercadian_masques.c:6577. */
int card_hoodwink(int player, int card, event_t event)
{
  /* Hoodwink	|1|U
   * Instant
   * Return target artifact, enchantment, or land to its owner's hand. */

  card_instance_t *instance;
  target_definition_t td2;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td2, 0x45);
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td2))
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 0x81, &td2,
                         "Select target artifact, enchantment, or land card.", 1, NULL);
}

/* ManalinkEh 2022: 0x2180f10; original cards/mercadian_masques.c:6602. */
int card_ignoble_soldier(int player, int card, event_t event)
{
  /* Ignoble Soldier	|2|W
   * Creature - Human Soldier 3/1
   * Whenever this creature becomes blocked, prevent all combat damage that would be dealt by it this turn. */

  int result;

  if (event == EVENT_DECLARE_BLOCKERS)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_unblocked(player, card);
      if (result == 0)
      {
        result = is_humiliated(player, card, AL_TRIG);
        if (result == 0)
        {
          negate_combat_damage_this_turn(player, card, player, card, 0);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2180fa0; original cards/mercadian_masques.c:6614. */
int card_indentured_djinn(int player, int card, event_t event)
{
  /* Indentured Djinn	|1|U|U
   * Creature - Djinn 4/4
   * Flying
   * When this creature enters, each other player may draw up to three cards. */

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event != EVENT_ETB)
  {
    return 0;
  }
  draw_some_cards_if_you_want(player, card, 1 - player, 3);
  return 0;
}

/* ManalinkEh 2022: 0x216dd10; original cards/mercadian_masques.c:6630. */
static int creatures_target_player_controls_attack_this_turn_if_able(int player, int card,
                                                                     event_t event)
{
  all_must_attack_if_able(player, event, -1);
  if (event != EVENT_CLEANUP)
  {
    return 0;
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x2181010; original cards/mercadian_masques.c:6639. */
int card_instigator(int player, int card, event_t event)
{
  /* Instigator	|1|B
   * Creature - Human Spellshaper 1/1
   * |1|B|B, |T, Discard a card: Creatures target player controls attack this turn if able. */

  int result;
  target_definition_t td_player;
  default_target_definition(player, card, &td_player, 0);
  td_player.zone = TARGET_ZONE_PLAYERS;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    result = get_original_internal_card_id(player, card);
    result = add_card_to_hand(1 - player, result);
    create_legacy_effect(1 - player, result,
                         creatures_target_player_controls_attack_this_turn_if_able);
  }
  result = generic_activated_ability(player, card, event, 0x25, 1, 2, 0, 0, 0, 0, 0, &td_player,
                                     "TARGET_PLAYER");
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2181100; original cards/mercadian_masques.c:6657. */
int card_insubordination(int player, int card, event_t event)
{
  /* Insubordination	|B|B
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of the end step of enchanted creature's controller, this Aura deals 2 damage to that player unless that creature attacked this turn. */

  if (in_play(player, card))
  {
    card_instance_t *instance = get_card_instance(player, card);
    int attached_player = instance->damage_target_player;
    if (attached_player >= 0 && instance->damage_target_card >= 0 &&
        current_turn == attached_player)
    {
      card_instance_t *attached = get_card_instance(attached_player, instance->damage_target_card);
      if (!(attached->state & (STATE_ATTACKING | STATE_ATTACKED)))
      {
        int end_of_turn = eot_trigger(player, card, event);
        if ((event == EVENT_SHOULD_AI_PLAY || end_of_turn) &&
            !is_humiliated(player, card, AL_STATIC))
          damage_player(attached_player, 2, player, card);
      }
    }
  }
  return aura_pref_controller(player, card, event, 1 - player, NULL);
}
#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21811e0; original cards/mercadian_masques.c:6676. */
int card_inviolability(int player, int card, event_t event)
{
  /* Inviolability	|1|W
   * Enchantment - Aura
   * Enchant creature
   * Prevent all damage that would be dealt to enchanted creature. */

  unsigned char attached_player_byte;
  card_instance_t *instance;
  int result;
  card_instance_t *damage_source;

  instance = in_play(player, card);
  if (instance != NULL)
  {

    instance = get_card_instance(player, card);
    result = instance->damage_target_card;
    attached_player_byte = instance->damage_target_player;
    if (((char)((attached_player_byte ^ 0xff) &
                ((unsigned char)((unsigned int)result >> 0x18) ^ 0xff)) < '\0') &&
        (event == EVENT_DEAL_DAMAGE))
    {

      instance = get_card_instance(affected_card_controller, affected_card);
      if ((instance->internal_card_id == damage_card) &&
          ((instance->damage_target_card == result &&
            (attached_player_byte == instance->damage_target_player))))
      {
        result = check_state(affected_card_controller, affected_card, 0x100000);
        if (result == 0)
        {
          damage_source =
              in_play((int)instance->damage_source_player, instance->damage_source_card);
          if (damage_source != NULL)
          {

            prevent_damage(affected_card_controller, affected_card, instance->info_slot);
          }
        }
      }
    }
  }
  return aura_pref_controller(player, card, event, player, NULL);
}
#endif

/* ManalinkEh 2022: 0x2181300; original cards/mercadian_masques.c:6705. */
int card_jeweled_torque(int player, int card, event_t event)
{
  /* Jeweled Torque	|2
   * Artifact
   * As this artifact enters, choose a color.
   * Whenever a player casts a spell of the chosen color, you may pay |2. If you do, you gain 2 life. */

  card_instance_t *instance;
  int result;
  test_definition_t test;

  if (card == -1)
  {
    return 0;
  }
  bool cast_trigger;
  if (event == EVENT_RESOLVE_SPELL)
  {
    result = choose_a_color_and_show_legacy(player, card, 1 - player, -1);
    instance = get_card_instance(player, card);
    instance->targets[2].player = 1 << ((unsigned char)result & 0x1f);
    cast_trigger = trigger_condition == TRIGGER_SPELL_CAST;
    if (!cast_trigger)
      return 0;
  }
  else
    cast_trigger = trigger_condition == TRIGGER_SPELL_CAST;
  if (cast_trigger && card == affected_card && player == affected_card_controller)
  {
    instance = get_card_instance(player, card);
    instance->number_of_targets = '\0';
    default_test_definition(&test, 0xff);
    instance = get_card_instance(player, card);
    test.color = instance->targets[2].player;
    result = duh_mode(player);
    sp_ability_trigger(2, event, (result != 0) + 1, TYPE_NONE, &test, player, card);
  }
  if (((event == EVENT_SPA) && (result = has_mana(player, 7, 2), result != 0)) &&
      (result =
           charge_mana_while_resolving(player, card, EVENT_RESOLVE_TRIGGER, player, COLOR_ANY, 2),
       result != 0))
  {
    gain_life(player, 2);
  }
  return 0;
}

/* ManalinkEh 2022: 0x21814a0; original cards/mercadian_masques.c:6737. */
int card_kyren_archive(int player, int card, event_t event)
{
  /* Kyren Archive	|3
   * Artifact - Book
   * At the beginning of your upkeep, you may exile the top card of your library face down.
   * |5, Discard your hand, Sacrifice this artifact: Put all cards exiled with this artifact into their owner's hand. */

  int result;
  card_instance_t *instance;
  unsigned int *remembered_entry;
  unsigned int iid;
  unsigned int entry_or_owner;
  int leg;
  int idx;

  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {

    exiledby_remember(player, card, player, *deck_ptr[player], &leg, &idx);
    rfg_top_card_of_deck(player);
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE)
  {
    return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 8, 5, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
  }
  if (event == EVENT_ACTIVATE)
  {
    new_discard_all(player, player);
    instance = get_card_instance(player, card);
    result = exiledby_detach(player, card);
    instance->targets[1].card = result;
    return generic_activated_ability(player, card, EVENT_ACTIVATE, 8, 5, 0, 0, 0, 0, 0, 0, NULL,
                                     NULL);
  }
  if (event != EVENT_RESOLVE_ACTIVATION)
  {
    return 0;
  }

  instance = get_card_instance(player, card);
  leg = 0;
  idx = 0;
  while (true)
  {
    remembered_entry =
        (unsigned int *)exiledby_find_any(player + -2, instance->targets[1].card, &leg, &idx);
    if (remembered_entry == NULL)
      break;

    entry_or_owner = *remembered_entry;
    *remembered_entry = 0xffffffff;
    iid = entry_or_owner & 0x7fffffff;
    entry_or_owner = entry_or_owner >> 0x1f;
    add_card_to_hand(entry_or_owner, iid);
    remove_card_from_rfg(entry_or_owner, (unsigned int)cards_data[iid].id);
  }
  return 0;
}

/* ManalinkEh 2022: 0x21816f0; original cards/mercadian_masques.c:6781. */
int card_kyren_legate(int player, int card, event_t event)
{
  /* Kyren Legate	|1|R
   * Creature - Goblin 1/1
   * If an opponent controls a Plains and you control a Mountain, you may cast this spell without paying its mana cost.
   * Haste */

  int result;
  card_instance_t *instance;

  haste(player, card, event);
  if (event != EVENT_MODIFY_COST)
  {
    if ((((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (result = nemesis_freespell(player, card, 0x6c), result == 0))
    {
      spell_fizzled = 1;
    }
    return 0;
  }
  result = get_hacked_subtype(player, card, 0x5007);
  result = check_battlefield_for_subtype(1 - player, 1, result);
  if (result != 0)
  {
    result = get_hacked_subtype(player, card, 0x5006);
    result = check_battlefield_for_subtype(player, 1, result);
    if (result != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  instance->info_slot = 0;
  return 0;
}

/* ManalinkEh 2022: 0x2181810; original cards/mercadian_masques.c:6811. */
int card_kyren_sniper(int player, int card, event_t event)
{
  /* Kyren Sniper	|2|R
   * Creature - Goblin 1/1
   * At the beginning of your upkeep, you may have this creature deal 1 damage to target player or planeswalker. */

  int result;
  card_instance_t *instance;
  target_definition_t td;
  result = duh_mode(player);
  upkeep_trigger_ability_mode(player, card, event, player,
                              (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    default_target_definition(player, card, &td, 0x100);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      instance->number_of_targets = '\0';
      result = pick_target(&td, "TARGET_PLAYER_OR_PLANESWALKER");
      if (result != 0)
      {
        damage_creature(instance->targets[0].player, instance->targets[0].card, 1, player, card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21818f0; original cards/mercadian_masques.c:6834. */
int card_lava_runner(int player, int card, event_t event)
{
  /* Lava Runner	|1|R|R
   * Creature - Lizard 2/2
   * Haste
   * Whenever this creature becomes the target of a spell or ability, that spell or ability's controller sacrifices a land of their choice. */

  card_instance_t *instance;
  const target_t *targeting_source;

  haste(player, card, event);
  instance = in_play(player, card);
  if (instance != NULL)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {

      targeting_source = becomes_target_of_spell_or_effect(player, card, event, player, card, 2);
      if (targeting_source != NULL)
      {
        player_sacrifices_a_permanent(player, card, targeting_source->player, 1,
                                      SAC_CAUSED | SAC_NO_CANCEL);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2181990; original cards/mercadian_masques.c:6851. */
int card_ley_line(int player, int card, event_t event)
{
  /* Ley Line	|3|G
   * Enchantment
   * At the beginning of each player's upkeep, that player may put a +1/+1 counter on target creature of their choice. */

  int result;
  card_instance_t *instance;
  target_definition_t td;

  upkeep_trigger_ability(player, card, event, 2);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {

    default_target_definition(player, card, &td, 2);
    td.preferred_controller = current_turn;
    td.who_chooses = current_turn;
    instance = get_card_instance(player, card);
    result = can_target(&td);
    if (result != 0)
    {
      result = new_pick_target(&td, "Select target creature put a +1/+1 counter on it.", 0, 0x80);
      if (result != 0)
      {
        add_counters(player, card, instance->targets[0].player,
                                 instance->targets[0].card, COUNTER_P1_P1, 1);
      }
    }
    instance->number_of_targets = '\0';
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2181a90; original cards/mercadian_masques.c:6875. */
int card_lithophage(int player, int card, event_t event)
{
  /* Lithophage	|3|R|R
   * Creature - Insect 7/7
   * At the beginning of your upkeep, sacrifice this creature unless you sacrifice a Mountain. */

  const char *msg;
  int result;
  test_definition_t test2;

  upkeep_trigger_ability(player, card, event, player);
  if (event == EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    msg = get_hacked_land_text(player, card, "Select a %s to sacrifice.", 0x5006);
    new_default_test_definition(&test2, 0x47, msg);
    test2.subtype = get_hacked_subtype(player, card, 0x5006);
    result = new_sacrifice(player, card, player, 0, &test2);
    if (result == 0)
    {
      kill_card(player, card, KILL_SACRIFICE);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2181b60; original cards/mercadian_masques.c:6899. */
int card_magistrates_veto(int player, int card, event_t event)
{
  /* Magistrate's Veto	|2|R
   * Enchantment
   * White creatures and blue creatures can't block. */

  card_instance_t *instance;
  unsigned int creature_color;
  unsigned int affected_colors;

  if (event == EVENT_BLOCK_LEGALITY)
  {
    instance = in_play(player, card);
    if (instance != NULL)
    {
      if (!is_humiliated(player, card, AL_STATIC))
      {
        creature_color = get_color(affected_card_controller, affected_card);
        affected_colors = get_sleighted_color_test(player, card, 0x24);
        if ((creature_color & affected_colors) != 0)
        {
          event_result = 1;
        }
      }
    }
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2181c00; original cards/mercadian_masques.c:6913. */
int card_mercadias_downfall(int player, int card, event_t event)
{
  /* Mercadia's Downfall	|2|R
   * Instant
   * Each attacking creature gets +1/+0 until end of turn for each nonbasic land defending player controls. */

  int land_count;
  int basic_land_count;
  test_definition_t this_test;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 2);
    this_test.state = 4;
    land_count = count_subtype(1 - current_turn, 1, -1);
    basic_land_count = count_subtype(1 - current_turn, 1, 0x2000);
    pump_creatures_until_eot(player, card, current_turn, 1, land_count - basic_land_count, 0, 0, 0,
                             &this_test);
    kill_card(player, card, KILL_DESTROY);
  }
  return basic_spell(player, card, event);
}

/* ManalinkEh 2022: 0x2181d10; original cards/mercadian_masques.c:6932. */
int card_mercadian_bazaar(int player, int card, event_t event)
{
  /* Mercadian Bazaar	""
   * Land
   * This land enters tapped.
   * |T: Put a storage counter on this land.
   * |T, Remove any number of storage counters from this land: Add |R for each storage counter removed this way. */

  int result;

  result = mercadian_storage_land(player, card, event, COLOR_RED);
  return result;
}

/* ManalinkEh 2022: 0x2181d30; original cards/mercadian_masques.c:6943. */
int card_molting_harpy(int player, int card, event_t event)
{
  /* Molting Harpy	|B
   * Creature - Harpy Mercenary 2/1
   * Flying
   * At the beginning of your upkeep, sacrifice this creature unless you pay |2. */

  basic_upkeep(player, card, event, 2, 0, 0, 0, 0, 0);
  return 0;
}

/* ManalinkEh 2022: 0x2181d90; original cards/mercadian_masques.c:6954. */
int card_panacea(int player, int card, event_t event)
{
  /* Panacea	|4
   * Artifact
   * |X|X, |T: Prevent the next X damage that would be dealt to any target this turn. */

  int result;
  card_instance_t *instance;
  card_instance_t *parent;
  target_definition_t td;

  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0x102);
    td.preferred_controller = player;
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 5, 2, 0, 0, 0, 0, 0, 0,
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
      result = generic_activated_ability(player, card, EVENT_ACTIVATE, 7, -2, 0, 0, 0, 0, 0, 0, &td,
                                         "Select any target to prevent the next X damage to it.");
      if (result == 0)
      {
        return 0;
      }
      instance->info_slot = x_value / 2;
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
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    instance = get_card_instance(player, card);
    parent = get_card_instance(instance->parent_controller, instance->parent_card);
    prevent_the_next_n_damage(instance->parent_controller, instance->parent_card,
                              parent->targets[0].player, parent->targets[0].card, instance->info_slot,
                              0, 0, 0);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2181fe0; original cards/mercadian_masques.c:6999. */
int card_port_inspector(int player, int card, event_t event)
{
  /* Port Inspector	|1|U
   * Creature - Human 1/2
   * Whenever this creature becomes blocked, you may look at defending player's hand. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((event == EVENT_DECLARE_BLOCKERS) && (result == 0))
    {
      result = is_attacking(player, card);
      if (result != 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          result = do_dialog(player, player, card, -1, -1, " Look opponent\'s hand\n Pass", 0);
          if (result == 0)
          {
            reveal_target_player_hand(1 - current_turn);
          }
        }
      }
    }
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x21820a0; original cards/mercadian_masques.c:7016. */
int card_pretenders_claim(int player, int card, event_t event)
{
  /* Pretender's Claim	|1|B
   * Enchantment - Aura
   * Enchant creature
   * Whenever enchanted creature becomes blocked, tap all lands defending player controls. */

  card_instance_t *instance;
  int result;
  test_definition_t test;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    instance = get_card_instance(player, card);
    if (-1 < instance->damage_target_player)
    {

      instance = get_card_instance(player, card);
      if (event == EVENT_DECLARE_BLOCKERS)
      {
        result = is_attacking((int)instance->damage_target_player, instance->damage_target_card);
        if (result != 0)
        {
          result = is_unblocked((int)instance->damage_target_player, instance->damage_target_card);
          if (result == 0)
          {
            default_test_definition(&test, 1);
            new_manipulate_all(player, card, 1 - current_turn, &test, ACT_TAP);
          }
        }
      }
    }
  }
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x21821a0; original cards/mercadian_masques.c:7037. */
int card_putrefaction(int player, int card, event_t event)
{
  /* Putrefaction	|4|B
   * Enchantment
   * Whenever a player casts a green or white spell, that player discards a card. */

  int result;
  test_definition_t test;

  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 0x28);
    sp_ability_trigger(2, event, 2, TYPE_NONE, &test, player, card);
  }
  if (event == EVENT_SPA)
  {
    discard(trigger_cause_controller, 0, player);
  }
  result = global_enchantment(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x2182280; original cards/mercadian_masques.c:7056. */
int card_quagmire_lamprey(int player, int card, event_t event)
{
  /* Quagmire Lamprey	|2|B
   * Creature - Fish 1/1
   * Whenever this creature becomes blocked by a creature, put a -1/-1 counter on that creature. */

  int result;
  int player_00;
  card_instance_t *instance;
  char marked[2][151];

  if (((event == EVENT_DECLARE_BLOCKERS) && (result = is_attacking(player, card), result != 0)) &&
      (result = is_humiliated(player, card, AL_TRIG), result == 0))
  {

    memset(marked, 0, sizeof(marked));
    mark_each_creature_blocking_me(player, card, marked);
    player_00 = 1 - current_turn;
    result = 0;
    if (0 < active_cards_count[player_00])
    {
      do
      {
        if ((marked[player_00][result] != '\0') &&
            (instance = in_play(player_00, result), instance != NULL))
        {
          add_counters(player, card, 1 - current_turn, result, COUNTER_M1_M1, 1);
        }
        player_00 = 1 - current_turn;
        result = result + 1;
      } while (result < active_cards_count[player_00]);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21823d0; original cards/mercadian_masques.c:7075. */
int card_ramosian_rally(int player, int card, event_t event)
{
  /* Ramosian Rally	|3|W
   * Instant
   * If you control a Plains, you may tap an untapped creature you control rather than pay this spell's mana cost.
   * Creatures you control get +1/+1 until end of turn. */

  card_instance_t *instance;
  int result;
  card_instance_t *spell_record;
  target_definition_t td2;

  if (event == EVENT_MODIFY_COST)
  {

    instance = get_card_instance(player, card);
    base_target_definition(player, card, &td2, 2);
    td2.allowed_controller = player;
    td2.preferred_controller = player;
    td2.illegal_state = TARGET_STATE_TAPPED;
    result = check_battlefield_for_subtype(player, 1, 0x5007);
    if ((result == 0) || (result = can_target(&td2), result == 0))
    {
      instance->info_slot = 0;
    }
    else
    {
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
    get_card_instance(player, card);
  }
  else
  {
    if (event == EVENT_CAN_CAST)
    {
      return basic_spell(player, card, EVENT_CAN_CAST);
    }
    instance = get_card_instance(player, card);
    if (event == EVENT_CAST_SPELL)
    {
      if ((card == affected_card) && (player == affected_card_controller))
      {
        instance->number_of_targets = '\0';
        result = played_for_free(player, card);
        if ((result == 0) &&
            ((result = is_token(player, card), result == 0 && (instance->info_slot == 1))))
        {

          spell_record = get_card_instance(player, card);
          result = has_mana_to_cast_iid(player, spell_record->internal_card_id, 0x6c);
          if ((result == 0) ||
              (result = do_dialog(player, player, card, -1, -1,
                                  " Tap a creature you control\n Play normally\n Cancel", 0),
               result == 0))
          {
            base_target_definition(player, card, &td2, 2);
            td2.allowed_controller = player;
            td2.preferred_controller = player;
            td2.illegal_state = TARGET_STATE_TAPPED;
            result = new_pick_target(&td2, "Select a creature you control to tap.", 0, 0x81);
            if (result != 0)
            {
              action_on_target(player, card, 0, ACT_TAP);
              instance->number_of_targets = '\0';
              return 0;
            }
          }
          else if (result == 1)
          {
            result = get_id(player, card);
            charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
            return 0;
          }
          spell_fizzled = 1;
        }
      }
    }
    else if (event == EVENT_RESOLVE_SPELL)
    {
      pump_subtype_until_eot(player, card, player, -1, 1, 1, 0, 0);
      kill_card(player, card, KILL_DESTROY);
      return 0;
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21826b0; original cards/mercadian_masques.c:7145. */
int card_rappelling_scouts(int player, int card, event_t event)
{
  /* Rappelling Scouts	|2|W|W
   * Creature - Human Rebel Scout 1/4
   * Flying
   * |2|W: This creature gains protection from the color of your choice until end of turn. */

  card_instance_t *instance;
  unsigned int protection;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    protection = select_a_protection(player);
    pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                           instance->parent_controller, instance->parent_card, 0, 0,
                           protection | 0x10000000, 0);
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 1, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x216b9c0; original cards/mercadian_masques.c:7161. */
static char *target_must_be_damage_targeting_this_revered_elder(int who_chooses, int player,
                                                                int card, int targeting_player,
                                                                int targeting_card)
{
  card_instance_t *instance;
  char *requirement_error;

  instance = get_card_instance(player, card);
  requirement_error = "must be damaging Revered Elder";
  if (((instance->internal_card_id == damage_card) &&
       (instance->damage_target_player == targeting_player)) &&
      (instance->damage_target_card == targeting_card))
  {
    requirement_error = NULL;
  }
  return requirement_error;
}

/* ManalinkEh 2022: 0x2182790; original cards/mercadian_masques.c:7172. */
int card_revered_elder(int player, int card, event_t event)
{
  /* Revered Elder	|2|W
   * Creature - Human Cleric 1/2
   * |1: Prevent the next 1 damage that would be dealt to this creature this turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
  {
    default_target_definition(player, card, &td, 0);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)target_must_be_damage_targeting_this_revered_elder;
    if (event == EVENT_CAN_ACTIVATE)
    {
      result = generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x1000004, 1, 0, 0, 0, 0,
                                         0, 0, &td, NULL);
      if (result == 0)
      {
        return 0;
      }
      return 99;
    }
    if (event == EVENT_ACTIVATE)
    {
      generic_activated_ability(player, card, EVENT_ACTIVATE, 0x1000006, 1, 0, 0, 0, 0, 0, 0, &td,
                                "Select damage made to Revered Elder.");
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td, 0);
  }
  td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
  td.extra = (intptr_t)target_must_be_damage_targeting_this_revered_elder;
  if (event == EVENT_RESOLVE_ACTIVATION)
  {
    instance = get_card_instance(player, card);
    result = valid_target(&td);
    if (result != 0)
    {
      prevent_damage(instance->targets[0].player, instance->targets[0].card, 1);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216bac0; original cards/mercadian_masques.c:7207. */
static void whenever_a_creature_blocks_black_red(int color_test, int val, int t_player, int t_card)
{
  unsigned int attacker_color;

  attacker_color = get_color(t_player, t_card);
  if ((color_test & attacker_color) != 0)
  {
    *(uint32_t *)val = 1;
  }
  return;
}

/* ManalinkEh 2022: 0x2182960; original cards/mercadian_masques.c:7213. */
int card_righteous_indignation(int player, int card, event_t event)
{
  /* Righteous Indignation	|2|W
   * Enchantment
   * Whenever a creature blocks a black or red creature, the blocking creature gets +1/+1 until end of turn. */

  int blocker_index_or_humiliated;
  card_instance_t *instance;
  int eligible_or_color;
  int blocking_black;

  if (((event == EVENT_DECLARE_BLOCKERS) &&
       (blocker_index_or_humiliated = is_humiliated(player, card, AL_TRIG),
        blocker_index_or_humiliated == 0)) &&
      (blocker_index_or_humiliated = active_cards_count[1 - current_turn] + -1,
       -1 < blocker_index_or_humiliated))
  {
    do
    {
      instance = in_play(1 - current_turn, blocker_index_or_humiliated);
      if (((instance != NULL) &&
           (eligible_or_color = is_what(1 - current_turn, blocker_index_or_humiliated, 2),
            eligible_or_color != 0)) &&
          (eligible_or_color =
               blocking(1 - current_turn, blocker_index_or_humiliated, EVENT_DECLARE_BLOCKERS),
           eligible_or_color != 0))
      {

        instance = get_card_instance(1 - current_turn, blocker_index_or_humiliated);
        eligible_or_color = get_sleighted_color_test(player, card, 0x12);
        blocking_black = 0;
        if (((instance->state & 8) != 0) &&
            (for_each_creature_blocked_by_me(1 - current_turn, blocker_index_or_humiliated,
                                             whenever_a_creature_blocks_black_red,
                                             eligible_or_color, (int)&blocking_black),
             blocking_black != 0))
        {
          pump_until_eot(player, card, 1 - current_turn, blocker_index_or_humiliated, 1, 1);
        }
      }
      blocker_index_or_humiliated = blocker_index_or_humiliated + -1;
    } while (blocker_index_or_humiliated != -1);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2182ae0; original cards/mercadian_masques.c:7237. */
int card_rouse(int player, int card, event_t event)
{
  /* Rouse	|1|B
   * Instant
   * If you control a Swamp, you may pay 2 life rather than pay this spell's mana cost.
   * Target creature gets +2/+0 until end of turn. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {

    instance = get_card_instance(player, card);
    base_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    result = check_battlefield_for_subtype(player, 1, 0x5009);
    if (((result != 0) && (result = can_target(&td), result != 0)) &&
        (result = can_pay_life(player, 2), result != 0))
    {
      instance->info_slot = 1;
      null_casting_cost(player, card);
      return 0;
    }
    instance->info_slot = 0;
  }
  else
  {
    if (event == EVENT_CHECK_PUMP)
    {
      instance = get_card_instance(player, card);
      result = has_mana_to_cast_iid(player, EVENT_CAN_CAST, instance->internal_card_id);
      if ((result == 0) && (result = check_battlefield_for_subtype(player, 1, 0x5009), result == 0))
      {
        return 0;
      }
      pumpable_power[player] = pumpable_power[player] + 2;
      return 0;
    }
    if (event == EVENT_CAN_CAST)
    {
      default_target_definition(player, card, &td, 2);
      get_card_instance(player, card);
      return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
    }
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
      default_target_definition(player, card, &td, 2);
      instance = get_card_instance(player, card);
      if (card != affected_card)
      {
        return 0;
      }
      if (player != affected_card_controller)
      {
        return 0;
      }
      result = played_for_free(player, card);
      if (((result == 0) && (result = is_token(player, card), result == 0)) &&
          (instance->info_slot == 1))
      {

        result = has_mana_to_cast_iid(player, EVENT_CAST_SPELL, instance->internal_card_id);
        if (result != 0)
        {
          result = can_pay_life(player, 2);
          result = dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                               "Cast for free/pay 2 life", result, 10, "Cast normally", 1, 5, 0, 0,
                               0, 0);
          if (result != 1)
          {
            if (result == 2)
            {
              result = get_id(player, card);
              charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
            }
            else
            {
              spell_fizzled = 1;
            }
            return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                                 "Select target creature it gets +2/+0.", 1, NULL);
          }
        }
        lose_life(player, 2);
      }
      return generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td,
                           "Select target creature it gets +2/+0.", 1, NULL);
    }
    if (event == EVENT_CAN_CHANGE_TARGET)
    {
      default_target_definition(player, card, &td, 2);
      get_card_instance(player, card);
      return 0;
    }
  }
  if (event != EVENT_RESOLVE_SPELL)
  {
    if (event != EVENT_CHANGE_TARGET)
    {
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
    return 0;
  }
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (event != EVENT_RESOLVE_SPELL)
  {
    return 0;
  }
  result = valid_target(&td);
  if (result != 0)
  {
    pump_until_eot(player, card, instance->targets[0].player, instance->targets[0].card, 2, 0);
  }
  kill_card(player, card, KILL_DESTROY);
  return 0;
}

/* ManalinkEh 2022: 0x2182f80; original cards/mercadian_masques.c:7313. */
int card_rushwood_grove(int player, int card, event_t event)
{
  /* Rushwood Grove	""
   * Land
   * This land enters tapped.
   * |T: Put a storage counter on this land.
   * |T, Remove any number of storage counters from this land: Add |G for each storage counter removed this way. */

  int result;

  result = mercadian_storage_land(player, card, event, COLOR_GREEN);
  return result;
}

/* ManalinkEh 2022: 0x2182fa0; original cards/mercadian_masques.c:7324. */
int card_rushwood_legate(int player, int card, event_t event)
{
  /* Rushwood Legate	|2|G
   * Creature - Dryad 2/1
   * If an opponent controls an Island and you control a Forest, you may cast this spell without paying its mana cost. */

  int result;
  card_instance_t *instance;

  if (event != EVENT_MODIFY_COST)
  {
    if ((((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (result = nemesis_freespell(player, card, 0x6c), result == 0))
    {
      spell_fizzled = 1;
    }
    return 0;
  }
  result = get_hacked_subtype(player, card, 0x5002);
  result = check_battlefield_for_subtype(1 - player, 1, result);
  if (result != 0)
  {
    result = get_hacked_subtype(player, card, 0x5001);
    result = check_battlefield_for_subtype(player, 1, result);
    if (result != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  instance->info_slot = 0;
  return 0;
}

/* ManalinkEh 2022: 0x21830c0; original cards/mercadian_masques.c:7351. */
int card_sacred_prey(int player, int card, event_t event)
{
  /* Sacred Prey	|G
   * Creature - Horse 1/1
   * Whenever this creature becomes blocked, you gain 1 life. */

  card_instance_t *instance;
  int result;

  instance = in_play(player, card);
  if (instance != NULL)
  {
    result = is_humiliated(player, card, AL_STATIC);
    if ((event == EVENT_DECLARE_BLOCKERS) && (result == 0))
    {
      result = is_attacking(player, card);
      if (result != 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          gain_life(player, 1);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216ca80; original cards/mercadian_masques.c:7366. */
static int sailmonger_legacy(int player, int card, event_t event)
{
  int p;
  bool source_found;
  card_instance_t *instance;
  card_instance_t *source_record;
  int source_player_or_id;
  int c;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if (((card == affected_card) && (player == affected_card_controller)) && (stack_size == 0))
    {

      instance = get_card_instance(player, card);
      source_player_or_id = instance->targets[0].player;
      if (-1 < source_player_or_id)
      {

        source_player_or_id = get_id(source_player_or_id, instance->targets[0].card);
        if (source_player_or_id != 0x11ec)
        {
          instance->targets[0].card = -1;
          instance->targets[0].player = -1;
          kill_card(player, card, KILL_EXILE);
        }
        source_found = false;
        p = 0;
        do
        {
          c = 0;
          if (-1 < active_cards_count[p])
          {
            do
            {
              source_record = in_play(p, c);
              if (((source_record != NULL) && (instance->targets[0].player == p)) &&
                  (instance->targets[0].card == c))
              {
                source_found = true;
                break;
              }
              c = c + 1;
            } while (c <= active_cards_count[p]);
          }
          p = p + 1;
        } while (p != 2);
        if (!source_found)
        {
          kill_card(player, card, KILL_EXILE);
        }
      }
    }
    instance = get_card_instance(player, card);
    if (instance->targets[0].player != -1)
    {
      return 0;
    }
  }
  else
  {
    instance = get_card_instance(player, card);
    if ((instance->targets[0].player != -1) &&
        (((event == EVENT_CAN_ACTIVATE || (event == EVENT_ACTIVATE)) ||
          ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))))
    {
      default_target_definition(player, card, &td, 2);
      td.preferred_controller = player;
      if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
      {
        td.illegal_abilities = td.illegal_abilities | 0x20;
      }
      if (event == EVENT_CAN_ACTIVATE)
      {
        return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 2, 0, 0, 0, 0, 0, 0,
                                         &td, "TARGET_CREATURE");
      }
      if (event == EVENT_ACTIVATE)
      {
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 2, 0, 0, 0, 0, 0, 0, &td,
                                         "TARGET_CREATURE");
      }
      if (event == EVENT_RESOLVE_ACTIVATION)
      {

        instance = get_card_instance(player, card);
        source_player_or_id = instance->targets[0].player;
        c = instance->targets[0].card;
        if (valid_target(&td) != 0)
        {
          pump_ability_until_eot(player, card, source_player_or_id, c, 0, 0, 0x20, 0);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2183140; original cards/mercadian_masques.c:7420. */
int card_sailmonger(int player, int card, event_t event)
{
  /* Sailmonger	|3|U
   * Creature - Human Monger 3/3
   * |2: Target creature gains flying until end of turn. Any player may activate this ability. */

  bool legacy_found;
  int p;
  card_instance_t *instance;
  card_instance_t *legacy_record;
  int card_00;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    instance = in_play(player, card);
    if (((instance != NULL) && (card == affected_card)) && (player == affected_card_controller))
    {
      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        card_00 = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, card_00);

            if (((legacy_record != NULL) && (is_what(p, card_00, 0x80) != 0)) &&
                ((legacy_record = get_card_instance(p, card_00),
                  legacy_record->info_slot_fn == sailmonger_legacy &&
                      ((legacy_record->targets[0].player == player &&
                        (legacy_record->targets[0].card == card))))))
            {
              if (player == p)
              {
                kill_card(player, card_00, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  return 0;
                }
                legacy_found = true;
              }
            }
            card_00 = card_00 + 1;
          } while (card_00 <= active_cards_count[p]);
        }

        p = p + 1;
      } while (p != 2);
      if ((!legacy_found) && (instance->targets[9].card != 0x42))
      {
        create_legacy_activate_for_opponent(player, card, sailmonger_legacy, player, card);
      }
    }
  }
  else if ((((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
            (event == EVENT_RESOLVE_ACTIVATION)) ||
           (event == EVENT_CLEANUP))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
    {
      td.illegal_abilities = td.illegal_abilities | 0x20;
    }
    return vanilla_creature_pumper(player, card, event, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, &td);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2183380; original cards/mercadian_masques.c:7469. */
int card_saprazzan_bailiff(int player, int card, event_t event)
{
  /* Saprazzan Bailiff	|3|U|U
   * Creature - Merfolk 2/2
   * When this creature enters, exile all artifact and enchantment cards from all graveyards.
   * When this creature leaves the battlefield, return all artifact and enchantment cards from all graveyards to their owners' hands. */

  int count;
  int *graveyard;
  int i;
  int matches_type;
  unsigned int player_00;
  unsigned int pass;
  test_definition_t test;

  etb_ability(player, card, event, RESOLVE_TRIGGER_MANDATORY);
  if (event == EVENT_ETB)
  {

    pass = 0;
    do
    {
      player_00 = pass ^ current_turn;

      count = count_graveyard(player_00);
      graveyard = graveyard_ptr_mutable[player_00];
      i = count + -1;
      if (-1 < i)
      {
        do
        {
          matches_type = is_what(-1, graveyard[i], 0x40);
          if ((matches_type != 0) ||
              (matches_type = is_what(-1, graveyard[i], 4), matches_type != 0))
          {
            rfg_card_from_grave(player_00, i);
          }
          i = i + -1;
        } while (i != -1);
      }
      pass = pass + 1;
    } while (pass != 2);
    leaves_play(player, card, EVENT_ETB, 1, 0);
    return 0;
  }
  leaves_play(player, card, event, 1, 0);
  if (event == EVENT_LEAVES_PLAY_ABILITY)
  {

    new_default_test_definition(&test, 0x44, "");
    from_grave_to_hand_multiple(1 - current_turn, &test);
    from_grave_to_hand_multiple(current_turn, &test);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x2183530; original cards/mercadian_masques.c:7506. */
int card_saprazzan_cove(int player, int card, event_t event)
{
  /* Saprazzan Cove	""
   * Land
   * This land enters tapped.
   * |T: Put a storage counter on this land.
   * |T, Remove any number of storage counters from this land: Add |U for each storage counter removed this way. */

  int result;

  result = mercadian_storage_land(player, card, event, COLOR_BLUE);
  return result;
}

/* ManalinkEh 2022: 0x2183550; original cards/mercadian_masques.c:7517. */
int card_saprazzan_legate(int player, int card, event_t event)
{
  /* Saprazzan Legate	|3|U
   * Creature - Merfolk Soldier 1/3
   * If an opponent controls a Mountain and you control an Island, you may cast this spell without paying its mana cost.
   * Flying */

  int result;
  card_instance_t *instance;

  if (event != EVENT_MODIFY_COST)
  {
    if ((((event == EVENT_CAST_SPELL) && (card == affected_card)) &&
         (player == affected_card_controller)) &&
        (result = nemesis_freespell(player, card, 0x6c), result == 0))
    {
      spell_fizzled = 1;
    }
    return 0;
  }
  result = get_hacked_subtype(player, card, 0x5006);
  result = check_battlefield_for_subtype(1 - player, 1, result);
  if (result != 0)
  {
    result = get_hacked_subtype(player, card, 0x5002);
    result = check_battlefield_for_subtype(player, 1, result);
    if (result != 0)
    {
      null_casting_cost(player, card);
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      return 0;
    }
  }
  instance = get_card_instance(player, card);
  instance->info_slot = 0;
  return 0;
}

/* ManalinkEh 2022: 0x216ef60; original cards/mercadian_masques.c:7545. */
int put_this_on_top_of_owners_library_at_end_of_combat(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (end_of_combat_trigger(player, card, event, 2))
  {
    instance = get_card_instance(player, card);
    put_on_top_of_deck(instance->targets[0].player, instance->targets[0].card);
  }
  if ((((trigger_condition == TRIGGER_END_COMBAT) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((reason_for_trigger_controller == player && (event == EVENT_END_TRIGGER))))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2183670; original cards/mercadian_masques.c:7558. */
int card_saprazzan_outrigger(int player, int card, event_t event)
{
  /* Saprazzan Outrigger	|3|U
   * Creature - Merfolk 5/5
   * When this creature attacks or blocks, put it on top of its owner's library at end of combat. */

  dat_ability(player, card, event, DAT_RESOLVE_TRIGGER_MANDATORY, 0);
  if (event != EVENT_DAT_ABILITY &&
      (!blocking(player, card, event) || is_humiliated(player, card, AL_TRIG)))
    return 0;
  create_targetted_legacy_effect(player, card, put_this_on_top_of_owners_library_at_end_of_combat,
                                 player, card);
  return 0;
}

/* ManalinkEh 2022: 0x2183710; original cards/mercadian_masques.c:7571. */
int card_saprazzan_raider(int player, int card, event_t event)
{
  /* Saprazzan Raider	|2|U
   * Creature - Merfolk 1/2
   * When this creature becomes blocked, return it to its owner's hand. */

  int result;

  if ((event == EVENT_DECLARE_BLOCKERS) && (current_turn == player))
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_humiliated(player, card, AL_STATIC);
      if (result == 0)
      {
        result = is_unblocked(player, card);
        if (result == 0)
        {
          bounce_permanent(player, card);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216e070; original cards/mercadian_masques.c:7585. */
static int scandalmonger_effect(int player, int card, event_t event)
{
  int p;
  card_instance_t *instance;
  int source_player_or_cost;
  int source_card;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_cost = get_id(p, instance->targets[0].card);
        if (source_player_or_cost != 0x11f7)
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
        source_player_or_cost = instance->targets[0].player;
        source_card = instance->targets[0].card;
        default_target_definition(player, card, &td, 0);
        td.zone = TARGET_ZONE_PLAYERS;
        if (event == EVENT_CAN_ACTIVATE)
        {
          return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0x14, 2, 0, 0, 0, 0, 0,
                                           0, &td, "TARGET_PLAYER");
        }
        if (event == EVENT_ACTIVATE)
        {

          instance->number_of_targets = '\x01';
          source_player_or_cost = get_cost_mod_for_activated_abilities(
              source_player_or_cost, source_card, 2, 0, 0, 0, 0, 0);
          if (can_sorcery_be_played(player, EVENT_ACTIVATE) == 0)
          {
            return 0;
          }
          charge_mana(player, COLOR_COLORLESS, source_player_or_cost);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          new_pick_target(&td, "TARGET_PLAYER", 1, 1);
          return 0;
        }
      }
      else
      {
        if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
        {
          return 0;
        }
        default_target_definition(player, card, &td, 0);
      }
      td.zone = TARGET_ZONE_PLAYERS;
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          (source_player_or_cost = validate_target(player, card, &td, 1),
           source_player_or_cost != 0))
      {
        instance = get_card_instance(player, card);
        discard(instance->targets[1].player, 0, player);
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2183790; original cards/mercadian_masques.c:7637. */
int card_scandalmonger(int player, int card, event_t event)
{
  /* Scandalmonger	|3|B
   * Creature - Boar Monger 3/3
   * |2: Target player discards a card. Any player may activate this ability but only as a sorcery. */

  int p;
  int c;
  bool legacy_found;
  int created_or_valid;
  card_instance_t *legacy_record;
  card_instance_t *instance;
  int eligible_or_opponent;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if ((((legacy_record != NULL) &&
                  (eligible_or_opponent = is_what(p, c, 0x80), eligible_or_opponent != 0)) &&
                 (legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn == scandalmonger_effect)) &&
                ((legacy_record->targets[0].player == player &&
                  (legacy_record->targets[0].card == card))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  return 0;
                }
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if (!legacy_found)
      {

        eligible_or_opponent = 1 - player;
        created_or_valid = add_card_to_hand(eligible_or_opponent, instance->internal_card_id);
        instance = get_card_instance(
            eligible_or_opponent,
            create_legacy_activate(eligible_or_opponent, created_or_valid, scandalmonger_effect));
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        obliterate_card(eligible_or_opponent, created_or_valid);
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    default_target_definition(player, card, &td, 0);
    td.zone = TARGET_ZONE_PLAYERS;
    if ((event == EVENT_RESOLVE_ACTIVATION) &&
        (created_or_valid = valid_target(&td), created_or_valid != 0))
    {
      instance = get_card_instance(player, card);
      discard(instance->targets[0].player, 0, player);
    }
    return generic_activated_ability(player, card, event, 0x14, 2, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_PLAYER");
  }
  return 0;
}

/* ManalinkEh 2022: 0x2183a00; original cards/mercadian_masques.c:7696. */
int card_security_detail(int player, int card, event_t event)
{
  /* Security Detail	|3|W
   * Enchantment
   * |W|W: Create a 1/1 white Soldier creature token. Activate only if you control no creatures and only once each turn. */

  int result;
  token_generation_t token;
  if (event != EVENT_ACTIVATE)
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      result = global_enchantment(player, card, event);
      return result;
    }
    if (event == EVENT_RESOLVE_ACTIVATION)
    {
      default_token_definition(player, card, CARD_ID_SOLDIER, &token);
      generate_token(&token);
    }
  }
  result = check_battlefield_for_subtype(player, 2, -1);
  if (result != 0)
  {
    return 0;
  }
  return generic_activated_ability(player, card, event, 0x2000, 0, 0, 0, 0, 0, 2, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x216eeb0; original cards/mercadian_masques.c:7719. */
int destroy_target_blocking_creature_at_end_of_combat(int player, int card, event_t event)
{
  card_instance_t *instance;
  if (end_of_combat_trigger(player, card, event, 2))
  {
    instance = get_card_instance(player, card);
    kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);
  }
  if ((((trigger_condition == TRIGGER_END_COMBAT) && (card == affected_card)) &&
       (player == affected_card_controller)) &&
      ((reason_for_trigger_controller == player && (event == EVENT_END_TRIGGER))))
  {
    kill_card(player, card, KILL_EXILE);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2183b20; original cards/mercadian_masques.c:7733. */
int card_silent_assassin(int player, int card, event_t event)
{
  /* Silent Assassin	|B|B
   * Creature - Human Mercenary Assassin 2/1
   * |3|B: Destroy target blocking creature at end of combat. */

  card_instance_t *instance;
  int result;
  target_definition_t td1;

  if (event == EVENT_ACTIVATE)
  {
    default_target_definition(player, card, &td1, 2);
    td1.required_state = TARGET_STATE_BLOCKING;
    get_card_instance(player, card);
  }
  else
  {
    if ((1 < (uint32_t)(event - EVENT_RESOLVE_ACTIVATION)) && (event != EVENT_CLEANUP))
    {
      return 0;
    }
    default_target_definition(player, card, &td1, 2);
    td1.required_state = TARGET_STATE_BLOCKING;
    instance = get_card_instance(player, card);
    if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td1), result != 0))
    {
      create_targetted_legacy_effect(player, card,
                                     destroy_target_blocking_creature_at_end_of_combat,
                                     instance->targets[0].player, instance->targets[0].card);
    }
  }
  result = generic_activated_ability(player, card, event, 6, 3, 1, 0, 0, 0, 0, 0, &td1,
                                     "Select target blocking creature, it dies at end of combat.");
  return result;
}

/* ManalinkEh 2022: 0x2183c80; original cards/mercadian_masques.c:7759. */
int card_snake_pit(int player, int card, event_t event)
{
  /* Snake Pit	|3|G
   * Enchantment
   * Whenever an opponent casts a blue or black spell, you may create a 1/1 green Snake creature token. */

  int result;


  if (((trigger_condition == TRIGGER_SPELL_CAST) && (card == affected_card)) &&
      (player == affected_card_controller))
  {
    test_definition_t test;
    default_test_definition(&test, 0xff);
    test.color = get_sleighted_color_test(player, card, 6);
    result = duh_mode(player);
    sp_ability_trigger(1 - player, event, (result != 0) + 1, TYPE_NONE, &test,
                       player, card);
  }
  if (event == EVENT_SPA)
  {
    token_generation_t token;
    default_token_definition(player, card, CARD_ID_SNAKE, &token);
    generate_token(&token);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x2183d90; original cards/mercadian_masques.c:7781. */
int card_soothing_balm(int player, int card, event_t event)
{
  /* Soothing Balm	|1|W
   * Instant
   * Target player gains 5 life. */

  card_instance_t *instance;
  target_definition_t td;

  if (!IS_GS_EVENT(player, card, event))
  {
    return 0;
  }
  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
    {
      gain_life(instance->targets[0].player, 5);
    }
    kill_card(player, card, KILL_DESTROY);
  }
  return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2183ee0; original cards/mercadian_masques.c:7807. */
int card_soul_channeling(int player, int card, event_t event)
{
  /* Soul Channeling	|2|B
   * Enchantment - Aura
   * Enchant creature
   * Pay 2 life: Regenerate enchanted creature. */

  int t_card;
  char attached_player;
  card_instance_t *instance;
  int can_regenerate;

  instance = in_play(player, card);
  if ((instance != NULL) &&
      (instance = get_card_instance(player, card), instance->damage_target_player != -1))
  {
    if ((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE))
    {
      instance = get_card_instance(player, card);
      if (event != EVENT_CAN_ACTIVATE)
      {
        if (can_be_regenerated((int)instance->damage_target_player, instance->damage_target_card) ==
            0)
        {
          ai_modifier = ai_modifier + -0x60;
        }
        return generic_activated_ability(player, card, EVENT_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 2, NULL,
                                         NULL);
      }
      if (((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) != 0) &&
          (can_regenerate = can_be_regenerated((int)instance->damage_target_player,
                                               instance->damage_target_card),
           can_regenerate != 0))
      {
        if (generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 0, 0, 0, 0, 0, 0, 0, 2,
                                      NULL, NULL) == 0)
        {
          return 0;
        }
        return 99;
      }
    }
    else if ((event == EVENT_RESOLVE_ACTIVATION) || (event == EVENT_CLEANUP))
    {

      instance = get_card_instance(player, card);
      attached_player = instance->damage_target_player;
      t_card = instance->damage_target_card;
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          (can_be_regenerated((int)attached_player, t_card) != 0))
      {
        regenerate_target((int)attached_player, t_card);
      }
    }
  }
  return aura_pref_controller(player, card, event, player, NULL);
}
#endif

/* ManalinkEh 2022: 0x21840e0; original cards/mercadian_masques.c:7842. */
int card_specters_wail(int player, int card, event_t event)
{
  /* Specter's Wail	|1|B
   * Sorcery
   * Target player discards a card at random. */

  card_instance_t *instance;
  target_definition_t td;

  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST ||
      (event == EVENT_CAST_SPELL && card == affected_card && player == affected_card_controller))
    return generic_spell(player, card, event, 1, &td, "TARGET_PLAYER", 1, NULL);
  if (event == EVENT_RESOLVE_SPELL)
  {
    if (valid_target(&td))
      discard(instance->targets[0].player, 1, player);
    kill_card(player, card, KILL_DESTROY);
  }
  return 0;
}

/* ManalinkEh 2022: 0x216e370; original cards/mercadian_masques.c:7867. */
int effect_squallmonger(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int source_player_or_cost;
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_cost = get_id(p, instance->targets[0].card);
        if (source_player_or_cost != 0x120a)
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
      source_player_or_cost = instance->targets[0].player;
      target_card = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (can_use_activated_abilities(source_player_or_cost, target_card) != 0)
        {

          source_player_or_cost = get_cost_mod_for_activated_abilities(source_player_or_cost,
                                                                       target_card, 2, 0, 0, 0, 0, 0);
          if (has_mana_multi(player, source_player_or_cost, 0, 0, 0, 0, 0) != 0)
          {
            return 1;
          }
        }
      }
      else
      {
        if (event == EVENT_ACTIVATE)
        {

          charge_mana_multi(player,
                            get_cost_mod_for_activated_abilities(source_player_or_cost, target_card, 2,
                                                                 0, 0, 0, 0, 0),
                            0, 0, 0, 0, 0);
          return 0;
        }
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          default_test_definition(&this_test, 2);
          this_test.keyword = 0x20;
          new_damage_all(source_player_or_cost, target_card, 2, 1, 2, &this_test);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21841f0; original cards/mercadian_masques.c:7911. */
int card_squallmonger(int player, int card, event_t event)
{
  /* Squallmonger	|3|G
   * Creature - Monger 3/3
   * |2: This creature deals 1 damage to each creature with flying and each player. Any player may activate this ability. */

  int p;
  int c;
  bool legacy_found;
  card_instance_t *instance;
  int created_card;
  card_instance_t *legacy_record;
  int eligible_or_opponent;
  test_definition_t this_test;

  instance = get_card_instance(player, card);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      legacy_found = false;
      p = 0;
      do
      {

        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if ((((legacy_record != NULL) &&
                  (eligible_or_opponent = is_what(p, c, 0x80), eligible_or_opponent != 0)) &&
                 (legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn == effect_squallmonger)) &&
                ((legacy_record->targets[0].player == player &&
                  (legacy_record->targets[0].card == card))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0,
                                                   NULL, NULL);
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if (!legacy_found)
      {

        eligible_or_opponent = 1 - player;
        created_card = add_card_to_hand(eligible_or_opponent, instance->internal_card_id);
        instance = get_card_instance(
            eligible_or_opponent,
            create_legacy_activate(eligible_or_opponent, created_card, effect_squallmonger));
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        obliterate_card(eligible_or_opponent, created_card);
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_test_definition(&this_test, 2);
    this_test.keyword = 0x20;
    new_damage_all(instance->parent_controller, instance->parent_card, 2, 1, 2, &this_test);
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2184430; original cards/mercadian_masques.c:7965. */
int card_stamina(int player, int card, event_t event)
{
  /* Stamina	|2|G
   * Enchantment - Aura
   * Enchant creature
   * Enchanted creature has vigilance.
   * Sacrifice this Aura: Regenerate enchanted creature. */

  int attached_card_or_result;
  card_instance_t *instance;
  card_instance_t *aura_in_play;
  int attached_player_or_available;

  if (((event == EVENT_ACTIVATE) || ((uint32_t)(event - EVENT_RESOLVE_ACTIVATION) < 2)) ||
      (event == EVENT_CLEANUP))
  {

    instance = get_card_instance(player, card);
    aura_in_play = in_play(player, card);
    if (aura_in_play == NULL)
    {
      if (event == EVENT_CAN_ACTIVATE)
      {
        return 0;
      }
    }
    else
    {
      attached_player_or_available = (int)instance->damage_target_player;
      attached_card_or_result = instance->damage_target_card;
      if ((event == EVENT_RESOLVE_ACTIVATION) && (-1 < attached_player_or_available))
      {
        if (can_be_regenerated(attached_player_or_available, attached_card_or_result) != 0)
        {
          regenerate_target(attached_player_or_available, attached_card_or_result);
        }
      }
      else if (event == EVENT_CAN_ACTIVATE)
      {
        if (attached_player_or_available < 0)
        {
          return 0;
        }
        if ((((unsigned int)land_can_be_played >> 8 & 0xff) & 2) == 0)
        {
          return 0;
        }
        if (can_be_regenerated(attached_player_or_available, attached_card_or_result) == 0)
        {
          return 0;
        }
      }
    }
    attached_player_or_available =
        generic_activated_ability(player, card, event, 8, 0, 0, 0, 0, 0, 0, 0, NULL, NULL);
    attached_card_or_result = 99;
    if (attached_player_or_available == 0)
    {
      return 0;
    }
  }
  else
  {
    attached_gets_kw(player, card, event, KEYWORD_0, SP_KEYWORD_VIGILANCE);
    attached_card_or_result = aura_pref_controller(player, card, event, player, NULL);
  }
  return attached_card_or_result;
}
#endif

/* ManalinkEh 2022: 0x21845e0; original cards/mercadian_masques.c:7992. */
int card_stinging_barrier(int player, int card, event_t event)
{
  /* Stinging Barrier	|2|U|U
   * Creature - Wall 0/4
   * Defender (This creature can't attack.)
   * |U, |T: This creature deals 1 damage to any target. */

  if (event == EVENT_CHECK_PUMP)
  {
    if (!is_tapped(player, card) && !is_animated_and_sick(player, card) &&
        can_use_activated_abilities(player, card) &&
        has_mana_for_activated_ability(player, card, 0, 0, 1, 0, 0, 0))
      --pumpable_toughness[1 - player];
  }
  else if (IS_GAA_EVENT(event))
  {
    target_definition_t td;
    default_target_definition(player, card, &td, 0x102);
    td.zone = TARGET_ZONE_CREATURE_OR_PLAYER;
    if (event == EVENT_RESOLVE_ACTIVATION && valid_target(&td))
      damage_target0(player, card, 1);
    return generic_activated_ability(player, card, event, 5, 0, 0, 1, 0, 0, 0, 0, &td, "TARGET_ANY");
  }
  return 0;
}

/* ManalinkEh 2022: 0x2184780; original cards/mercadian_masques.c:8020. */
int card_strongarm_thug(int player, int card, event_t event)
{
  /* Strongarm Thug	|2|B
   * Creature - Human Mercenary 1/1
   * When this creature enters, you may return target Mercenary card from your graveyard to your hand. */

  int result;
  test_definition_t test;
  result = duh_mode(player);
  etb_ability(player, card, event, (result != 0) + RESOLVE_TRIGGER_OPTIONAL);
  if (event == EVENT_ETB)
  {
    new_default_test_definition(&test, 0x47, "Select target Mercenary card.");
    test.subtype = 0x73;
    result = new_special_count_grave(player, &test);
    if (result != 0)
    {
      result = graveyard_has_shroud(player);
      if (result == 0)
      {
        new_global_tutor(player, player, 2, 1, 0, 2, &test);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2184850; original cards/mercadian_masques.c:8041. */
int card_subterranean_hangar(int player, int card, event_t event)
{
  /* Subterranean Hangar	""
   * Land
   * This land enters tapped.
   * |T: Put a storage counter on this land.
   * |T, Remove any number of storage counters from this land: Add |B for each storage counter removed this way. */

  int result;

  result = mercadian_storage_land(player, card, event, COLOR_BLACK);
  return result;
}

/* ManalinkEh 2022: 0x2184870; original cards/mercadian_masques.c:8052. */
int card_sustenance(int player, int card, event_t event)
{
  /* Sustenance	|1|G
   * Enchantment
   * |1, Sacrifice a land: Target creature gets +1/+1 until end of turn. */

  card_instance_t *instance;
  unsigned int chosen_sacrifice;
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
      result = can_sacrifice_type_as_cost(player, 1, TYPE_LAND);
      if (result == 0)
      {
        return 0;
      }
      return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 1, 0, 0, 0, 0, 0, 0,
                                       &td, NULL);
    }
    if (event == EVENT_ACTIVATE)
    {
      new_default_test_definition(&test, 1, "Select a land to sacrifice.");
      chosen_sacrifice = new_sacrifice(player, card, player,
                                       SAC_RETURN_CHOICE | SAC_AS_COST | SAC_JUST_MARK, &test);
      if (chosen_sacrifice == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      generic_activated_ability(player, card, EVENT_ACTIVATE, 4, 1, 0, 0, 0, 0, 0, 0, &td,
                                "TARGET_CREATURE");
      player_00 = (int)chosen_sacrifice >> 0x10 & 0xff;
      if (spell_fizzled != 1)
      {
        kill_card(player_00, chosen_sacrifice >> 0x18, KILL_SACRIFICE);
        if (current_phase != 0x17)
        {
          return 0;
        }
        ai_modifier = ai_modifier + 0x18;
        return 0;
      }
      state_untargettable(player_00, chosen_sacrifice >> 0x18, 0);
      return 0;
    }
  }
  else
  {
    if ((event != EVENT_RESOLVE_ACTIVATION) && (event != EVENT_CLEANUP))
    {
      return global_enchantment(player, card, event);
    }
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    instance = get_card_instance(player, card);
  }
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    pump_ability_until_eot(instance->parent_controller, instance->parent_card,
                           instance->targets[0].player, instance->targets[0].card, 1, 1, 0, 0);
  }
  return 0;
}

/* ManalinkEh 2022: 0x2184b80; original cards/mercadian_masques.c:8109. */
int card_thunderclap(int player, int card, event_t event)
{
  /* Thunderclap	|2|R
   * Instant
   * You may sacrifice a Mountain rather than pay this spell's mana cost.
   * ~ deals 3 damage to target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td;
  test_definition_t test;

  if (event == EVENT_MODIFY_COST)
  {
    default_test_definition(&test, 0x47);
    test.subtype = 0x5006;
    result = new_can_sacrifice_as_cost(player, card, &test);
    if (result != 0)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      null_casting_cost(player, card);
      return 0;
    }
    instance = get_card_instance(player, card);
    instance->info_slot = 0;
    return 0;
  }
  if (event == EVENT_CAN_CAST)
  {
    default_target_definition(player, card, &td, 2);
    get_card_instance(player, card);
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if ((event != EVENT_RESOLVE_SPELL) && (event != EVENT_CAN_CHANGE_TARGET))
    {
      if (event != EVENT_CHANGE_TARGET)
      {
        return 0;
      }
      default_target_definition(player, card, &td, 2);
      get_card_instance(player, card);
      return 0;
    }
    default_target_definition(player, card, &td, 2);
    instance = get_card_instance(player, card);
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }
    result = valid_target(&td);
    if (result != 0)
    {
      damage_creature(instance->targets[0].player, instance->targets[0].card, 3, player, card);
    }
    kill_card(player, card, KILL_DESTROY);
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
  default_target_definition(player, card, &td, 2);
  instance = get_card_instance(player, card);
  if (card != affected_card)
  {
    return 0;
  }
  if (player != affected_card_controller)
  {
    return 0;
  }
  result = played_for_free(player, card);
  if (((result == 0) && (result = is_token(player, card), result == 0)) &&
      (instance->info_slot == 1))
  {

    result = has_mana_to_cast_iid(player, EVENT_CAST_SPELL, instance->internal_card_id);
    if (result != 0)
    {
      result = count_subtype(player, 1, -1);
      result =
          dialog_impl(player, card, EVENT_CAST_SPELL, DLG_RANDOM, DLG_NO_STORAGE,
                      "Sacrifice a mountain", 1, result * 2, "Cast normally", 1, 5, 0, 0, 0, 0);
      if (result != 1)
      {
        if (result != 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        result = get_id(player, card);
        charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
        if (spell_fizzled != 1)
          pick_target(&td, "TARGET_CREATURE");
        return 0;
      }
    }
    new_default_test_definition(&test, 0x47, "Select a Mountain to sacrifice.");
    test.subtype = 0x5006;
    new_sacrifice(player, card, player, SAC_AS_COST, &test);
  }
  if (spell_fizzled != 1)
  {
    pick_target(&td, "TARGET_CREATURE");
  }
  return 0;
}

/* ManalinkEh 2022: 0x2184f90; original cards/mercadian_masques.c:8179. */
int card_tidal_bore(int player, int card, event_t event)
{
  /* Tidal Bore	|1|U
   * Instant
   * You may return an Island you control to its owner's hand rather than pay this spell's mana cost.
   * You may tap or untap target creature. */

  card_instance_t *instance;
  int result;
  target_definition_t td2;
  target_definition_t td;

  if (event == EVENT_MODIFY_COST)
  {
    base_target_definition(player, card, &td, 1);
    td.required_subtype = 0x5002;
    td.allowed_controller = player;
    td.preferred_controller = player;
    result = target_available(player, card, &td);
    if (result < 1)
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 0;
    }
    else
    {
      instance = get_card_instance(player, card);
      instance->info_slot = 1;
      null_casting_cost(player, card);
    }
    default_target_definition(player, card, &td2, 2);
    get_card_instance(player, card);
    return 0;
  }
  default_target_definition(player, card, &td2, 2);
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td2, NULL, 1, NULL);
  }
  if (event != EVENT_CAST_SPELL)
  {
    if (event != EVENT_RESOLVE_SPELL)
    {
      return 0;
    }
    result = valid_target(&td2);
    if (result != 0)
    {
      twiddle(player, card, 0);
    }
    kill_card(player, card, KILL_DESTROY);
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
  instance->number_of_targets = '\0';
  result = played_for_free(player, card);
  if (((result == 0) && (result = is_token(player, card), result == 0)) &&
      (instance->info_slot == 1))
  {

    result = has_mana_to_cast_iid(player, EVENT_CAST_SPELL, instance->internal_card_id);
    if (result != 0)
    {

      result = count_subtype(player, 1, -1);
      result = do_dialog(player, player, card, -1, -1, " Bounce an Island\n Play normally\n Cancel",
                         (unsigned int)(result < 5));
      if (result != 0)
      {
        if (result != 1)
        {
          spell_fizzled = 1;
          return 0;
        }
        result = get_id(player, card);
        charge_mana_from_id(player, card, EVENT_CAST_SPELL, result);
        if ((spell_fizzled != 1) &&
            (generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td2,
                           "Select target creature to tap or untap.", 1, NULL),
             spell_fizzled != 1))
        {
          ai_modifier_twiddle(player, card, 0);
        }
        return 0;
      }
    }
    base_target_definition(player, card, &td, 1);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.required_subtype = 0x5002;
    result = new_pick_target(&td, "Select an Island you control to bounce to it\'s owner\'s hand.",
                             0, 0x81);
    if (result != 0)
    {
      bounce_permanent(instance->targets[0].player, instance->targets[0].card);
      instance->number_of_targets = '\0';
    }
  }
  if ((spell_fizzled != 1) && (generic_spell(player, card, EVENT_CAST_SPELL, 0x81, &td2,
                                             "Select target creature to tap or untap.", 1, NULL),
                               spell_fizzled != 1))
  {
    ai_modifier_twiddle(player, card, 0);
  }
  return 0;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2185340; original cards/mercadian_masques.c:8255. */
int card_tiger_claws(int player, int card, event_t event)
{
  /* Tiger Claws	|2|G
   * Enchantment - Aura
   * Flash
   * Enchant creature
   * Enchanted creature gets +1/+1 and has trample. */

  int result;

  attached_gets_pt(player, card, event, 1, 1);
  attached_gets_kw(player, card, event, KEYWORD_TRAMPLE, SP_KEYWORD_0);
  result = aura_pref_controller(player, card, event, player, NULL);
  return result;
}
#endif

/* ManalinkEh 2022: 0x21853c0; original cards/mercadian_masques.c:8269. */
int card_tonic_peddler(int player, int card, event_t event)
{
  /* Tonic Peddler	|1|W
   * Creature - Human Spellshaper 1/1
   * |W, |T, Discard a card: Target player gains 3 life. */

  card_instance_t *instance;
  int result;
  target_definition_t td;

  if (!IS_GAA_EVENT(event))
    return 0;

  default_target_definition(player, card, &td, 0);
  td.zone = TARGET_ZONE_PLAYERS;
  td.preferred_controller = player;
  instance = get_card_instance(player, card);
  if ((event == EVENT_RESOLVE_ACTIVATION) && (result = valid_target(&td), result != 0))
  {
    gain_life(instance->targets[0].player, 3);
  }
  result = generic_activated_ability(player, card, event, 0x25, 0, 0, 0, 0, 0, 1, 0, &td,
                                     "TARGET_PLAYER");
  return result;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2185510; original cards/mercadian_masques.c:8301. */
int card_unnatural_hunger(int player, int card, event_t event)
{
  /* Unnatural Hunger	|3|B|B
   * Enchantment - Aura
   * Enchant creature
   * At the beginning of the upkeep of enchanted creature's controller, this Aura deals damage equal to that creature's power to that player unless they sacrifice another creature of their choice. */

  card_instance_t *instance;
  int attached_card;
  int defending_life;
  int creature_power;
  int whose_upkeep;
  test_definition_t test;

  instance = in_play(player, card);
  if (instance != NULL)
  {

    instance = get_card_instance(player, card);
    whose_upkeep = (int)instance->damage_target_player;
    attached_card = instance->damage_target_card;
    if ((whose_upkeep != -1) && (attached_card != -1))
    {
      upkeep_trigger_ability(player, card, event, whose_upkeep);
      if ((event == EVENT_UPKEEP_TRIGGER_ABILITY) && (current_turn == whose_upkeep))
      {
        new_default_test_definition(&test, 2, "Select another creature to sacrifice");
        if (new_can_sacrifice(player, card, whose_upkeep, &test) != 0)
        {

          defending_life = life[whose_upkeep];
          creature_power = get_power(whose_upkeep, attached_card);
          if (do_dialog(whose_upkeep, player, card, -1, -1,
                        " Sac another creature or get damage\n Pass",
                        (unsigned int)(creature_power + 4 < defending_life)) == 0)
          {
            new_sacrifice(player, card, whose_upkeep, SAC_DONE | SAC_NO_CANCEL, &test);
            if (spell_fizzled != 1)
              return aura_pref_controller(player, card, event, 1 - player, NULL);
          }
        }
        damage_player(whose_upkeep, get_power(whose_upkeep, attached_card), player, card);
      }
    }
  }
  return aura_pref_controller(player, card, event, 1 - player, NULL);
}
#endif

/* ManalinkEh 2022: 0x216bd00; original cards/mercadian_masques.c:8344. */
static void destroy_creature_when_blocks_or_blocked(int player, int card, int t_player, int t_card)
{
  create_targetted_legacy_effect(player, card, die_at_end_of_combat, t_player, t_card);
  return;
}

/* ManalinkEh 2022: 0x21856c0; original cards/mercadian_masques.c:8349. */
int card_venomous_dragonfly(int player, int card, event_t event)
{
  /* Venomous Dragonfly	|3|G
   * Creature - Insect 1/1
   * Flying
   * Whenever this creature blocks or becomes blocked by a creature, destroy that creature at end of combat. */

  card_instance_t *instance;

  instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      if (!is_humiliated(player, card, AL_TRIG))
      {
        instance->destroys_if_blocked = instance->destroys_if_blocked | 0xc;
      }
    }
  }
  else if (event == EVENT_DECLARE_BLOCKERS)
  {
    if (!is_humiliated(player, card, AL_TRIG))
    {
      if ((current_turn == player) && ((instance->state & 4) != 0))
      {
        for_each_creature_blocking_me(current_turn, card, destroy_creature_when_blocks_or_blocked,
                                      current_turn, card);
      }
      if ((1 - current_turn == player) && ((instance->state & 8) != 0))
      {
        for_each_creature_blocked_by_me(player, card, destroy_creature_when_blocks_or_blocked,
                                        player, card);
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x216e890; original cards/mercadian_masques.c:8373. */
static int cast_creature_and_enchantment_spells_as_they_had_flash(int player, int card,
                                                                  event_t event)
{
  int i;
  int eligible_or_selected;
  card_instance_t *instance;
  int type_or_mana_available;
  test_definition_t this_test;

  eligible_or_selected = is_what(player, card, 0x80);
  if ((eligible_or_selected != 0) &&
      (instance = get_card_instance(player, card), -1 < instance->targets[0].player))
  {

    instance = get_card_instance(player, card);
    if (is_humiliated(instance->targets[0].player, instance->targets[0].card, AL_STATIC) != 0)
    {
      return 0;
    }
    other_leaves_play(player, card, instance->targets[0].player, instance->targets[0].card, event,
                      0, 0);
    if (event == EVENT_LEAVES_PLAY_ABILITY)
    {
      kill_card(player, card, KILL_EXILE);
      return 0;
    }
  }
  if (event == EVENT_CAN_ACTIVATE)
  {

    if (0 < active_cards_count[player])
    {
      i = 0;
      do
      {
        instance = in_hand(player, i);
        if ((instance != NULL) &&
            ((type_or_mana_available = is_what(player, i, 2),
              type_or_mana_available != 0 ||
                  (type_or_mana_available = is_what(player, i, 4), type_or_mana_available != 0))))
        {
          type_or_mana_available = has_mana_to_cast_iid(player, EVENT_CAN_ACTIVATE,
                                                        get_original_internal_card_id(player, i));
          if (type_or_mana_available != 0)
          {
            return 1;
          }
        }
        i = i + 1;
      } while (i < active_cards_count[player]);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {

    new_default_test_definition(&this_test, 6, "Select a creature or enchantment card to cast.");
    eligible_or_selected = new_select_a_card(player, player, 4, 0, 1, -1, &this_test);
    if (eligible_or_selected == -1)
    {
      spell_fizzled = 1;
    }
    else
    {
      if (charge_mana_from_id(player, eligible_or_selected, EVENT_ACTIVATE, -1) != 0)
      {
        play_card_in_hand_for_free(player, eligible_or_selected);
        cant_be_responded_to = 1;
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x21857c0; original cards/mercadian_masques.c:8414. */
int card_vernal_equinox(int player, int card, event_t event)
{
  /* Vernal Equinox	|3|G
   * Enchantment
   * Any player may cast creature and enchantment spells as though they had flash. */

  int p;
  int c;
  bool legacy_found;
  int created_or_humiliated;
  card_instance_t *instance;
  card_instance_t *legacy_record;
  int eligible_or_opponent;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if ((((legacy_record != NULL) &&
                  (eligible_or_opponent = is_what(p, c, 0x80), eligible_or_opponent != 0)) &&
                 (legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn ==
                      cast_creature_and_enchantment_spells_as_they_had_flash)) &&
                ((legacy_record->targets[0].player == player &&
                  (legacy_record->targets[0].card == card))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                  return global_enchantment(player, card, event);
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if (!legacy_found)
      {

        eligible_or_opponent = 1 - player;
        created_or_humiliated = add_card_to_hand(eligible_or_opponent, instance->internal_card_id);
        instance = get_card_instance(
            eligible_or_opponent,
            create_legacy_activate(eligible_or_opponent, created_or_humiliated,
                                   cast_creature_and_enchantment_spells_as_they_had_flash));
        instance->targets[0].card = card;
        instance->targets[0].player = player;
        instance->number_of_targets = '\x01';
        obliterate_card(eligible_or_opponent, created_or_humiliated);
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) &&
           (created_or_humiliated = is_humiliated(player, card, AL_STATIC),
            created_or_humiliated == 0))
  {
    return cast_creature_and_enchantment_spells_as_they_had_flash(player, card, event);
  }
  return global_enchantment(player, card, event);
}

/* ManalinkEh 2022: 0x21859a0; original cards/mercadian_masques.c:8465. */
int card_volcanic_wind(int player, int card, event_t event)
{
  /* Volcanic Wind	|4|R|R
   * Sorcery
   * ~ deals X damage divided as you choose among any number of target creatures, where X is the number of creatures on the battlefield as you cast this spell. */

  card_instance_t *instance;
  int damage_count;
  int ret_location;
  int selected_count;
  target_definition_t td;
  char msg[100];

  default_target_definition(player, card, &td, 2);
  if ((ai_is_speculating == 1) || ((player == 1 && (((unsigned char)trace_mode & 2) == 0))))
  {
    td.preferred_controller = 1 - player;
    td.illegal_abilities = get_protections_from(player, card);
    td.special = TARGET_SPECIAL_EXTRA_FUNCTION;
    td.extra = (intptr_t)not_indestructible;
  }
  instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
  {
    return generic_spell(player, card, EVENT_CAN_CAST, 1, &td, NULL, 1, NULL);
  }
  if (event == EVENT_CAST_SPELL)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      damage_count = count_subtype(2, 2, -1);
      instance->number_of_targets = '\0';
      instance->info_slot = damage_count;
      ai_modifier = ai_modifier + damage_count * 2;
      if (0 < damage_count)
      {
        ret_location = 0;
        do
        {
          selected_count = ret_location + 1;
          scnprintf(msg, 100, "Select target creature (%d of %d) to deal 1 damage.", selected_count,
                    damage_count);
          if (new_pick_target(&td, msg, ret_location, 0x80) == 0)
          {
            spell_fizzled = 1;
            selected_count = ret_location;
            break;
          }
          add_state(instance->targets[ret_location].player, instance->targets[ret_location].card,
                    0x200000);
          damage_count = instance->info_slot;
          ret_location = selected_count;
        } while ((int)(unsigned int)instance->number_of_targets < damage_count);
        for (int i = 0; i < selected_count; ++i)
          remove_state(instance->targets[i].player, instance->targets[i].card, 0x200000);
      }
    }
  }
  else if (event == EVENT_RESOLVE_SPELL)
  {
    divide_damage(player, card, &td);
    kill_card(player, card, KILL_DESTROY);
    return 0;
  }
  return 0;
}

/* ManalinkEh 2022: 0x216e600; original cards/mercadian_masques.c:8519. */
int effect_warmonger(int player, int card, event_t event)
{
  int p;
  int target_card;
  card_instance_t *instance;
  int source_player_or_cost;
  test_definition_t this_test;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      p = instance->targets[0].player;
      if (-1 < p)
      {
        source_player_or_cost = get_id(p, instance->targets[0].card);
        if (source_player_or_cost != 0x122d)
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
      source_player_or_cost = instance->targets[0].player;
      target_card = instance->targets[0].card;
      if (event == EVENT_CAN_ACTIVATE)
      {
        if (can_use_activated_abilities(source_player_or_cost, target_card) != 0)
        {

          source_player_or_cost = get_cost_mod_for_activated_abilities(source_player_or_cost,
                                                                       target_card, 2, 0, 0, 0, 0, 0);
          if (has_mana_multi(player, source_player_or_cost, 0, 0, 0, 0, 0) != 0)
          {
            return 1;
          }
        }
      }
      else
      {
        if (event == EVENT_ACTIVATE)
        {

          charge_mana_multi(player,
                            get_cost_mod_for_activated_abilities(source_player_or_cost, target_card, 2,
                                                                 0, 0, 0, 0, 0),
                            0, 0, 0, 0, 0);
          return 0;
        }
        if (event == EVENT_RESOLVE_ACTIVATION)
        {
          default_test_definition(&this_test, 2);
          this_test.keyword = 0x20;
          this_test.keyword_flag = 1;
          new_damage_all(source_player_or_cost, target_card, 2, 1, 2, &this_test);
        }
      }
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2185c10; original cards/mercadian_masques.c:8564. */
int card_warmonger(int player, int card, event_t event)
{
  /* Warmonger	|3|R
   * Creature - Minotaur Monger 3/3
   * |2: This creature deals 1 damage to each creature without flying and each player. Any player may activate this ability. */

  int p;
  int c;
  bool legacy_found;
  card_instance_t *instance;
  int created_card;
  card_instance_t *legacy_record;
  int eligible_or_opponent;
  test_definition_t this_test;

  instance = get_card_instance(player, card);
  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {
      legacy_found = false;
      p = 0;
      do
      {

        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if ((((legacy_record != NULL) &&
                  (eligible_or_opponent = is_what(p, c, 0x80), eligible_or_opponent != 0)) &&
                 (legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn == effect_warmonger)) &&
                ((legacy_record->targets[0].player == player &&
                  (legacy_record->targets[0].card == card))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0,
                                                   NULL, NULL);
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if (!legacy_found)
      {

        eligible_or_opponent = 1 - player;
        created_card = add_card_to_hand(eligible_or_opponent, instance->internal_card_id);
        instance = get_card_instance(
            eligible_or_opponent,
            create_legacy_activate(eligible_or_opponent, created_card, effect_warmonger));
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        obliterate_card(eligible_or_opponent, created_card);
      }
    }
  }
  else if (event == EVENT_RESOLVE_ACTIVATION)
  {
    default_test_definition(&this_test, 2);
    this_test.keyword = 0x20;
    this_test.keyword_flag = 1;
    new_damage_all(instance->parent_controller, instance->parent_card, 2, 1, 2, &this_test);
  }
  return generic_activated_ability(player, card, event, 0, 2, 0, 0, 0, 0, 0, 0, NULL, NULL);
}

/* ManalinkEh 2022: 0x216eb80; original cards/mercadian_masques.c:8619. */
static int are_blocked_creatures(int unused, int unused2, int player, int card)
{
  int result;
  result = is_what(player, card, 2);
  if (result != 0)
  {
    result = is_attacking(player, card);
    if (result != 0)
    {
      result = is_unblocked(player, card);
      return (unsigned int)(result == 0);
    }
  }
  return 0;
}

/* ManalinkEh 2022: 0x2185e60; original cards/mercadian_masques.c:8626. */
int card_warpath(int player, int card, event_t event)
{
  /* Warpath	|3|R
   * Instant
   * ~ deals 3 damage to each blocking creature and each blocked creature. */

  int result;
  test_definition_t this_test;
  test_definition_t this_test2;
  if (event == EVENT_RESOLVE_SPELL)
  {
    default_test_definition(&this_test, 2);
    this_test.special_selection_function = are_blocked_creatures;
    new_damage_all(player, card, 2, 3, 0, &this_test);
    default_test_definition(&this_test2, 2);
    this_test2.state = 8;
    new_damage_all(player, card, 2, 3, 0, &this_test2);
    kill_card(player, card, KILL_DESTROY);
  }
  result = basic_spell(player, card, event);
  return result;
}

/* ManalinkEh 2022: 0x216dd60; original cards/mercadian_masques.c:8648. */
static int wishmonger_effect(int player, int card, event_t event)
{
  int target_card;
  card_instance_t *instance;
  int result;
  unsigned int protection;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      instance = get_card_instance(player, card);
      result = instance->targets[0].player;
      if (-1 < result)
      {
        result = get_id(result, instance->targets[0].card);
        if (result != 0x1232)
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
        result = instance->targets[0].player;
        target_card = instance->targets[0].card;
        default_target_definition(player, card, &td, 2);
        td.preferred_controller = player;
        if (event == EVENT_CAN_ACTIVATE)
        {
          return generic_activated_ability(player, card, EVENT_CAN_ACTIVATE, 4, 2, 0, 0, 0, 0, 0, 0,
                                           &td, "TARGET_CREATURE");
        }
        if (event == EVENT_ACTIVATE)
        {

          instance->number_of_targets = '\x01';
          result = get_cost_mod_for_activated_abilities(result, target_card, 2, 0, 0, 0, 0, 0);
          charge_mana(player, COLOR_COLORLESS, result);
          if (spell_fizzled == 1)
          {
            return 0;
          }
          new_pick_target(&td, "TARGET_CREATURE", 1, 1);
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
      }
      td.preferred_controller = player;
      if ((event == EVENT_RESOLVE_ACTIVATION) &&
          (result = validate_target(player, card, &td, 1), result != 0))
      {

        protection = select_a_protection(player);
        pump_ability_until_eot(player, card, instance->targets[1].player, instance->targets[1].card,
                               0, 0, protection | 0x10000000, 0);
      }
    }
  }

  return 0;
}

/* ManalinkEh 2022: 0x2185f50; original cards/mercadian_masques.c:8698. */
int card_wishmonger(int player, int card, event_t event)
{
  /* Wishmonger	|3|W
   * Creature - Unicorn Monger 3/3
   * |2: Target creature gains protection from the color of its controller's choice until end of turn. Any player may activate this ability. */

  int p;
  int c;
  bool legacy_found;
  int created_card;
  card_instance_t *legacy_record;
  card_instance_t *instance;
  unsigned int protection;
  int eligible_or_opponent;
  target_definition_t td;

  if (event == EVENT_STATIC_EFFECTS)
  {
    if ((card == affected_card) && (player == affected_card_controller))
    {

      p = 0;
      instance = get_card_instance(player, card);
      legacy_found = false;
      do
      {
        c = 0;
        if (-1 < active_cards_count[p])
        {
          do
          {
            legacy_record = in_play(p, c);

            if ((((legacy_record != NULL) &&
                  (eligible_or_opponent = is_what(p, c, 0x80), eligible_or_opponent != 0)) &&
                 (legacy_record = get_card_instance(p, c),
                  legacy_record->info_slot_fn == wishmonger_effect)) &&
                ((legacy_record->targets[0].player == player &&
                  (legacy_record->targets[0].card == card))))
            {
              if (player == p)
              {
                kill_card(player, c, KILL_EXILE);
              }
              else
              {
                if (p == 1)
                {
                  return 0;
                }
                legacy_found = true;
              }
            }
            c = c + 1;
          } while (c <= active_cards_count[p]);
        }
        p = p + 1;
      } while (p != 2);
      if (!legacy_found)
      {

        eligible_or_opponent = 1 - player;
        created_card = add_card_to_hand(eligible_or_opponent, instance->internal_card_id);
        instance = get_card_instance(
            eligible_or_opponent,
            create_legacy_activate(eligible_or_opponent, created_card, wishmonger_effect));
        instance->targets[0].player = player;
        instance->targets[0].card = card;
        instance->number_of_targets = '\x01';
        obliterate_card(eligible_or_opponent, created_card);
      }
    }
  }
  else if (((event == EVENT_CAN_ACTIVATE) || (event == EVENT_ACTIVATE)) ||
           ((event == EVENT_RESOLVE_ACTIVATION || (event == EVENT_CLEANUP))))
  {
    default_target_definition(player, card, &td, 2);
    td.preferred_controller = player;
    if (event == EVENT_RESOLVE_ACTIVATION)
    {

      instance = get_card_instance(player, card);
      if (valid_target(&td) != 0)
      {

        protection = select_a_protection(player);
        pump_ability_until_eot(player, card, instance->targets[0].player, instance->targets[0].card,
                               0, 0, protection | 0x10000000, 0);
      }
    }
    return generic_activated_ability(player, card, event, 4, 2, 0, 0, 0, 0, 0, 0, &td,
                                     "TARGET_CREATURE");
  }
  return 0;
}

/* ManalinkEh 2022: 0x21861f0; original cards/mercadian_masques.c:8759. */
int card_worry_beads(int player, int card, event_t event)
{
  /* Worry Beads	|3
   * Artifact
   * At the beginning of each player's upkeep, that player mills a card. */

  upkeep_trigger_ability(player, card, event, 2);
  if (event != EVENT_UPKEEP_TRIGGER_ABILITY)
  {
    return 0;
  }
  mill(current_turn, 1);
  return 0;
}
