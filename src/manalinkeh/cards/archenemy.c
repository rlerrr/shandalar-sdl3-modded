#include "manalink.h"

#define SCHEME_DECK_SIZE 20

static int* schemes = NULL;
static int num_schemes = 0;

static int arch_deck[2][SCHEME_DECK_SIZE];

int create_archenemy_legacy(int player, int card, int csvid, int (*func_ptr)(int, int, event_t), int t_player, int t_card)
{
  int leg;
  if (t_player >= 0 && t_card >= 0)
	leg = create_targetted_legacy_effect(player, card, func_ptr, t_player, t_card);
  else
	leg = create_legacy_effect(player, card, func_ptr);

  if (leg != -1)
	{
	  card_instance_t* legacy = get_card_instance(player, leg);
	  legacy->display_pic_csv_id = csvid;
	  legacy->display_pic_num = get_card_image_number(csvid, player, card);
	}

  return leg;
}

static int effect_a_display_of_my_dark_power(int player, int card, event_t event)
{
  if (event == EVENT_BEGIN_TURN && current_turn == player)
	kill_card(player, card, KILL_EXILE);

  return card_mana_flare(player, card, event);
}
static int scheme_a_display_of_my_dark_power(int player, int card, event_t event)
{
  /* A Display of My Dark Power	""
   * Scheme
   * When you set this scheme in motion, until your next turn, whenever a player taps a land for mana, that player adds one mana of any type that land produced. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_a_display_of_my_dark_power, -1, -1);
  return 0;
}

// Does nothing the turn it's put in play.  Otherwise removed at end of controller's turn.
static int effect_all_in_good_time(int player, int card, event_t event)
{
  if (event == EVENT_CLEANUP && affect_me(player, card) && current_turn == player)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  if (instance->targets[1].card == 42)
		kill_card(player, card, KILL_EXILE);
	  else
		instance->targets[1].card = 42;
	}

  return 0;
}
static int scheme_all_in_good_time(int player, int card, event_t event)
{
  /* All in Good Time	""
   * Scheme
   * When you set this scheme in motion, take an extra turn after this one. Schemes can't be set in motion that turn. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  time_walk_effect(player, card);
	  create_archenemy_legacy(player, card, event_result, effect_all_in_good_time, -1, -1);
	}
  return 0;
}

static int scheme_all_shall_smolder_in_my_wake(int player, int card, event_t event)
{
  /* All Shall Smolder in My Wake	""
   * Scheme
   * When you set this scheme in motion, destroy up to one target artifact, up to one target enchantment, and up to one target nonbasic land. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_ARTIFACT);
	  if (IS_AI(player))
		td.allowed_controller = 1-player;

	  if (can_target(&td) && pick_target(&td, "TARGET_ARTIFACT"))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  instance->number_of_targets = 0;
	  default_target_definition(player, card, &td, TYPE_ENCHANTMENT);
	  if (IS_AI(player))
		td.allowed_controller = 1-player;

	  if (can_target(&td) && pick_target(&td, "TARGET_ENCHANTMENT"))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  instance->number_of_targets = 0;
	  if (pick_target_nonbasic_land(player, card, 1))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_DESTROY);

	  instance->number_of_targets = 0;
	}
  return 0;
}

static int effect_approach_my_molten_realm(int player, int card, event_t event)
{
  if (event == EVENT_BEGIN_TURN && current_turn == player)
	kill_card(player, card, KILL_EXILE);

  card_instance_t* damage = damage_being_dealt(event);
  if (damage)
	damage->info_slot *= 2;

  return 0;
}
static int scheme_approach_my_molten_realm(int player, int card, event_t event)
{
  /* Approach My Molten Realm	""
   * Scheme
   * When you set this scheme in motion, until your next turn, if a source would deal damage, it deals double that damage instead. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_approach_my_molten_realm, -1, -1);
  return 0;
}

static int scheme_behold_the_power_of_destruction(int player, int card, event_t event)
{
  /* Behold the Power of Destruction	""
   * Scheme
   * When you set this scheme in motion, destroy all nonland permanents target opponent controls. */

  if (event == EVENT_SET_IN_MOTION && target_opponent(player, card))
	{
	  int c, num_tgts = 0, tgts[150];

	  for (c = 0; c < active_cards_count[1-player]; ++c)
		if (in_play(1-player, c) && is_what(1-player, c, TYPE_PERMANENT) && !is_what(1-player, c, TYPE_LAND))
		  tgts[num_tgts++] = c;

	  for (c = 0; c < num_tgts; ++c)
		kill_card(1-player, tgts[c], KILL_DESTROY);
	}
  return 0;
}

/* Choose Your Champion	""
 * Scheme
 * When you set this scheme in motion, target opponent chooses a player. Until your next turn, only you and the chosen player can cast spells and attack with creatures. */
// Meaningless in two-player, unless your opponent is especially dumb.

