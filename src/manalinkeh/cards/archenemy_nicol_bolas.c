#include "manalink.h"

int create_archenemy_legacy(int player, int card, int csvid, int (*func_ptr)(int, int, event_t), int t_player, int t_card);

void add_rotted_ones_lay_siege_effect(token_generation_t* token, int card_added, int number);
static void black_horror(int player, int card, int attacks_unless_controlled)
{
  token_generation_t token;
  default_token_definition(player, card, CARD_ID_HORROR, &token);
  token.pow = 3;
  token.tou = 3;
  token.color_forced = COLOR_TEST_BLACK;
  token.no_sleight = 1;

  if (attacks_unless_controlled)
	{
	  token.action_argument = event_result;
	  token.special_code_on_generation = &add_rotted_ones_lay_siege_effect;	// sic
	}
  generate_token(&token);
}

static int scheme_a_reckoning_approaches(int player, int card, event_t event)
{
  /* A Reckoning Approaches	""
   * Scheme
   * When you set this scheme in motion, look at the top six cards of your library. You may put a
   * creature card from among them onto the battlefield. Put the rest on the bottom of your library
   * in any order. */

  if (event == EVENT_SET_IN_MOTION)
  {
    int n = count_deck(player);
    n = MIN(n, 6);
    if (n > 0)
    {
      test_definition_t this_test;
      new_default_test_definition(&this_test, TYPE_CREATURE, "Select a creature card.");
      this_test.create_minideck = n;
      this_test.no_shuffle = 1;

      if (new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_PLAY, 0, AI_MAX_CMC,
                           &this_test) >= 0)
        --n;

      if (n > 0)
        put_top_x_on_bottom(player, player, n);
    }
  }
  return 0;
}

/* Because I Have Willed It	""
 * Ongoing Scheme
 * Spells you cast cost |1 less to cast.
 * At the beginning of your opponents' end step, if they cast four or more spells this turn, abandon this scheme. */

static int scheme_behold_my_grandeur(int player, int card, event_t event)
{
  /* Behold My Grandeur	""
   * Scheme
   * When you set this scheme in motion, add |U|B|R.
   * When you set this scheme in motion, if you control six or more lands, you may search your library for a card with converted mana cost 6 or greater, reveal it, put it into your hand, then shuffle your library. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  produce_mana_multi(player, MANACOST_UBR(1,1,1));
	  if (count_permanents_by_type(player, TYPE_LAND) < 6)
		{
		  DIALOG(player, card, EVENT_ACTIVATE,
				 DLG_FULLCARD_CSVID(event_result),
				 DLG_NO_STORAGE,
				 DLG_NO_CANCEL,
				 DLG_WHO_CHOOSES(HUMAN),
				 "Set into motion", 1, 1);
		}
	  else if (DIALOG(player, card, EVENT_ACTIVATE,
					  DLG_FULLCARD_CSVID(event_result),
					  DLG_NO_STORAGE,
					  DLG_NO_CANCEL,
					  "Search for a card", 1, 1,
					  "Decline", 1, 0) == 1)
		{
		  test_definition_t test;
		  new_default_test_definition(&test, TYPE_ANY, "Select a card with converted mana cost 6 or greater.");
		  test.cmc = 5;
		  test.cmc_flag = F5_CMC_GREATER_THAN_VALUE;
		  new_global_tutor(player, player, TUTOR_FROM_DECK, TUTOR_HAND, 0, AI_MAX_VALUE, &test);
		}
	}
  return 0;
}

/* Bow to My Command	""
 * Ongoing Scheme
 * As you set this scheme in motion, choose an opponent.
 * Creatures the chosen player controls can't attack you or planeswalkers you control.
 * At the beginning of your opponents' end step, they may tap any number of untapped creatures they control with total power 8 or greater. If they do, abandon this scheme. */

/* Choose Your Demise	""
 * Scheme
 * When you set this scheme in motion, look at the top four cards of your library and separate them into a face-down pile and a face-up pile. An opponent chooses one of those piles. Put the cards in that pile into your hand and the rest on the bottom of your library in any order. */

static int effect_delight_in_the_hunt(int player, int card, event_t event)
{
  if ((event == EVENT_CLEANUP || event == EVENT_SHOULD_AI_PLAY) && affect_me(player, card))
	kill_card(player, card, KILL_EXILE);

  card_instance_t* damage = damage_being_prevented(event);
  if (damage && damage->damage_target_player == player && damage->damage_target_card >= 0
	  && !damage_is_to_planeswalker(damage))
	prevent_damage(affected_card_controller, affected_card, -1);

  return 0;
}
static int scheme_delight_in_the_hunt(int player, int card, event_t event)
{
  /* Delight in the Hunt	""
   * Scheme
   * When you set this scheme in motion, create a 3/3 |Sblack Horror creature token and prevent all damage that would be dealt to creatures you control this turn. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  black_horror(player, card, 0);
	  create_archenemy_legacy(player, card, event_result, effect_delight_in_the_hunt, -1, -1);
	}
  return 0;
}

static int scheme_every_dream_a_nightmare(int player, int card, event_t event)
{
  /* Every Dream a Nightmare	""
   * Scheme
   * When you set this scheme in motion, each opponent discards a card. You draw a card for each land card discarded this way. */

  if (event == EVENT_SET_IN_MOTION && hand_count[1-player] > 0)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_ANY, "Select a card to discard.");

	  int c = new_select_a_card(1-player, 1-player, TUTOR_FROM_HAND, 1, AI_MIN_VALUE, -1, &test);
	  int land = is_what(1-player, c, TYPE_LAND);
	  new_discard_card(1-player, c, 0, player);
	  if (land)
		draw_cards(player, 1);
	}
  return 0;
}