static int scheme_dance_pathetic_marionette(int player, int card, event_t event)
{
  /* Dance, Pathetic Marionette	""
   * Scheme
   * When you set this scheme in motion, each opponent reveals cards from the top of their library until they reveal a creature card. Choose one of the revealed creature cards and put it onto the battlefield under your control. Put all other cards revealed this way into their owners' graveyards. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  int c;
	  for (c = 0; deck_ptr[1-player][c] != -1 && !is_what(-1, deck_ptr[1-player][c], TYPE_CREATURE); ++c)
		{}

	  int iid = deck_ptr[1-player][c];

	  if (iid == -1)
		--c;

	  if (c >= 0)
		show_deck(HUMAN, deck_ptr[1-player], c + 1, "Revealed these cards", 0, 0x7375B0);

	  if (iid != -1)
		remove_card_from_deck(1-player, c);

	  if (c > 0)
		mill(1-player, c);

	  if (iid != -1)
		{
		  c = add_card_to_hand(player, iid);
		  get_card_instance(player, c)->state ^= STATE_OWNED_BY_OPPONENT;
		  put_into_play(player, c);
		}
	}
  return 0;
}

/* Drench the Soil in Their Blood	""
 * Scheme
 * When you set this scheme in motion, after this main phase, there is an additional combat phase followed by an additional main phase. Creatures you control gain vigilance until end of turn. */
// Additional combats don't really work in Manalink

static int scheme_embrace_my_diabolical_vision(int player, int card, event_t event)
{
  /* Embrace My Diabolical Vision	""
   * Scheme
   * When you set this scheme in motion, each player shuffles their hand and graveyard into their library. You draw seven cards, then each other player draws four cards. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  int p;
	  for (p = 0; p < 2; ++p)
		{
		  reshuffle_grave_into_deck(p, 1);
		  reshuffle_hand_into_deck(p, 0);
		}
	  draw_cards(player, 7);
	  draw_cards(1-player, 4);
	}
  return 0;
}

static int scheme_every_hope_shall_vanish(int player, int card, event_t event)
{
  /* Every Hope Shall Vanish	""
   * Scheme
   * When you set this scheme in motion, each opponent reveals their hand. Choose a nonland card from each of those hands. Those players discard those cards. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  test_definition_t this_test;
	  default_test_definition(&this_test, TYPE_LAND);
	  this_test.type_flag = DOESNT_MATCH;

	  ec_definition_t this_definition;
	  default_ec_definition(1-player, player, &this_definition);

	  new_effect_coercion(&this_definition, &this_test);
	}
  return 0;
}

static int scheme_every_last_vestige_shall_rot(int player, int card, event_t event)
{
  /* Every Last Vestige Shall Rot	""
   * Scheme
   * When you set this scheme in motion, you may pay |X. If you do, put each nonland permanent target player controls with converted mana cost X or less on the bottom of its owner's library. */

  if (event == EVENT_SET_IN_MOTION && target_opponent(player, card))
	{
	  int spent = -1;
	  if (IS_AI(player))
		{
		  int max_cmc = get_highest_cmc_nonland(1-player);
		  if (has_mana(player, COLOR_ANY, max_cmc))
			{
			  charge_mana(player, COLOR_COLORLESS, max_cmc);
			  if (cancel != 1)
				spent = max_cmc;
			}
		}

	  if (spent == -1)
		{
		  charge_mana(player, COLOR_COLORLESS, -1);
		  spent = x_value;
		}

	  manipulate_all(player, card, 1-player, TYPE_LAND, DOESNT_MATCH, 0, 0, 0, 0, 0, 0, spent+1,
					 F5_CMC_LESSER_THAN_VALUE, ACT_PUT_ON_BOTTOM);
	}
  return 0;
}

static int scheme_evil_comes_to_fruition(int player, int card, event_t event)
{
  /* Evil Comes to Fruition	""
   * Scheme
   * When you set this scheme in motion, create seven 0/1 |Sgreen Plant creature tokens. If you control ten or more lands, create seven 3/3 |Sgreen Elemental creature tokens instead. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  if (count_subtype(player, TYPE_LAND, -1) >= 10)
		{
		  token_generation_t token;
		  default_token_definition(player, card, CARD_ID_ELEMENTAL, &token);
		  token.qty = 7;
		  token.pow = 3;
		  token.tou = 3;
		  token.color_forced = COLOR_TEST_GREEN;
		  token.no_sleight = 1;
		  generate_token(&token);
		}
	  else
		generate_tokens_by_id(player, card, CARD_ID_PLANT, 7);
	}
  return 0;
}

static int scheme_feed_the_machine(int player, int card, event_t event)
{
  /* Feed the Machine	""
   * Scheme
   * When you set this scheme in motion, target opponent chooses self or others. If that player chooses self, the player sacrifices two creatures. If the player chooses others, each of your other opponents sacrifices a creature. */

  if (event == EVENT_SET_IN_MOTION && target_opponent(player, card))
	impose_sacrifice(player, card, 1-player, 2, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0);
  return 0;
}

/* I Bask in Your Silent Awe	""
 * Ongoing Scheme
 * Each opponent can't cast more than one spell each turn.
 * At the beginning of your upkeep, if no opponent cast a spell since your last turn ended, abandon this scheme. */
/* Doable.  Complex, since it needs to handle EVENT_MODIFY_COST_GLOBAL on the original Archenemy card, but for a
 * different player than The Pieces Are Coming Together, which will break get_updated_casting_cost().  Plus, lack of
 * full artwork makes the card look particularly ugly.  Declining to implement this for now. */

static int scheme_i_call_on_the_ancient_magics(int player, int card, event_t event)
{
  /* I Call on the Ancient Magics	""
   * Scheme
   * When you set this scheme in motion, each other player searches their library for a card, reveals it, and puts it into their hand. Then you search your library for two cards and put them into your hand. Each player shuffles their library. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  test_definition_t test;
	  default_test_definition(&test, 0);
	  test.no_shuffle = 1;

	  int card_added = new_global_tutor(1-player, 1-player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
	  if (card_added != -1)
		reveal_card(1-player, card_added, -1, -1);

	  test.qty = 2;
	  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);

	  shuffle(player);
	  shuffle(1-player);
	}
  return 0;
}

static int scheme_i_delight_in_your_convulsions(int player, int card, event_t event)
{
  /* I Delight in Your Convulsions	""
   * Scheme
   * When you set this scheme in motion, each opponent loses 3 life. You gain life equal to the life lost this way. */

  if (event == EVENT_SET_IN_MOTION)
	gain_life(player, lose_life(1-player, 3));
  return 0;
}

static int effect_i_know_all_i_see_all(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);

  untap_permanents_during_opponents_untap(player, card, 0, &instance->targets[2].player);

  if (trigger_condition == TRIGGER_EOT
	  && get_trap_condition(player, TRAP_CARDS_TO_GY_FROM_ANYWHERE) >= 3
	  && eot_trigger(player, card, event))
	kill_card(player, card, KILL_EXILE);

  return 0;
}
static int scheme_i_know_all_i_see_all(int player, int card, event_t event)
{
  /* I Know All, I See All	""
   * Ongoing Scheme
   * Untap all permanents you control during each opponent's untap step.
   * At the beginning of each end step, if three or more cards were put into your graveyard this turn from anywhere, abandon this scheme. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_i_know_all_i_see_all, -1, -1);
  return 0;
}

static int scheme_ignite_the_cloneforge(int player, int card, event_t event)
{
  /* Ignite the Cloneforge!	""
   * Scheme
   * When you set this scheme in motion, create a token that's a copy of target permanent an opponent controls. */
  if (event == EVENT_SET_IN_MOTION)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.allow_cancel = 0;
	  td.allowed_controller = 1-player;
	  td.preferred_controller = 1-player;

	  if (can_target(&td) && pick_target(&td, "TARGET_PERMANENT"))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  copy_token(player, card, instance->targets[0].player, instance->targets[0].card);
		}
	}
  return 0;
}

static int effect_imprison_this_insolent_wretch(int player, int card, event_t event)
{
  // Permanents the chosen player controls don't untap during his or her untap step.
  if (event == EVENT_UNTAP && current_phase == PHASE_UNTAP && affected_card_controller == 1-player)
	get_card_instance(affected_card_controller, affected_card)->untap_status &= ~3;

  // When the chosen player is attacked or becomes the target of a spell or ability, abandon this scheme.
  if (declare_attackers_trigger(player, card, event, DAT_ATTACKS_PLAYER, player, -1))
	kill_card(player, card, KILL_EXILE);

  if (becomes_target_of_spell_or_effect(player, card, event, 1-player, -1, ANYBODY))
	kill_card(player, card, KILL_EXILE);

  return 0;
}
static int scheme_imprison_this_insolent_wretch(int player, int card, event_t event)
{
  /* Imprison This Insolent Wretch	""
   * Ongoing Scheme
   * When you set this scheme in motion, choose an opponent.
   * Permanents the chosen player controls don't untap during their untap step.
   * When the chosen player is attacked or becomes the target of a spell or ability, abandon this scheme. */
  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_imprison_this_insolent_wretch, -1, -1);
 return 0;
}

static int scheme_into_the_earthen_maw(int player, int card, event_t event)
{
  /* Into the Earthen Maw	""
   * Scheme
   * When you set this scheme in motion, exile up to one target creature with flying, up to one target creature without flying, and all cards from up to one target opponent's graveyard. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  if (IS_AI(player))
		td.allowed_controller = 1-player;

	  td.required_abilities = KEYWORD_FLYING;
	  if (can_target(&td) && pick_target(&td, "TARGET_FLYING"))
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
	  cancel = 0;

	  td.required_abilities = 0;
	  td.illegal_abilities |= KEYWORD_FLYING;
	  if (can_target(&td) && pick_target(&td, "FLOOD"))	// Select target creature without flying
		kill_card(instance->targets[0].player, instance->targets[0].card, KILL_EXILE);
	  cancel = 0;

	  default_target_definition(player, card, &td, 0);
	  td.zone = TARGET_ZONE_PLAYERS;
	  td.allowed_controller = 1-player;
	  if (can_target(&td) && pick_target(&td, "TARGET_OPPONENT"))	// since it's optional
		rfg_whole_graveyard(instance->targets[0].player);
	}
  return 0;
}

static int scheme_introductions_are_in_order(int player, int card, event_t event)
{
  /* Introductions Are in Order	""
   * Scheme
   * When you set this scheme in motion, choose one -
   * * Search your library for a creature card, reveal it, put it into your hand, then shuffle your library.
   * * You may put a creature card from your hand onto the battlefield. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = player;
	  td.preferred_controller = player;
	  td.illegal_abilities = 0;
	  td.zone = TARGET_ZONE_HAND;

	  int choice = DIALOG(player, card, EVENT_ACTIVATE,
						  DLG_FULLCARD_CSVID(event_result),
						  DLG_NO_STORAGE,
						  DLG_NO_CANCEL,
						  "Search for a creature", 1, 1,
						  "Put a creature in play", 1, can_target(&td) ? 2 : -1);

	  if (choice == 1)
		{
		  int card_added = global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, TYPE_CREATURE, MATCH, 0, 0, 0, 0, 0, 0, -1, 0);
		  if (player == AI && card_added != -1)
			DIALOG(player, card, EVENT_ACTIVATE,
				   DLG_FULLCARD(player, card_added),
				   DLG_MSG("Opponent selected"));
		}
	  else if (pick_target(&td, "SELECT_CREATURE"))// choice == 2
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  put_into_play(instance->targets[0].player, instance->targets[0].card);
		}
	}
  return 0;
}

static int scheme_know_naught_but_fire(int player, int card, event_t event)
{
  /* Know Naught but Fire	""
   * Scheme
   * When you set this scheme in motion, it deals damage to each opponent equal to the number of cards in that player's hand. */

  if (event == EVENT_SET_IN_MOTION)
	damage_player(1-player, hand_count[1-player], player, card);
  return 0;
}

static int scheme_look_skyward_and_despair(int player, int card, event_t event)
{
  /* Look Skyward and Despair	""
   * Scheme
   * When you set this scheme in motion, create a 5/5 |Sred Dragon creature token with flying. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_DRAGON, &token);
	  token.color_forced = COLOR_TEST_RED;
	  token.pow = 5;
	  token.tou = 5;
	  generate_token(&token);
	}
  return 0;
}

static int scheme_may_civilization_collapse(int player, int card, event_t event)
{
  /* May Civilization Collapse	""
   * Scheme
   * When you set this scheme in motion, target opponent chooses self or others. If that player chooses self, that player sacrifices two lands. If the player chooses others, each of your other opponents sacrifices a land. */

  if (event == EVENT_SET_IN_MOTION && target_opponent(player, card))
	impose_sacrifice(player, card, 1-player, 2, TYPE_LAND, 0, 0, 0, 0, 0, 0, 0, -1, 0);
  return 0;
}

/* Mortal Flesh Is Weak	""
 * Scheme
 * When you set this scheme in motion, each opponent's life total becomes the lowest life total among your opponents. */
// Meaningless in two-player.

static int scheme_my_crushing_masterstroke(int player, int card, event_t event)
{
  /* My Crushing Masterstroke	""
   * Scheme
   * When you set this scheme in motion, gain control of all nonland permanents your opponents control until end of turn. Untap those permanents. They gain haste until end of turn. Each of them attacks its owner this turn if able. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  int c;
	  for (c = 0; c < active_cards_count[1-player]; ++c)
		if (in_play(1-player, c) && is_what(1-player, c, TYPE_PERMANENT) && !is_what(1-player, c, TYPE_LAND))
		  {
			int leg = effect_act_of_treason(player, card, 1-player, c);
			if (leg != -1)
			  {
				card_instance_t* legacy = get_card_instance(player, leg);
				legacy->display_pic_csv_id = event_result;
				legacy->display_pic_num = get_card_image_number(event_result, player, card);

				legacy->targets[1].player |= SP_KEYWORD_MUST_ATTACK;
			  }
		  }
	}
  return 0;
}

static int scheme_my_genius_knows_no_bounds(int player, int card, event_t event)
{
  /* My Genius Knows No Bounds	""
   * Scheme
   * When you set this scheme in motion, you may pay |X. If you do, you gain X life and draw X cards. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  int spent;
	  if (IS_AI(player))
		{
		  int avail = has_mana(player, COLOR_ANY, 1);
		  avail = MIN(avail, 12 - hand_count[player]);		// Don't draw past 12 cards in hand
		  avail = MIN(avail, count_deck(player) - 3);	// Don't draw any of last 3 cards in library
		  for (; avail > 0; --avail)
			{
			  cancel = 0;
			  charge_mana(player, COLOR_COLORLESS, avail);
			  if (cancel != 1)	// Just in case mana was counted wrong, e.g. by multiple Gemhide Slivers
				break;
			}
		  spent = avail;
		}
	  else
		{
		  charge_mana(player, COLOR_COLORLESS, -1);
		  if (cancel == 1)
			spent = 0;
		  else
			spent = x_value;
		}

	  if (spent > 0)
		{
		  gain_life(player, spent);
		  draw_cards(player, spent);
		}
	}
  return 0;
}

static int effect_my_undead_horde_awakens_target(int player, int card, event_t event)
{
  card_instance_t* instance = get_card_instance(player, card);
  if (graveyard_from_play(instance->damage_target_player, instance->damage_target_card, event))
	{
	  kill_card(instance->targets[1].player, instance->targets[1].card, KILL_EXILE);

	  // Destroy this and other effects from the same scheme
	  int p, c;
	  for (p = 0; p < 2; ++p)
		for (c = 0; c < active_cards_count[p]; ++c)
		  if (in_play(p, c))
			{
			  card_instance_t* eff = get_card_instance(p, c);
			  if (eff->internal_card_id == LEGACY_EFFECT_CUSTOM
				  && eff->info_slot == (int)effect_my_undead_horde_awakens_target
				  && eff->targets[1].player == instance->targets[1].player
				  && eff->targets[1].card == instance->targets[1].card)
				kill_card(p, c, KILL_EXILE);
			}
	}

  return 0;
}
static int effect_my_undead_horde_awakens(int player, int card, event_t event)
{
  if (current_turn == player && eot_trigger(player, card, event))
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select target creature card.");
	  int result = new_global_tutor(player, 1-player, TUTOR_FROM_GRAVE, TUTOR_PLAY, 0, AI_MAX_CMC, &test);
	  if (result != -1)
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  int leg = create_archenemy_legacy(instance->damage_source_player, instance->damage_source_card,
											CARD_ID_MY_UNDEAD_HORDE_AWAKENS, &effect_my_undead_horde_awakens_target,
											player, result);
		  if (leg != -1)
			{
			  card_instance_t* legacy = get_card_instance(player, leg);
			  legacy->targets[1].player = player;
			  legacy->targets[1].card = card;
			}
		}
	}

  return 0;
}
static int scheme_my_undead_horde_awakens(int player, int card, event_t event)
{
  /* My Undead Horde Awakens	""
   * Ongoing Scheme
   * At the beginning of your end step, you may put target creature card from an opponent's graveyard onto the battlefield under your control.
   * When a creature put onto the battlefield with this scheme dies, abandon this scheme. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_my_undead_horde_awakens, -1, -1);
  return 0;
}

static int scheme_my_wish_is_your_command(int player, int card, event_t event)
{
  /* My Wish Is Your Command	""
   * Scheme
   * When you set this scheme in motion, each opponent reveals their hand. You may choose a noncreature, nonland card revealed this way and cast it without paying its mana cost. */

  if (event == EVENT_SET_IN_MOTION && hand_count[1-player] > 0)
	{
	  test_definition_t this_test;
	  new_default_test_definition(&this_test, TYPE_CREATURE | TYPE_LAND, "Select a noncreature, nonland card.");
	  this_test.type_flag = DOESNT_MATCH;
	  int selected = new_select_a_card(player, 1-player, TUTOR_FROM_HAND, 0, AI_MAX_CMC, -1, &this_test);
	  if (selected != -1)
		opponent_plays_card_in_your_hand_for_free(1-player, selected);
	}
  return 0;
}