static int scheme_for_each_of_you_a_gift(int player, int card, event_t event)
{
  /* For Each of You, a Gift	""
   * Scheme
   * When you set this scheme in motion, for each opponent, create a 3/3 |Sblack Horror creature token that attacks that player each combat if able. */

  if (event == EVENT_SET_IN_MOTION)
	black_horror(player, card, 1);
  return 0;
}

/* Know Evil	""
 * Scheme
 * When you set this scheme in motion, until your next turn, up to one target opponent can't attack with creatures, up to one target opponent can't cast creature spells, and up to one target opponent can't cast noncreature spells. You can't choose any player as a target more than once. */

/* Make Yourself Useful	""
 * Scheme
 * When you set this scheme in motion, destroy target creature an opponent controls. If a creature is destroyed this way, you gain life equal to its toughness. */

/* My Forces Are Innumerable	""
 * Ongoing Scheme
 * At the beginning of your end step, create a 3/3 |Sblack Horror creature token.
 * At the beginning of your opponents' end step, they may sacrifice two creatures. If they do, abandon this scheme. */

/* My Laughter Echoes	""
 * Ongoing Scheme
 * Whenever you set a non-ongoing scheme in motion, you may abandon this scheme. If you do, set that scheme in motion again. */

static int scheme_no_one_will_hear_your_cries(int player, int card, event_t event)
{
  /* No One Will Hear Your Cries	""
   * Scheme
   * When you set this scheme in motion, each opponent chooses a creature they control, then sacrifices the rest. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  target_definition_t td;
	  base_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = td.preferred_controller = td.who_chooses = 1-player;
	  td.allow_cancel = 0;

	  int c, chosen = -1;
	  if (can_target(&td) && pick_target(&td, "SELECT_A_CREATURE_YOU_CONTROL"))
		{
		  card_instance_t* instance = get_card_instance(player, card);
		  instance->number_of_targets = 0;
		  chosen = instance->targets[0].card;
		}
	  cancel = 0;

	  for (c = 0; c < active_cards_count[1-player]; ++c)
		if (c != chosen
			&& in_play(1-player, c)
			&& is_what(1-player, c, TYPE_CREATURE))
		  kill_card(1-player, c, KILL_SACRIFICE);
	}
  return 0;
}

static int scheme_pay_tribute_to_me(int player, int card, event_t event)
{
  /* Pay Tribute to Me	""
   * Scheme
   * When you set this scheme in motion, each opponent sacrifices a creature. If you control six or more lands, each opponent sacrifices another creature. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  test_definition_t test;
	  new_default_test_definition(&test, TYPE_CREATURE, "Select a creature to sacrifice.");
	  new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	  if (count_permanents_by_type(player, TYPE_LAND) >= 6)
		new_sacrifice(player, card, 1-player, SAC_CAUSED | SAC_NO_CANCEL, &test);
	}
  return 0;
}

/* Power Without Equal	""
 * Scheme
 * When you set this scheme in motion, draw three cards. You have no maximum hand size until your next turn. If you control six or more lands, you may cast up to three nonland cards from your hand without paying their mana costs. */

static int scheme_the_mighty_will_fall(int player, int card, event_t event)
{
  /* The Mighty Will Fall	""
   * Scheme
   * When you set this scheme in motion, choose an opponent with the highest life total among your opponents. That player loses 7 life. */

  if (event == EVENT_SET_IN_MOTION)
	lose_life(1-player, 7);
  return 0;
}

static int scheme_there_is_no_refuge(int player, int card, event_t event)
{
  /* There Is No Refuge	""
   * Scheme
   * When you set this scheme in motion, it deals 3 damage to up to one target creature. Create a 3/3 |Sblack Horror creature token. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  card_instance_t* instance = get_card_instance(player, card);

	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  if (IS_AI(player))
		td.allowed_controller = 1-player;

	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
		damage_target0(player, card, 3);
	  cancel = 0;
	  instance->number_of_targets = 0;

	  black_horror(player, card, 0);
	}
  return 0;
}

static int scheme_this_world_belongs_to_me(int player, int card, event_t event)
{
  /* This World Belongs to Me	""
   * Scheme
   * When you set this scheme in motion, draw two cards. You may play an additional land this turn. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  draw_cards(player, 2);
	  create_archenemy_legacy(player, card, event_result, check_playable_lands_legacy, -1, -1);
	}
  return 0;
}

static int scheme_whats_yours_is_now_mine(int player, int card, event_t event)
{
  /* What's Yours Is Now Mine	""
   * Scheme
   * When you set this scheme in motion, gain control of target creature an opponent controls and untap it. */

  if (event == EVENT_SET_IN_MOTION)
	{
	  target_definition_t td;
	  default_target_definition(player, card, &td, TYPE_CREATURE);
	  td.allowed_controller = 1-player;
	  td.allow_cancel = 0;

	  card_instance_t* instance = get_card_instance(player, card);
	  if (can_target(&td) && pick_target(&td, "TARGET_CREATURE"))
		{
		  int leg = gain_control(player, card, instance->targets[0].player, instance->targets[0].card);
		  if (leg != -1)
			{
			  card_instance_t* legacy = get_card_instance(player, leg);
			  legacy->display_pic_csv_id = event_result;
			  legacy->display_pic_num = get_card_image_number(event_result, player, card);

			  if (in_play(legacy->damage_target_player, legacy->damage_target_card))
				untap_card(legacy->damage_target_player, legacy->damage_target_card);
			}
		  else	// still untaps, even if failed to gain control
			untap_card(instance->targets[0].player, instance->targets[0].card);
		}
	  cancel = 0;
	  instance->number_of_targets = 0;
	}
  return 0;
}

/* When Will You Learn?	""
 * Scheme
 * When you set this scheme in motion, each opponent exiles the top card of their library. You may cast any number of nonland cards exiled this way without paying their mana costs. */

int register_archenemy_nicol_bolas_schemes(int* schemes_list)
{
  int n = 0;
#define REGISTER_SCHEME(csvid, fn)	\
  do {								\
	cards_data[get_internal_card_id_from_csv_id(csvid)].code_pointer = fn;	\
	if (schemes_list)				\
	  schemes_list[n] = csvid;		\
	++n;							\
  } while (0)

  REGISTER_SCHEME(CARD_ID_A_RECKONING_APPROACHES, scheme_a_reckoning_approaches);
  REGISTER_SCHEME(CARD_ID_BEHOLD_MY_GRANDEUR, scheme_behold_my_grandeur);
  REGISTER_SCHEME(CARD_ID_DELIGHT_IN_THE_HUNT, scheme_delight_in_the_hunt);
  REGISTER_SCHEME(CARD_ID_EVERY_DREAM_A_NIGHTMARE, scheme_every_dream_a_nightmare);
  REGISTER_SCHEME(CARD_ID_FOR_EACH_OF_YOU_A_GIFT, scheme_for_each_of_you_a_gift);
  REGISTER_SCHEME(CARD_ID_NO_ONE_WILL_HEAR_YOUR_CRIES, scheme_no_one_will_hear_your_cries);
  REGISTER_SCHEME(CARD_ID_PAY_TRIBUTE_TO_ME, scheme_pay_tribute_to_me);
  REGISTER_SCHEME(CARD_ID_THE_MIGHTY_WILL_FALL, scheme_the_mighty_will_fall);
  REGISTER_SCHEME(CARD_ID_THERE_IS_NO_REFUGE, scheme_there_is_no_refuge);
  REGISTER_SCHEME(CARD_ID_THIS_WORLD_BELONGS_TO_ME, scheme_this_world_belongs_to_me);
  REGISTER_SCHEME(CARD_ID_WHATS_YOURS_IS_NOW_MINE, scheme_whats_yours_is_now_mine);
#undef REGISTER_SCHEME
  return n;
}