static int scheme_nature_demands_an_offering(int player, int card, event_t event)
{
  /* Nature Demands an Offering	""
   * Scheme
   * When you set this scheme in motion, target opponent chooses a creature you don't control and puts it on top of its owner's library, then repeats this process for an artifact, an enchantment, and a land. Then the owner of each permanent chosen this way shuffles their library. */

  if (event == EVENT_SET_IN_MOTION && target_opponent(player, card))
	{
	  int typs[4] = { TYPE_CREATURE, TYPE_ARTIFACT, TYPE_ENCHANTMENT, TYPE_LAND };
	  const char* prompts[4] = { "TARGET_CREATURE", "TARGET_ARTIFACT", "TARGET_ENCHANTMENT", "TARGET_LAND" };
	  int who_shuffles = 0;

	  int i;
	  for (i = 0; i < 4; ++i)
		{
		  target_definition_t td;
		  default_target_definition(player, card, &td, typs[i]);
		  td.allowed_controller = 1 - player;
		  td.preferred_controller = 1 - player;
		  td.who_chooses = 1 - player;
		  td.allow_cancel = 0;
		  td.illegal_abilities = 0;
		  if (can_target(&td))
			{
			  pick_target(&td, prompts[i]);

			  card_instance_t* instance = get_card_instance(player, card);
			  int owner = instance->targets[0].player;
			  if (is_stolen(instance->targets[0].player, instance->targets[0].card))
				owner = 1-player;

			  who_shuffles |= 1<<owner;

			  put_on_top_of_deck(1 - player, instance->targets[0].card);
			}
		}

	  if (who_shuffles & (1<<player))
		shuffle(player);
	  if (who_shuffles & (1<<(1-player)))
		shuffle(1-player);
	}
  return 0;
}

static int effect_nature_shields_its_own(int player, int card, event_t event)
{
  /* Approximation - "Whenever a creature attacks and isn't blocked" is an event, not a trigger, in Manalink, so isn't
   * orderable.  Worse, the attacking player would put all his triggers on the stack first, so this would resolve
   * before any of them.  And there isn't an earlier opportunity than EVENT_DECLARE_BLOCKERS to do this in.
   *
   * This also may or may not activate "Whenever this creature becomes blocked" and "Whenever this creature becomes
   * blocked by a creature" abilities, depending on that card's and this effect's timestamps, because those are both
   * handled in EVENT_DECLARE_BLOCKERS instead of being real triggers, too. */

  if (event == EVENT_DECLARE_BLOCKERS && current_turn != player)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  instance->targets[1].player = 0;

	  int c;
	  for (c = 0; c < active_cards_count[1-player]; ++c)
		if (in_play(1-player, c) && is_what(1-player, c, TYPE_CREATURE) && (get_card_instance(1-player, c)->state & STATE_ATTACKING))
		  {
			if (!check_special_flags(1-player, c, SF_ATTACKING_PWALKER))
			  ++instance->targets[1].player;

			if (is_unblocked(1-player, c))
			  {
				token_generation_t token;
				default_token_definition(player, card, CARD_ID_PLANT, &token);
				token.action = TOKEN_ACTION_BLOCKING;
				token.action_argument = c;
				generate_token(&token);
			  }
		  }
	}

  if (trigger_condition == TRIGGER_END_COMBAT && affect_me(player, card) && reason_for_trigger_controller == player
	  && get_card_instance(player, card)->targets[1].player >= 4)
	{
	  if (event == EVENT_TRIGGER)
		event_result |= RESOLVE_TRIGGER_MANDATORY;
	  else if (event == EVENT_RESOLVE_TRIGGER)
		kill_card(player, card, KILL_EXILE);
	}

  return 0;
}
static int scheme_nature_shields_its_own(int player, int card, event_t event)
{
  /* Nature Shields Its Own	""
   * Ongoing Scheme
   * Whenever a creature attacks and isn't blocked, if you're the defending player, create a 0/1 |Sgreen Plant creature token that's blocking that creature.
   * When four or more creatures attack you, abandon this scheme at end of combat. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_nature_shields_its_own, -1, -1);
  return 0;
}

static int effect_nothing_can_stop_me_now(int player, int card, event_t event)
{
  if (event == EVENT_PREVENT_DAMAGE)
	{
	  card_instance_t* damage = get_card_instance(affected_card_controller, affected_card);
	  if (damage->internal_card_id == damage_card && damage->info_slot > 0
		  && damage->damage_source_player != player
		  && damage->damage_target_player == player && damage->damage_target_card == -1)
		--damage->info_slot;
	}

  if (trigger_condition == TRIGGER_EOT && affect_me(player, card)
	  && get_trap_condition(player, TRAP_DAMAGE_TAKEN) >= 5 && eot_trigger(player, card, event))
	kill_card(player, card, KILL_EXILE);

  return 0;
}
static int scheme_nothing_can_stop_me_now(int player, int card, event_t event)
{
  /* Nothing Can Stop Me Now	""
   * Ongoing Scheme
   * If a source an opponent controls would deal damage to you, prevent 1 of that damage.
   * At the beginning of each end step, if you've been dealt 5 or more damage this turn, abandon this scheme. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_nothing_can_stop_me_now, -1, -1);
  return 0;
}

static int scheme_only_blood_ends_your_nightmares(int player, int card, event_t event)
{
  /* Only Blood Ends Your Nightmares	""
   * Scheme
   * When you set this scheme in motion, each opponent sacrifices a creature. Then each opponent who didn't sacrifice a creature discards two cards. */

  if (event == EVENT_SET_IN_MOTION
	  && impose_sacrifice(player, card, 1-player, 1, TYPE_CREATURE, 0, 0, 0, 0, 0, 0, 0, -1, 0) < 1)
	new_multidiscard(1-player, 2, 0, player);
  return 0;
}

static int scheme_perhaps_youve_met_my_cohort(int player, int card, event_t event)
{
  /* Perhaps You've Met My Cohort	""
   * Scheme
   * When you set this scheme in motion, search your library for a planeswalker card, put it onto the battlefield, then shuffle your library. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TARGET_TYPE_PLANESWALKER, "Select a planeswalker card.");
	  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_VALUE, &test);
	}
  return 0;
}

// Does nothing on its own; searched for in set_scheme_in_motion().
static int effect_plots_that_span_centuries(int player, int card, event_t event)
{
  return 0;
}
static int scheme_plots_that_span_centuries(int player, int card, event_t event)
{
  /* Plots That Span Centuries	""
   * Scheme
   * When you set this scheme in motion, the next time you set a scheme in motion, set three schemes in motion instead. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_plots_that_span_centuries, -1, -1);
  return 0;
}

static int scheme_realms_befitting_my_majesty(int player, int card, event_t event)
{
  /* Realms Befitting My Majesty	""
   * Scheme
   * When you set this scheme in motion, search your library for up to two basic land cards, put them onto the battlefield tapped, then shuffle your library. */

  if (event == EVENT_SET_IN_MOTION)
	tutor_basic_lands(player, TUTOR_PLAY_TAPPED, 2);
  return 0;
}

static int scheme_roots_of_all_evil(int player, int card, event_t event)
{
  /* Roots of All Evil	""
   * Scheme
   * When you set this scheme in motion, create five 1/1 |Sgreen Saproling creature tokens. */

  if (event == EVENT_SET_IN_MOTION)
	generate_tokens_by_id(player, card, CARD_ID_SAPROLING, 5);
  return 0;
}

/* Attacks each combat if it's controlled by same player as effect controller.  Also used by The Dead Shall Serve and
 * For Each of You, a Gift. */
static int effect_rotted_ones_lay_siege(int player, int card, event_t event)
{
  if (event == EVENT_MUST_ATTACK && current_turn == player)
	{
	  card_instance_t* instance = get_card_instance(player, card);
	  attack_if_able(instance->damage_target_player, instance->damage_target_card, event);
	}

  return 0;
}
void add_rotted_ones_lay_siege_effect(token_generation_t* token, int card_added, int number)
{
  create_archenemy_legacy(token->s_player, token->s_card, token->action_argument, effect_rotted_ones_lay_siege,
						  token->t_player, card_added);
}
static int scheme_rotted_ones_lay_siege(int player, int card, event_t event)
{
  /* Rotted Ones, Lay Siege	""
   * Scheme
   * When you set this scheme in motion, for each opponent, create a 2/2 |Sblack Zombie creature token that attacks that player each combat if able. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  // Can't just add SP_KEYWORD_MUST_ATTACK, since it doesn't have to attack the Archenemy if it gets controlled.
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_ZOMBIE, &token);
	  token.action_argument = event_result;	// For use while creating the legacy
	  token.special_code_on_generation = &add_rotted_ones_lay_siege_effect;
	  generate_token(&token);
	}
  return 0;
}

static int scheme_surrender_your_thoughts(int player, int card, event_t event)
{
  /* Surrender Your Thoughts	""
   * Scheme
   * When you set this scheme in motion, target opponent chooses self or others. If that player chooses self, that player discards four cards. If the player chooses others, each of your other opponents discards two cards. */

  if (event == EVENT_SET_IN_MOTION && target_opponent(player, card))
	new_multidiscard(1-player, 4, 0, player);
  return 0;
}

static int scheme_the_dead_shall_serve(int player, int card, event_t event)
{
  /* The Dead Shall Serve	""
   * Scheme
   * When you set this scheme in motion, for each opponent, put up to one target creature card from that player's graveyard onto the battlefield under your control. Each of those creatures attacks its owner each combat if able. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  test_definition_t test;
	  default_test_definition(&test, TYPE_CREATURE);

	  int selected, reanimated;
	  if (new_special_count_grave(1-player, &test) > 0
		  && new_select_target_from_grave(player, card, 1-player, 0, AI_MAX_VALUE, &test, 0) != -1
		  // always validates for now, since it resolves immediately instead of using the stack
		  && (selected = validate_target_from_grave(player, card, 1-player, 0)) != -1
		  && (reanimated = reanimate_permanent(player, card, 1-player, selected, REANIMATE_NO_CONTROL_LEGACY))
		  && in_play(player, reanimated))	// Might've been sacrificed or controlled already by a cip trigger
		create_archenemy_legacy(player, card, event_result, effect_rotted_ones_lay_siege /*sic*/, player, reanimated);
	}
  return 0;
}

static int scheme_the_fate_of_the_flammable(int player, int card, event_t event)
{
  /* The Fate of the Flammable	""
   * Scheme
   * When you set this scheme in motion, target opponent chooses self or others. If that player chooses self, this scheme deals 6 damage to that player. If the player chooses others, this scheme deals 3 damage to each of your other opponents. */

  if (event == EVENT_SET_IN_MOTION && target_opponent(player, card))
	damage_player(1-player, 6, player, card);
  return 0;
}

static int scheme_the_iron_guardian_stirs(int player, int card, event_t event)
{
  /* The Iron Guardian Stirs	""
   * Scheme
   * When you set this scheme in motion, create a 4/6 colorless Golem artifact creature token. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  token_generation_t token;
	  default_token_definition(player, card, CARD_ID_GOLEM, &token);
	  token.pow = 4;
	  token.tou = 6;
	  generate_token(&token);
	}
  return 0;
}

static int effect_the_pieces_are_coming_together(int player, int card, event_t event)
{
  // Cost reduction done in card_archenemy()
  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	kill_card(player, card, KILL_EXILE);

  return 0;
}
static int scheme_the_pieces_are_coming_together(int player, int card, event_t event)
{
  /* The Pieces Are Coming Together	""
   * Scheme
   * When you set this scheme in motion, draw two cards. Artifact spells you cast this turn cost |2 less to cast. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  create_archenemy_legacy(player, card, event_result, effect_the_pieces_are_coming_together, -1, -1);
	  draw_cards(player, 2);
	  /* Irritatingly enough, a card needs EA_PLAY_COST set to be sent EVENT_MODIFY_COST_GLOBAL, not just to set the
	   * bit in event_flags.  So inform the parent Archenemy card so it can reduce costs instead.  If this is the
	   * Archenemy avatar, then player/card is the source Archenemy card; otherwise, player/card is an activation
	   * card, and its parent is. */
	  card_instance_t* instance = get_card_instance(player, card);
	  card_instance_t* archenemy;
	  if (instance->internal_card_id == activation_card)
		archenemy = get_card_instance(instance->parent_controller, instance->parent_card);
	  else
		archenemy = instance;

	  archenemy->info_slot |= 1;
	}
  return 0;
}

static int effect_the_very_soil_shall_shake(int player, int card, event_t event)
{
  if ((event == EVENT_POWER || event == EVENT_TOUGHNESS)
	  && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE))
	event_result += 2;

  if (event == EVENT_ABILITIES
	  && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE))
	event_result |= KEYWORD_TRAMPLE;

  if (event == EVENT_GRAVEYARD_FROM_PLAY
	  && affected_card_controller == player && is_what(affected_card_controller, affected_card, TYPE_CREATURE)
	  && in_play(affected_card_controller, affected_card))
	{
	  card_instance_t* affected = get_card_instance(affected_card_controller, affected_card);
	  if (affected->kill_code > 0 && affected->kill_code < KILL_EXILE)
		kill_card(player, card, KILL_EXILE);
	}

  return 0;
}
static int scheme_the_very_soil_shall_shake(int player, int card, event_t event)
{
  /* The Very Soil Shall Shake	""
   * Ongoing Scheme
   * Creatures you control get +2/+2 and have trample.
   * When a creature you control dies, abandon this scheme. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_the_very_soil_shall_shake, -1, -1);
  return 0;
}

static int scheme_tooth_claw_and_tail(int player, int card, event_t event)
{
  /* Tooth, Claw, and Tail	""
   * Scheme
   * When you set this scheme in motion, destroy up to three target nonland permanents. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_PERMANENT);
	  td.illegal_type = TYPE_LAND;
	  if (IS_AI(player))
		td.allowed_controller = 1-player;

	  card_instance_t* instance = get_card_instance(player, card);
	  instance->number_of_targets = 0;

	  int k;
	  for (k = 0; k < 3 && can_target(&td) && new_pick_target(&td, "TARGET_NONLAND_PERMANENT", -1, 0); ++k)
		state_untargettable(instance->targets[k].player, instance->targets[k].card, 1);

	  for (k = 0; k < instance->number_of_targets; ++k)
		state_untargettable(instance->targets[k].player, instance->targets[k].card, 0);

	  for (k = 0; k < instance->number_of_targets; ++k)
		kill_card(instance->targets[k].player, instance->targets[k].card, KILL_DESTROY);
	}
  return 0;
}

static int scheme_which_of_you_burns_brightest(int player, int card, event_t event)
{
  /* Which of You Burns Brightest?	""
   * Scheme
   * When you set this scheme in motion, you may pay |X. If you do, this scheme deals X damage to target opponent or planeswalker and each creature that player or that planeswalker's controller controls. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  target_definition_t td_opw;
	  opponent_or_pw_target_definition(player, card, &td_opw);
	  if (can_target(&td_opw) && pick_target(&td_opw, "TARGET_OPPONENT_OR_PLANESWALKER"))
		{
		  int spent;
		  if (IS_AI(player))
			{
			  int avail = has_mana(player, COLOR_ANY, 1);
			  for (; avail > 0; --avail)
				{
				  cancel = 0;
				  charge_mana(player, COLOR_COLORLESS, avail);
				  if (cancel != 1)	// Just in case mana was counted wrong, e.g. by multiple Gemhide Slivers
					break;
				}
			  spent = avail;
			}
		  else
			{
			  charge_mana(player, COLOR_COLORLESS, -1);
			  if (cancel == 1)
				spent = 0;
			  else
				spent = x_value;
			}

		  if (spent > 0)
			{
			  damage_target0(player, card, spent);
			  new_damage_all(player, card, 1-player, spent, NDA_ALL_CREATURES, NULL);
			}

		  card_instance_t* instance = get_card_instance(player, card);
		  instance->number_of_targets = 0;
		}
	}
  return 0;
}

static int scheme_your_fate_is_thrice_sealed(int player, int card, event_t event)
{
  /* Your Fate Is Thrice Sealed	""
   * Scheme
   * When you set this scheme in motion, reveal the top three cards of your library. Put all land cards revealed this way onto the battlefield and the rest into your hand. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  if (player == HUMAN)
		show_deck(1-player, deck_ptr[player], 3, "Opponent reveals these cards", 0, 0x7375B0);

	  int* deck = deck_ptr[player];
	  int k, iids[3];
	  for (k = 0; k < 3; ++k)
		iids[k] = deck[k];

	  obliterate_top_n_cards_of_deck(player, 3);

	  for (k = 0; k < 3 && iids[k] != -1; ++k)
		{
		  int card_added = add_card_to_hand(player, iids[k]);
		  if (is_what(player, card_added, TYPE_LAND))
			put_into_play(player, card_added);
		}
	}
  return 0;
}

static int effect_your_inescapable_doom(int player, int card, event_t event)
{
  if (player == current_turn && eot_trigger(player, card, event))
	{
	  add_counter(player, card, COUNTER_DOOM);
	  damage_player(1-player, count_counters(player, card, COUNTER_DOOM), player, card);
	}

  return 0;
}
static int scheme_your_inescapable_doom(int player, int card, event_t event)
{
  /* Your Inescapable Doom	""
   * Ongoing Scheme
   * At the beginning of your end step, put a doom counter on this scheme, then this scheme deals damage equal to the number of doom counters on it to the opponent with the highest life total among your opponents.
   * If two or more players are tied for highest life total, you choose one. */

  if (event == EVENT_SET_IN_MOTION)
	create_archenemy_legacy(player, card, event_result, effect_your_inescapable_doom, -1, -1);
  return 0;
}

static int effect_your_puny_minds_cannot_fathom(int player, int card, event_t event)
{
  if (event == EVENT_BEGIN_TURN && current_turn == player)
	kill_card(player, card, KILL_EXILE);

  if (event == EVENT_MAX_HAND_SIZE && current_turn == player)
	event_result = 99;

  return 0;
}

static int scheme_your_puny_minds_cannot_fathom(int player, int card, event_t event)
{
  /* Your Puny Minds Cannot Fathom	""
   * Scheme
   * When you set this scheme in motion, draw four cards. You have no maximum hand size until your next turn. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  draw_cards(player, 4);
	  create_archenemy_legacy(player, card, event_result, effect_your_puny_minds_cannot_fathom, -1, -1);
	}
  return 0;
}

static int scheme_your_will_is_not_your_own(int player, int card, event_t event)
{
  /* Your Will Is Not Your Own	""
   * Scheme
   * When you set this scheme in motion, gain control of target creature an opponent controls until end of turn. Untap that creature. It gets +3/+3 and gains trample and haste until end of turn. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.preferred_controller = 1-player;
	  td.allow_cancel = 0;

	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  int leg = effect_act_of_treason(player, card, instance->targets[0].player, instance->targets[0].card);
		  if (leg != -1)
			{
			  card_instance_t* legacy = get_card_instance(player, leg);
			  legacy->targets[2].player = 3;	// +power
			  legacy->targets[2].card = 3;		// +toughness
			  legacy->targets[3].player = KEYWORD_TRAMPLE;
			}
		}
	}
  return 0;
}

static void set_scheme_in_motion(int player, int card, int csvid)
{
  card_instance_t *instance = get_card_instance(player, card);

  // All in Good Time legacy: prevents controller from setting schemes in motion except on turn its put on the bf
  // Plots That Span Centuries legacy: set three schemes in motion (but only if no All in Good Time legacy)
  int c;
  int plots_legacy = -1;
  for (c = 0; c < active_cards_count[player]; ++c)
	if (in_play(player, c))
	  {
		card_instance_t* inst = get_card_instance(player, c);
		if (inst->internal_card_id == LEGACY_EFFECT_CUSTOM)
		  {
			if (inst->info_slot == (int)effect_all_in_good_time && inst->targets[1].card == 42)
			  return;

			if (plots_legacy == -1 && inst->info_slot == (int)effect_plots_that_span_centuries)
			  plots_legacy = c;
		  }
	  }

  if (plots_legacy != -1)
	{
	  kill_card(player, plots_legacy, KILL_EXILE);
	  set_scheme_in_motion(player, card, -1);
	  set_scheme_in_motion(player, card, -1);
	  set_scheme_in_motion(player, card, -1);
	  return;
	}

  if (csvid <= 0)
	{
	  // Top card of schemes deck
	  csvid = arch_deck[player][0];

	  // Put the used scheme on the bottom of the schemes deck
	  int k;
	  for (k = 0; k < SCHEME_DECK_SIZE - 1; ++k)
		arch_deck[player][k] = arch_deck[player][k+1];

	  arch_deck[player][SCHEME_DECK_SIZE - 1] = csvid;
	}

  play_sound_effect(WAV_SCHEME);

  if (csvid != CARD_ID_INTRODUCTIONS_ARE_IN_ORDER	// has its own prompt
	  && csvid != CARD_ID_BEHOLD_THE_POWER_OF_DESTRUCTION)	// also has its own prompt
	DIALOG(player, card, EVENT_ACTIVATE,
		   DLG_FULLCARD_CSVID(csvid),
		   DLG_NO_STORAGE,
		   DLG_NO_CANCEL,
		   DLG_WHO_CHOOSES(HUMAN),
		   "Set into motion", 1, 1);	// give it an explicit choice so that the player has time to read the scheme card

  instance->number_of_targets = 0;

  int iid = get_internal_card_id_from_csv_id(csvid);
  ASSERT(iid > 0 && "invalid scheme csvid");
  CardFunction fn = cards_data[iid].code_pointer;
  ASSERT(fn && fn != (CardFunction)0x401000 && "invalid scheme function");
  int old_event_result = event_result;
  event_result = csvid;
  fn(player, card, EVENT_SET_IN_MOTION);
  event_result = old_event_result;
}

static int register_archenemy_schemes(int* schemes_list)
{
  int n = 0;
#define REGISTER_SCHEME(csvid, fn)	\
  do {								\
	cards_data[get_internal_card_id_from_csv_id(csvid)].code_pointer = fn;	\
	if (schemes_list)				\
	  schemes_list[n] = csvid;		\
	++n;							\
  } while (0)

  REGISTER_SCHEME(CARD_ID_A_DISPLAY_OF_MY_DARK_POWER, scheme_a_display_of_my_dark_power);
  REGISTER_SCHEME(CARD_ID_ALL_IN_GOOD_TIME, scheme_all_in_good_time);
  REGISTER_SCHEME(CARD_ID_ALL_SHALL_SMOLDER_IN_MY_WAKE, scheme_all_shall_smolder_in_my_wake);
  REGISTER_SCHEME(CARD_ID_APPROACH_MY_MOLTEN_REALM, scheme_approach_my_molten_realm);
  REGISTER_SCHEME(CARD_ID_BEHOLD_THE_POWER_OF_DESTRUCTION, scheme_behold_the_power_of_destruction);
  REGISTER_SCHEME(CARD_ID_DANCE_PATHETIC_MARIONETTE, scheme_dance_pathetic_marionette);
  REGISTER_SCHEME(CARD_ID_EMBRACE_MY_DIABOLICAL_VISION, scheme_embrace_my_diabolical_vision);
  REGISTER_SCHEME(CARD_ID_EVERY_HOPE_SHALL_VANISH, scheme_every_hope_shall_vanish);
  REGISTER_SCHEME(CARD_ID_EVERY_LAST_VESTIGE_SHALL_ROT, scheme_every_last_vestige_shall_rot);
  REGISTER_SCHEME(CARD_ID_EVIL_COMES_TO_FRUITION, scheme_evil_comes_to_fruition);
  REGISTER_SCHEME(CARD_ID_FEED_THE_MACHINE, scheme_feed_the_machine);
  REGISTER_SCHEME(CARD_ID_I_CALL_ON_THE_ANCIENT_MAGICS, scheme_i_call_on_the_ancient_magics);
  REGISTER_SCHEME(CARD_ID_I_DELIGHT_IN_YOUR_CONVULSIONS, scheme_i_delight_in_your_convulsions);
  REGISTER_SCHEME(CARD_ID_I_KNOW_ALL_I_SEE_ALL, scheme_i_know_all_i_see_all);
  REGISTER_SCHEME(CARD_ID_IGNITE_THE_CLONEFORGE, scheme_ignite_the_cloneforge);
  REGISTER_SCHEME(CARD_ID_IMPRISON_THIS_INSOLENT_WRETCH, scheme_imprison_this_insolent_wretch);
  REGISTER_SCHEME(CARD_ID_INTO_THE_EARTHEN_MAW, scheme_into_the_earthen_maw);
  REGISTER_SCHEME(CARD_ID_INTRODUCTIONS_ARE_IN_ORDER, scheme_introductions_are_in_order);
  REGISTER_SCHEME(CARD_ID_KNOW_NAUGHT_BUT_FIRE, scheme_know_naught_but_fire);
  REGISTER_SCHEME(CARD_ID_LOOK_SKYWARD_AND_DESPAIR, scheme_look_skyward_and_despair);
  REGISTER_SCHEME(CARD_ID_MAY_CIVILIZATION_COLLAPSE, scheme_may_civilization_collapse);
  REGISTER_SCHEME(CARD_ID_MY_CRUSHING_MASTERSTROKE, scheme_my_crushing_masterstroke);
  REGISTER_SCHEME(CARD_ID_MY_GENIUS_KNOWS_NO_BOUNDS, scheme_my_genius_knows_no_bounds);
  REGISTER_SCHEME(CARD_ID_MY_UNDEAD_HORDE_AWAKENS, scheme_my_undead_horde_awakens);
  REGISTER_SCHEME(CARD_ID_MY_WISH_IS_YOUR_COMMAND, scheme_my_wish_is_your_command);
  REGISTER_SCHEME(CARD_ID_NATURE_DEMANDS_AN_OFFERING, scheme_nature_demands_an_offering);
  REGISTER_SCHEME(CARD_ID_NATURE_SHIELDS_ITS_OWN, scheme_nature_shields_its_own);
  REGISTER_SCHEME(CARD_ID_NOTHING_CAN_STOP_ME_NOW, scheme_nothing_can_stop_me_now);
  REGISTER_SCHEME(CARD_ID_ONLY_BLOOD_ENDS_YOUR_NIGHTMARES, scheme_only_blood_ends_your_nightmares);
  REGISTER_SCHEME(CARD_ID_PERHAPS_YOUVE_MET_MY_COHORT, scheme_perhaps_youve_met_my_cohort);
  REGISTER_SCHEME(CARD_ID_PLOTS_THAT_SPAN_CENTURIES, scheme_plots_that_span_centuries);
  REGISTER_SCHEME(CARD_ID_REALMS_BEFITTING_MY_MAJESTY, scheme_realms_befitting_my_majesty);
  REGISTER_SCHEME(CARD_ID_ROOTS_OF_ALL_EVIL, scheme_roots_of_all_evil);
  REGISTER_SCHEME(CARD_ID_ROTTED_ONES_LAY_SIEGE, scheme_rotted_ones_lay_siege);
  REGISTER_SCHEME(CARD_ID_SURRENDER_YOUR_THOUGHTS, scheme_surrender_your_thoughts);
  REGISTER_SCHEME(CARD_ID_THE_DEAD_SHALL_SERVE, scheme_the_dead_shall_serve);
  REGISTER_SCHEME(CARD_ID_THE_FATE_OF_THE_FLAMMABLE, scheme_the_fate_of_the_flammable);
  REGISTER_SCHEME(CARD_ID_THE_IRON_GUARDIAN_STIRS, scheme_the_iron_guardian_stirs);
  REGISTER_SCHEME(CARD_ID_THE_PIECES_ARE_COMING_TOGETHER, scheme_the_pieces_are_coming_together);
  REGISTER_SCHEME(CARD_ID_THE_VERY_SOIL_SHALL_SHAKE, scheme_the_very_soil_shall_shake);
  REGISTER_SCHEME(CARD_ID_TOOTH_CLAW_AND_TAIL, scheme_tooth_claw_and_tail);
  REGISTER_SCHEME(CARD_ID_WHICH_OF_YOU_BURNS_BRIGHTEST, scheme_which_of_you_burns_brightest);
  REGISTER_SCHEME(CARD_ID_YOUR_FATE_IS_THRICE_SEALED, scheme_your_fate_is_thrice_sealed);
  REGISTER_SCHEME(CARD_ID_YOUR_INESCAPABLE_DOOM, scheme_your_inescapable_doom);
  REGISTER_SCHEME(CARD_ID_YOUR_PUNY_MINDS_CANNOT_FATHOM, scheme_your_puny_minds_cannot_fathom);
  REGISTER_SCHEME(CARD_ID_YOUR_WILL_IS_NOT_YOUR_OWN, scheme_your_will_is_not_your_own);
#undef REGISTER_SCHEME
  return n;
}

int register_archenemy_nicol_bolas_schemes(int* schemes);

static void create_schemes_deck(int player)
{
  int i;

  if (!schemes)
	{
	  // first call - get a count, and assign individual scheme functions
	  int n = register_archenemy_schemes(NULL);
	  n += register_archenemy_nicol_bolas_schemes(NULL);

	  num_schemes = n;
	  schemes = (int*)malloc(sizeof(int) * num_schemes);

	  // second call - append to schemes[], and redundantly reassign individual scheme functions
	  n = register_archenemy_schemes(schemes);
	  register_archenemy_nicol_bolas_schemes(&schemes[n]);
	}

  // Create a random deck of 15 schemes. Only two schemes with the same name are allowed.
  int chosen[num_schemes];
  memset(chosen, 0, sizeof(chosen));
  for (i = 0; i < SCHEME_DECK_SIZE; ++i)
	{
	  int sch;
	  do
		{
		  sch = internal_rand(num_schemes);
		} while (chosen[sch] >= 2);

	  arch_deck[player][i] = schemes[sch];
	  ++chosen[sch];
	}
}

int card_archenemy(int player, int card, event_t event)
{
  /* Local data:
   * info_slot: &1: Check for The Pieces Are Coming Together effects during EVENT_MODIFY_COST_GLOBAL
   *            &2: A scheme was set in motion at the start of this turn's precombat main phase
   *            &4: A scheme was set in motion via activating the card
   * special_infos: 88 if an activateable Archenemy's Power challenge reward; else 0
   */
  card_instance_t* instance = get_card_instance(player, card);

  // The Pieces Are Coming Together - EVENT_MODIFY_COST_GLOBAL can't be handled in LEGACY_EFFECT_CUSTOM; must be handled first
  if (event == EVENT_MODIFY_COST_GLOBAL)
	{
	  if ((instance->info_slot & 1)
		  && (affected_card_controller == player || affected_card_controller == -1)
		  && is_what(affected_card_controller, affected_card, TYPE_ARTIFACT))
		{
		  // Count number of The Pieces Are Coming Together effects in play
		  int c, num_effects = 0;
		  for (c = player; c < active_cards_count[player]; ++c)
			if (in_play(player, c))
			  {
				card_instance_t* inst = get_card_instance(player, c);
				if (inst->internal_card_id == 903 && inst->display_pic_csv_id == CARD_ID_THE_PIECES_ARE_COMING_TOGETHER)
				  ++num_effects;
			  }

		  if (num_effects == 0)
			instance->info_slot &= ~1;	// all effects gone
		  else
			COST_COLORLESS -= 2 * num_effects;
		}

	  return 0;
	}

  if (event == EVENT_RESOLVE_SPELL)
	create_schemes_deck(player);

  // Normal archenemy avatar - set a scheme in motion at start of precombat main phase
  if (get_special_infos(player, card) < 88)
	{
	  vanguard_card(player, card, event, 7, 40, 7);

	  if (event == EVENT_PHASE_CHANGED && current_phase == PHASE_MAIN1 && current_turn == player
		  && !(instance->info_slot & 2))
		{
		  instance->info_slot |= 2;	// EVENT_PHASE_CHANGED sometimes gets sent more than once
		  set_scheme_in_motion(player, card, -1);
		  dispatch_event(player, card, EVENT_TRIGGER_RESOLVED);	// so we at least get becomes-targetted triggers
		}
	}
  else	// Archenemy's power - set a scheme in motion when activated, once per game
	{
	  if (event == EVENT_CAN_ACTIVATE)
		return !(instance->info_slot & 4) && can_sorcery_be_played(player, event);

	  if (event == EVENT_ACTIVATE)
		instance->info_slot |= 4;

	  if (event == EVENT_RESOLVE_ACTIVATION)
		{
		  ASSERT(schemes != NULL);
		  int scheme = schemes[internal_rand(num_schemes-1)];
		  set_scheme_in_motion(player, card, scheme);
		}
	}

  if (event == EVENT_BEGIN_TURN)
	instance->info_slot &= ~2;		// hasn't yet activated at start of main phase already

  return 0;
}
