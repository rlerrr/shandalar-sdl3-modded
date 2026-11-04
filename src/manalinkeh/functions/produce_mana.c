#include "manalink.h"

/* Canonical recovered helper declarations. */
void mana_doesnt_drain_until_eot(int player, color_t color, int amt);
void minimize_nondraining_mana(void);
void produce_mana_multi(int player, int cless, int black, int blue, int green, int red,
                             int white);
int single_color_test_bit_to_color(color_test_t col);
void declare_mana_available_maybe_hex(int player, color_test_t colors, int amount);
int produce_mana_tapped_all_one_color_with_default(int player, int card,
                                                        color_test_t available, int num_mana,
                                                        color_test_t default_color);
int produce_mana_tapped_any_combination_of_colors(int player, int card, color_test_t available,
                                                       int num_mana, const char *prompt);
int produce_mana_tapped_all_one_color(int player, int card, color_test_t available,
                                           int num_mana);
void produce_mana_tapped(int player, int card, color_t color, int amount);
void produce_mana_tapped2(int player, int card, color_t color1, int amount1, color_t color2,
                               int amount2);
void produce_mana_tapped_multi(int player, int card, int colorless, int black, int blue,
                                    int green, int red, int white);
int choose_mana_all_one_color(int player, color_test_t available, int num_mana,
                                   const char *prompt, color_test_t default_color);
int produce_mana_any_combination_of_colors(int player, color_test_t available, int num_mana,
                                                const char *prompt);
int produce_mana_all_one_color(int player, color_test_t available, int num_mana);
void declare_mana_available_any_combination_of_colors(int player, color_test_t available,
                                                           int amount);
void produce_mana_of_any_type_tapped_for(int player, int card, int amount);
void produce_mana_when_subtype_is_tapped(int allowed_player, event_t event, int type,
                                              int subtype, color_t color, int amount);
int get_colors_of_mana_land_could_produce_ignoring_costs(int player, int card);
int two_mana_land(int player, int card, event_t event, color_t color1, color_t color2);
int mana_producing_creature(int player, int card, event_t event, int reluctance_to_fight,
                                 color_t color, int amount);
int mana_producing_creature_multi(int player, int card, event_t event, int reluctance_to_fight,
                                       int colorless, int black, int blue, int green, int red,
                                       int white);
int mana_producer(int player, int card, event_t event);
int mana_producer_tapped(int player, int card, event_t event);
void mana_burn(void);
int permanents_you_control_can_tap_for_mana(int player, int card, event_t event, type_t type,
                                                 int32_t subtype, color_t color, int amount);
int artifact_mana_all_one_color(int player, int card, event_t event, int amount, int sac);
int mana_producing_creature_all_one_color(int player, int card, event_t event,
                                               int reluctance_to_fight, color_test_t available,
                                               int num_mana);
static void choose_combination_of_colors_of_mana_to_produce_into_arr(int *cols, int player,
                                                                   color_test_t available,
                                                                   int num_mana, const char *prompt);
int wild_growth_aura(int player, int card, event_t event, int subtype, color_t color,
                          int amount);
int wild_growth_aura_all_one_color(int player, int card, event_t event, int subtype,
                                        color_test_t available, int num_mana);
int wild_growth_aura_any_combination_of_colors(int player, int card, event_t event,
                                                    int subtype, color_test_t available,
                                                    int num_mana);
int all_lands_are_basiclandtype(int player, int card, int event, int whose_lands,
                                     int land_color, int land_subtype);
int tap_a_permanent_you_control_for_mana(int player, int card, event_t event, type_t type,
                                              int32_t subtype, color_t color, int amount);
int permanents_you_control_can_tap_for_mana_all_one_color(int player, int card, event_t event,
                                                               type_t type, int32_t subtype,
                                                               color_test_t available, int num_mana);
int sac_land_tapped(int player, int card, event_t event, int base_color, int color1,
                         int color2);
int sac_land(int player, int card, event_t event, int base_color, int color1, int color2);

int tapped_for_mana[COLOR_ARTIFACT + 1] = { 0 };

char always_prompt_for_color = 0;

/* Color or colors activately chosen to tap for.  Won't be set if the color is overridden by e.g. Contamination.  Only set for some of the functions here;
 * inspect them before inspecting this. */
color_test_t chosen_colors = 0;

/* Forwards to either declare_mana_available() or declare_mana_available_hex(), depending on whether more than one bit is set in colors.
 * declare_mana_available() is always preferable to declare_mana_available_hex(), which has limited slots and is much slower in has_mana(), charge_mana(), etc.
 * Calling declare_mana_available() directly instead of this is preferable too when the combination of colors is known. */

// Converts from a color_test_t with exactly one bit set to a color_t.

/* Produces the specified color/amount of mana (modified by Contamination if the card is a land) and taps card.  Only produces mana if card is < 0, though it'll
 * still set tapped_for_mana_color, so you probably don't want to do this. */

// As produce_mana_tapped(), but for two colors of mana.

// As produce_mana_tapped, but for arbitrary amounts of colored/unrestricted-colorless mana.

static int choose_any_color_from_available(color_test_t available){
	int i;
	for (i = COLOR_COLORLESS; i <= COLOR_ARTIFACT; ++i){
		if (available & (1<<i)){
			return i;
		}
	}

	// no colored available
	return COLOR_COLORLESS;
}

// Chooses colors one at a time - call once for each num_mana, decreasing num_mana each time.
static int choose_combination_of_colors_of_mana_to_produce_onechoice(int* cols, int player, color_test_t available, int num_mana, const char* prompt){
	int i, highest = -1, num_available_needed = 0;
	color_test_t needed = 0;
	int charging[COLOR_ARTIFACT + 1] = { 0 };

	if (needed_mana_colors > 0){	// paying a cost
		// First, collect amounts needed
		for (i = COLOR_COLORLESS; i <= COLOR_ARTIFACT; ++i){
			if ((*charge_mana_addr_of_pay_mana_xbugrw)[i] == -1){
				charging[i] = 1000;
			} else {
				charging[i] = ((*charge_mana_addr_of_pay_mana_xbugrw)[i]	// amount not yet paid for
							   - cols[i]									// amount already in array
							   + (charge_mana_addr_of_pre_mana_pool ?		// this will be non-NULL only when handling EVENT_TAP_CARD from sources tapped within charge_mana()
								  (*charge_mana_addr_of_pre_mana_pool)[i] - mana_pool[player][i] : 0));	// amount added to mana pool by source just activated, but not yet applied to amount charged
			}
		}

		// Now, apply any excess colored mana to artifact and/or colorless cost if needed; otherwise, ignore excess.
		// If amount for any color is unsatisfied, record that.
		for (i = COLOR_COLORLESS; i <= COLOR_ARTIFACT; ++i){
			if (charging[i] < 0){
				if (charging[COLOR_ARTIFACT] > 0 && i != COLOR_ARTIFACT){
					int amt_to_apply = MIN(-charging[i], charging[COLOR_ARTIFACT]);
					charging[COLOR_ARTIFACT] -= amt_to_apply;
					charging[i] += amt_to_apply;
					if (charging[i] >= 0)
						continue;
					// otherwise, apply to colorless, too
				}
				if (charging[COLOR_COLORLESS] > 0 && i != COLOR_COLORLESS){
					int amt_to_apply = MIN(-charging[i], charging[COLOR_COLORLESS]);
					charging[COLOR_COLORLESS] -= amt_to_apply;
					charging[i] += amt_to_apply;
				}
			} else if (charging[i] > 0){
				needed |= 1<<i;

				if (i != COLOR_COLORLESS && i != COLOR_ARTIFACT && (available & (1<<i))){
					++num_available_needed;

					num_mana -= charging[i];

					if (highest == -1
						|| charging[i] > charging[highest]){
						highest = i;
					}
				}
			}
		}

		if (num_mana >= 0){	// Able to pay all the colored costs - return a color if there are any needed, or else anything if there's colorless needed, or else prompt
			if (highest >= 0){
				return highest;
			} else if (always_prompt_for_color){
				highest = choose_any_color_from_available(available);
				if ((charging[COLOR_COLORLESS] > 0 || charging[COLOR_ARTIFACT] > 0) && !always_prompt_for_color){
					return highest;
				} else {
					return choose_a_color_exe(player, prompt, 1, highest, available);
				}
			}
		}
	}

	if (num_available_needed == 1){	// paying a cost, and only one matching noncolorless is unsatisfied - autochoose that
		return highest;
	}

	if (highest == -1 && !always_prompt_for_color){	// no colored mana cost is unsatisfied, so just pick first available as default/ai choice
		highest = choose_any_color_from_available(available);

		if (needed_mana_colors > 0
			&& (charging[COLOR_COLORLESS] > 0 || charging[COLOR_ARTIFACT] > 0)){	// paying a cost with at least 1 colorless unsatisfied, so autochoose this
			return highest;
		}
	}

	// Either not paying a cost, or at least two needed colors are available.  AI chooses highest amount needed; player gets prompted.
	return choose_a_color_exe(player, prompt, 1, highest, available);
}

/* As produce_mana_tapped(), but num_mana of any combination of the colors set in available.  If prompt is NULL, one will be constructed on-the-fly for each
 * choose_a_color dialog, showing amount of mana left and colors chosen so far.  Cancellable. */

/* As produce_mana_tapped_any_combination_of_colors(), but doesn't tap card, account for Contamination, or set tapped_for_mana_color.  Cancellable. */

// As produce_mana_tapped_all_one_color(), but if no specific color is needed and default_color is produceable, produce that.  Cancellable.

// As produce_mana_tapped(), but num_mana all of the same color (chosen from those set in available).  Cancellable.

// As produce_mana_tapped_x_all_one_color(), but doesn't tap card, account for Contamination, or set tapped_for_mana_color.  Cancellable.

/* Produces amount of mana in any combination of the types put into tapped_for_mana_color/tapped_for_mana[].  This is the back-end function of Mana Flare and
 * similar effects. */

/* Produces amount of color mana when allowed_player taps a card of type and subtype for mana.  allowed_player may be 2 to work for both players; as usual,
 * subtype may be 0 or -1 to ignore subtype.  This is the back-end function of High Tide and similar effects. */

typedef enum {
	PM_FIXED,
	PM_ALL_ONE,
	PM_ANY_COMBO
} produce_mana_t;

// An enchant land that produces a specific amount of color of mana when enchanted land is tapped for mana.

// As wild_growth_aura(), but produces num_mana of any one color set in available.

// As wild_growth_aura(), but produces num_mana of any combination of colors set in available.

/* A generic creature that taps to produce mana, all of a single constant color.  If reluctance_to_fight is nonzero, it will tend not to attack or block if its
 * controller controls few lands of matching color.  Higher numbers are more reluctant; Llanowar Elves and Birds of Paradise use 24. */

// As mana_producing_creature(), but produces multiple colors.  reluctance_to_fight goes by total number of lands.

// As mana_producing_creature(), but produces num_mana all of a single color in available.  reluctance_to_fight goes by total number of lands.

// A generic artifact that taps to produce mana of its mana_color.  If sac is nonzero, then sacrifices itself when activated.

/* Implements "X you control have 'T: Add Y to your mana pool.'", where Y is a specific color and amount of mana.  Set subtype to -1 if just checking for a
 * type, as usual. (Example: Sachi, Daughter of Seshiro - X is Shamans, and Y is |G|G.) */
// A reasonable next step is to implement permanents_anyone_controls_can_tap_for_mana(), using a scheme similar to Iff-Biff Efreet.

// As permanents_you_control_can_tap_for_mana(), but choice of colors set in available.  (Example: Overlaid Terrain - Lands, and 2 of COLOR_TEST_ANY_COLORED.)

/* Implements "Tap an untapped X you control: Add Y to your mana pool.", where Y is a specific color and amount of mana. (Example: Seton, Krosan Protector - X
 * is Druid and Y is |G.)  Differs from permanents_you_control_can_tap_for_mana() in that 1. this is correct, not an approximation; and 2. it can tap
 * summoning-sick creatures. */

// This will only be correct for lands that are in play; info_slot isn't generally set while a land's still in hand

/* ManalinkEh 2022: 0x266f0c0; original functions/produce_mana.c:123. */
int plain_mana_producer(int player, int card, event_t event)
{
  int result;

  if ((event == EVENT_QUERY) && (attacking_card == 3))
  {
    return 1;
  }
  result = mana_producer(player, card, event);
  return result;
}

// Stores last-hacked color_test in info_slot.  No avoiding it; it's needed for get_color_of_mana_produced_by_id(), which doesn't get a card ref.

/* A specific amount of a specific color of mana already in player's mana pool doesn't drain until end of turn.  Unlike MANADRAIN_DOESNT_DRAIN and
 * MANADRAIN_BECOMES_COLORLESS, this should be called immediately after adding the mana to the pool, not during EVENT_MANA_POOL_DRAINING. */

int finalize_activation(int player, int card);

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */

/* ManalinkEh 2022: 0x266f160; original functions/produce_mana.c:161. */
int plain_mana_producer_tapped(int player, int card, event_t event)
{
  int result;

  if ((event == EVENT_QUERY) && (attacking_card == 3))
  {
    return 1;
  }
  result = mana_producer_tapped(player, card, event);
  return result;
}

/* Recovered October 2022 implementations; see docs/recovery-progress.md. */
int plain_mana_producer(int player, int card, event_t event);
int plain_mana_producer_tapped(int player, int card, event_t event);

/* ManalinkEh 2022: 0x266d010; original functions/produce_mana.c:262. */
void produce_mana_tapped(int player, int card, color_t color, int amount)
{
  produce_mana_tapped_impl(player, card, color, amount, 1, 1);
  return;
}

/* ManalinkEh 2022: 0x266d970; original functions/produce_mana.c:328. */
void produce_mana_tapped2(int player, int card, color_t color1, int amount1, color_t color2,
                               int amount2)
{
  produce_mana_tapped2_impl(player, card, color1, amount1, color2, amount2, 1);
  return;
}

/* ManalinkEh 2022: 0x266e590; original functions/produce_mana.c:441. */
void produce_mana_tapped_multi(int player, int card, int colorless, int black, int blue,
                                    int green, int red, int white)
{
  produce_mana_tapped_multi_impl(player, card, colorless, black, blue, green, red, white, 1);
  return;
}

/* October 2022 tapped-mana producer component. */
int produce_mana_tapped_all_one_color_with_default(int player, int card,
                                                        color_test_t available, int num_mana,
                                                        color_test_t default_color);
int produce_mana_tapped_any_combination_of_colors(int player, int card, color_test_t available,
                                                       int num_mana, const char *prompt);
int produce_mana_tapped_all_one_color(int player, int card, color_test_t available,
                                           int num_mana);
void produce_mana_tapped(int player, int card, color_t color, int amount);
void produce_mana_tapped2(int player, int card, color_t color1, int amount1, color_t color2,
                               int amount2);
void produce_mana_tapped_multi(int player, int card, int colorless, int black, int blue,
                                    int green, int red, int white);
int produce_mana_tapped_all_different_colors(int player, int card, color_test_t available,
                                                  int num_mana, const char *prompt);
int produce_mana_tapped_all_one_color_impl(int player, int card, color_test_t available,
                                           int num_mana, int mca);
int produce_mana_tapped_all_one_color_with_default_impl(int player, int card,
                                                        color_test_t available, int num_mana,
                                                        color_test_t default_color, int mca,
                                                        int contaminate);
void produce_mana_tapped_impl(int player, int card, color_t color, int amount, int mca,
                              int contaminate);
void produce_mana_tapped2_impl(int player, int card, color_t color1, int amount1, color_t color2,
                               int amount2, int mca);
void produce_mana_tapped_multi_impl(int player, int card, int colorless, int black, int blue,
                                    int green, int red, int white, int mca);
int produce_mana_tapped_any_combination_of_colors_impl(int player, int card, color_test_t available,
                                                       int num_mana, const char *prompt, int mca);
static int produce_mana_tapped_all_different_colors_impl(int player, int card,
    color_test_t available, int num_mana, const char *prompt, int mca);

/* ManalinkEh 2022: 0x266ecc0; original functions/produce_mana.c:674. */
int produce_mana_tapped_any_combination_of_colors(int player, int card, color_test_t available,
                                                       int num_mana, const char *prompt)
{
  int result;
  result = produce_mana_tapped_any_combination_of_colors_impl(player, card, available, num_mana,
                                                              prompt, 1);
  return result;
}

/* ManalinkEh 2022: 0x2671510; original functions/produce_mana.c:784. */
int produce_mana_tapped_all_different_colors(int player, int card, color_test_t available,
                                                  int num_mana, const char *prompt)
{
  int result;
  result =
      produce_mana_tapped_all_different_colors_impl(player, card, available, num_mana, prompt, 1);
  return result;
}

/* ManalinkEh 2022: 0x266ae20; original functions/produce_mana.c:788. */
int choose_mana_all_one_color(int player, color_test_t available, int num_mana,
    const char *prompt, color_test_t default_color)
{
  if (num_mana <= 0)
  {
    if (!(available & COLOR_TEST_COLORLESS))
      for (int color = 1; color < 7; ++color)
        if (available & (1U << color))
          return color;
    return COLOR_COLORLESS;
  }

  int charging[7] = {0};
  int best = -1;
  int remaining = num_mana;
  if (needed_mana_colors > 0)
  {
    for (int color = 0; color < 7; ++color)
    {
      int needed = (*charge_mana_addr_of_pay_mana_xbugrw)[color];
      if (needed == -1)
        charging[color] = 1000;
      else
      {
        if (charge_mana_addr_of_pre_mana_pool != NULL)
          needed = (needed + (*charge_mana_addr_of_pre_mana_pool)[color]) - mana_pool[player][color];
        charging[color] = needed;
      }
    }
    int colored_needs = 0;
    for (int color = 0; color < 7; ++color)
    {
      if (charging[color] < 0)
      {
        if (color != COLOR_ARTIFACT && charging[COLOR_ARTIFACT] > 0)
        {
          int paid = -charging[color];
          if (charging[COLOR_ARTIFACT] < paid)
            paid = charging[COLOR_ARTIFACT];
          charging[COLOR_ARTIFACT] -= paid;
          charging[color] += paid;
          if (charging[color] >= 0)
            continue;
        }
        if (color != COLOR_COLORLESS && charging[COLOR_COLORLESS] > 0)
        {
          int paid = -charging[color];
          if (charging[COLOR_COLORLESS] < paid)
            paid = charging[COLOR_COLORLESS];
          charging[COLOR_COLORLESS] -= paid;
          charging[color] += paid;
        }
      }
      else if (charging[color] > 0 && color != COLOR_COLORLESS && color != COLOR_ARTIFACT &&
          (available & (1U << color)))
      {
        ++colored_needs;
        remaining -= charging[color];
        if (best == -1 || charging[best] < charging[color])
          best = color;
      }
    }
    // A single colored need is selected without prompting.
    if (colored_needs == 1)
      return best;
    if (best != -1)
      return choose_a_color_exe(player, prompt, 1, best, available);
  }

  if (!always_prompt_for_color)
  {
    color_test_t preferred = default_color & available;
    if (!preferred)
      preferred = available;
    best = COLOR_COLORLESS;
    if (!(preferred & COLOR_TEST_COLORLESS))
      for (int color = 1; color < 7; ++color)
        if (preferred & (1U << color))
        {
          best = color;
          break;
        }
    int generic = charging[COLOR_COLORLESS] > 0 ? charging[COLOR_COLORLESS] : 0;
    int artifact = charging[COLOR_ARTIFACT] > 0 ? charging[COLOR_ARTIFACT] : 0;
    if (needed_mana_colors > 0 && remaining <= generic + artifact)
      return best;
  }
  return choose_a_color_exe(player, prompt, 1, best, available);
}

/* ManalinkEh 2022: 0x266c500; original functions/produce_mana.c:956. */
int produce_mana_tapped_all_one_color_with_default(int player, int card,
                                                        color_test_t available, int num_mana,
                                                        color_test_t default_color)
{
  int result;
  result = produce_mana_tapped_all_one_color_with_default_impl(player, card, available, num_mana,
                                                               default_color, 1, 1);
  return result;
}

/* ManalinkEh 2022: 0x266c540; original functions/produce_mana.c:960. */
int produce_mana_tapped_all_one_color_impl(int player, int card, color_test_t available,
                                           int num_mana, int mca)
{
  int result;
  result = produce_mana_tapped_all_one_color_with_default_impl(player, card, available, num_mana,
                                                               COLOR_TEST_COLORLESS, mca, 1);
  return result;
}

/* ManalinkEh 2022: 0x266c580; original functions/produce_mana.c:964. */
int produce_mana_tapped_all_one_color(int player, int card, color_test_t available,
                                           int num_mana)
{
  int result;
  result = produce_mana_tapped_all_one_color_with_default_impl(player, card, available, num_mana,
                                                               COLOR_TEST_COLORLESS, 1, 1);
  return result;
}

int get_color_of_mana_produced_by_id(int csvid, int info_slot, int player)
{
  int index = get_internal_card_id_from_csv_id(csvid);
  int count;
  int initializing;
  card_data_t *land_data;
  unsigned int colors;
  if (multi_mana_transmutation[player][0])
  {
    count = mana_doublers(player, TYPE_LAND);
    if (count > 1 && (cards_data[index].type & TYPE_LAND) &&
        ((cards_data[index].extra_ability & EA_MANA_SOURCE) || mana_colors_added[player]))
      return multi_mana_transmutation[player][0];
    colors = multi_mana_transmutation[player][0];
    if (colors)
    {
      switch (csvid)
      {
      case CARD_ID_CABAL_COFFERS:
        index = count_subtype(player, TYPE_LAND, SUBTYPE_SWAMP);
        return index < 2 ? -1 : multi_mana_transmutation[player][0];

      case CARD_ID_TOLARIAN_ACADEMY:
        index = count_subtype(player, TYPE_ARTIFACT, -1);
        return index < 2 ? -1 : multi_mana_transmutation[player][0];

      case CARD_ID_GAEAS_CRADLE:
        index = count_subtype(player, TYPE_CREATURE, -1);
        return index < 2 ? -1 : multi_mana_transmutation[player][0];

      case CARD_ID_ORZHOV_BASILICA:
      case CARD_ID_GOLGARI_ROT_FARM:
      case CARD_ID_DIMIR_AQUEDUCT:
      case CARD_ID_GRUUL_TURF:
      case CARD_ID_IZZET_BOILERWORKS:
      case CARD_ID_AZORIUS_CHANCERY:
      case CARD_ID_RAKDOS_CARNARIUM:
      case CARD_ID_SIMIC_GROWTH_CHAMBER:
      case CARD_ID_BALDUVIAN_TRADING_POST:
      case CARD_ID_SCHOOL_OF_THE_UNSEEN:
      case CARD_ID_SOLDEVI_EXCAVATIONS:
      case CARD_ID_TEFERIS_ISLE:
      case CARD_ID_CORAL_ATOLL:
      case CARD_ID_DORMANT_VOLCANO:
      case CARD_ID_EVERGLADES:
      case CARD_ID_JUNGLE_BASIN:
      case CARD_ID_KAROO:
      case CARD_ID_LOTUS_VALE:
      case CARD_ID_PHYREXIAN_TOWER:
      case CARD_ID_HICKORY_WOODLOT:
      case CARD_ID_PEAT_BOG:
      case CARD_ID_REMOTE_FARM:
      case CARD_ID_SANDSTONE_NEEDLE:
      case CARD_ID_SAPRAZZAN_SKERRY:
      case CARD_ID_SHIVAN_REEF:
      case CARD_ID_CRYSTAL_QUARRY:
      case CARD_ID_DARKWATER_CATACOMBS:
      case CARD_ID_MOSSFIRE_VALLEY:
      case CARD_ID_SHADOWBLOOD_RIDGE:
      case CARD_ID_SKYCLOUD_EXPANSE:
      case CARD_ID_SUNGRASS_PRAIRIE:
      case CARD_ID_GRAVEN_CAIRNS:
      case CARD_ID_FIRE_LIT_THICKET:
      case CARD_ID_MYSTIC_GATE:
      case CARD_ID_SUNKEN_RUINS:
      case CARD_ID_WOODED_BASTION:
      case CARD_ID_CASCADE_BLUFFS:
      case CARD_ID_FETID_HEATH:
      case CARD_ID_FLOODED_GROVE:
      case CARD_ID_RUGGED_PRAIRIE:
      case CARD_ID_TWILIGHT_MIRE:
      case CARD_ID_CASCADING_CATARACTS:
      case CARD_ID_LOST_VALE:
      case CARD_ID_GOLD_FORGE_GARRISON:
        return colors;

      case CARD_ID_BOTTOMLESS_VAULT:
      case CARD_ID_EBON_STRONGHOLD:
      case CARD_ID_SULFUR_VENT:
        return colors | COLOR_TEST_BLACK;

      case CARD_ID_DWARVEN_HOLD:
      case CARD_ID_DWARVEN_RUINS:
      case CARD_ID_GEOTHERMAL_CREVICE:
        return colors | COLOR_TEST_RED;

      case CARD_ID_HAVENWOOD_BATTLEGROUND:
      case CARD_ID_HOLLOW_TREES:
      case CARD_ID_TINDER_FARM:
        return colors | COLOR_TEST_GREEN;

      case CARD_ID_ICATIAN_STORE:
      case CARD_ID_RUINS_OF_TROKAIR:
      case CARD_ID_IRRIGATION_DITCH:
        return colors | COLOR_TEST_WHITE;

      case CARD_ID_SAND_SILOS:
      case CARD_ID_SVYELUNITE_TEMPLE:
      case CARD_ID_ANCIENT_SPRING:
        return colors | COLOR_TEST_BLUE;

      case CARD_ID_SERRAS_SANCTUM:
        index = count_subtype(player, TYPE_ENCHANTMENT, -1);
        return index < 2 ? -1 : multi_mana_transmutation[player][0];

      case CARD_ID_ITLIMOC_CRADLE_OF_THE_SUN:
        index = count_subtype(player, TYPE_CREATURE, -1);
        return index < 2 ? -1 : multi_mana_transmutation[player][0] | COLOR_TEST_COLORLESS | COLOR_TEST_BLACK;

      case CARD_ID_SANCTUM_OF_THE_SUN:
        return life[player] < 2 ? -1 : colors;

      case CARD_ID_VAULT_OF_CATLACAN:
        index = count_subtype(player, TYPE_ARTIFACT, -1);
        return index < 2 ? -1 : multi_mana_transmutation[player][0] | COLOR_TEST_BLACK;

      case CARD_ID_CABAL_STRONGHOLD:
        index = count_basic_lands(player, SUBTYPE_SWAMP);
        return index < 2 ? -1 : multi_mana_transmutation[player][0];

      default:
        break;
      }
    }
  }
  switch (csvid)
  {
  case CARD_ID_CHANNEL:
  case CARD_ID_DOUBLING_CUBE:
  case CARD_ID_ELEMENTAL_RESONANCE:
  case CARD_ID_CAVERN_OF_SOULS:
    return 1;

  case CARD_ID_FELLWAR_STONE:
  case CARD_ID_EXOTIC_ORCHARD:
  case CARD_ID_BENTHIC_EXPLORERS:
  case CARD_ID_QUIRION_EXPLORER:
  case CARD_ID_SYLVOK_EXPLORER:
    player = 1 - player;
    break;

  case CARD_ID_GEM_BAZAAR:
  case CARD_ID_SOL_GRAIL:
  case CARD_ID_MIRRODINS_CORE:
  case CARD_ID_UTOPIA_SPRAWL:
  case CARD_ID_COLDSTEEL_HEART:
  case CARD_ID_PARADISE_PLUME:
  case CARD_ID_VERDANT_HAVEN:
  case CARD_ID_SHIMMERDRIFT_VALE:
    return info_slot < 0 ? COLOR_TEST_ANY_COLORED : info_slot;

  case CARD_ID_STAR_COMPASS:
  case CARD_ID_SQUANDERED_RESOURCES:
  case CARD_ID_REFLECTING_POOL:
  case CARD_ID_NAGA_VITALIST:
  case CARD_ID_INCUBATION_DRUID:
  case CARD_ID_PLAZA_OF_HARMONY:
    break;

  case CARD_ID_AN_HAVVA_TOWNSHIP:
  case CARD_ID_AYSEN_ABBEY:
  case CARD_ID_CASTLE_SENGIR:
  case CARD_ID_KOSKUN_KEEP:
  case CARD_ID_WIZARDS_SCHOOL:
  case CARD_ID_GRAVEN_CAIRNS:
  case CARD_ID_FIRE_LIT_THICKET:
  case CARD_ID_MYSTIC_GATE:
  case CARD_ID_SUNKEN_RUINS:
  case CARD_ID_WOODED_BASTION:
  case CARD_ID_BLOOM_TENDER:
  case CARD_ID_CASCADE_BLUFFS:
  case CARD_ID_FETID_HEATH:
  case CARD_ID_FLOODED_GROVE:
  case CARD_ID_RUGGED_PRAIRIE:
  case CARD_ID_TWILIGHT_MIRE:
  case CARD_ID_COMMAND_TOWER:
  case CARD_ID_OPAL_PALACE:
  case CARD_ID_COMMANDERS_SPHERE:
  case CARD_ID_PATH_OF_ANCESTRY:
  case CARD_ID_ARCANE_SIGNET:
  case CARD_ID_FAEBURROW_ELDER:
  case CARD_ID_GENERIC_ANIMATED_LAND:
    return info_slot;

  case CARD_ID_SCHOOL_OF_THE_UNSEEN:
  case CARD_ID_TENDO_ICE_BRIDGE:
  case CARD_ID_PRISMATIC_LENS:
  case CARD_ID_SHIMMERING_GROTTO:
  case CARD_ID_NYKTHOS_SHRINE_TO_NYX:
  case CARD_ID_UNKNOWN_SHORES:
  case CARD_ID_AETHER_HUB:
  case CARD_ID_CASCADING_CATARACTS:
    return info_slot < 0 ? COLOR_TEST_ANY : info_slot;

  case CARD_ID_QUIRION_ELVES:
  case CARD_ID_SILHANA_STARFLETCHER:
  case CARD_ID_VIVID_GROVE:
    return (-1) < info_slot ? info_slot : 8;

  case CARD_ID_PHYREXIAN_TOWER:
  case CARD_ID_CABAL_STRONGHOLD:
    return info_slot < 0 ? 3 : info_slot;

  case CARD_ID_TAINTED_FIELD:
    return info_slot < 0 ? 35 : info_slot;

  case CARD_ID_TAINTED_ISLE:
    return info_slot < 0 ? 7 : info_slot;

  case CARD_ID_TAINTED_PEAK:
    return info_slot < 0 ? 19 : info_slot;

  case CARD_ID_TAINTED_WOOD:
    return info_slot < 0 ? 11 : info_slot;

  case CARD_ID_CHROME_MOX:
  case CARD_ID_HONORED_HIERARCH:
  case CARD_ID_CORRUPTED_GRAFSTONE:
    return info_slot < 0 ? 0 : info_slot;

  case CARD_ID_GEMSTONE_CAVERNS:
    return info_slot < 0 ? 1 : info_slot;

  case CARD_ID_URBORG_TOMB_OF_YAWGMOTH:
    return info_slot < 0 ? 2 : 1U >> (info_slot & 31);

  case CARD_ID_NIMBUS_MAZE:
    return info_slot < 0 ? 37 : info_slot;

  case CARD_ID_RIVER_OF_TEARS:
    return info_slot < 0 ? 6 : info_slot;

  case CARD_ID_VIVID_CRAG:
    return (-1) < info_slot ? info_slot : 16;

  case CARD_ID_VIVID_CREEK:
    return (-1) < info_slot ? info_slot : 4;

  case CARD_ID_VIVID_MARSH:
    return (-1) < info_slot ? info_slot : 2;

  case CARD_ID_VIVID_MEADOW:
    return (-1) < info_slot ? info_slot : 32;

  case CARD_ID_SPRINGJACK_PASTURE:
    return (-1) < info_slot ? (info_slot & 0x40000000U) ? COLOR_TEST_ANY : COLOR_TEST_COLORLESS : 1;

  case CARD_ID_MOX_AMBER:
    if (info_slot == -1)
      return COLOR_TEST_ANY_COLORED;
    colors = 0;
    if (active_cards_count[player] > 0)
    {
      index = 0;
      do
      {
        land_data = get_card_data(player, index);
        if ((((land_data->type & TYPE_CREATURE) && is_legendary(player, index)) ||
             is_planeswalker(player, index)) && in_play(player, index))
          colors |= get_color(player, index);
        ++index;
      } while (index < active_cards_count[player] &&
               (colors & COLOR_TEST_ANY) != COLOR_TEST_ANY);
    }
    return colors & ~(COLOR_TEST_ARTIFACT | COLOR_TEST_COLORLESS);

  case CARD_ID_THRIVING_BLUFF:
    initializing = info_slot < 0;
    return initializing ? 16 : info_slot | 0x10;

  case CARD_ID_THRIVING_GROVE:
    initializing = info_slot < 0;
    return initializing ? 8 : info_slot | 8;

  case CARD_ID_THRIVING_HEATH:
    initializing = info_slot < 0;
    return initializing ? 32 : info_slot | 0x20;

  case CARD_ID_THRIVING_ISLE:
    initializing = info_slot < 0;
    return initializing ? 4 : info_slot | 4;

  case CARD_ID_THRIVING_MOOR:
    initializing = info_slot < 0;
    return initializing ? 2 : info_slot | 2;

  default:
    return -1;
  }
  if (info_slot == -1)
    return COLOR_TEST_ANY_COLORED;
  colors = 0;
  if (active_cards_count[player] > 0)
  {
    index = 0;
    do
    {
      land_data = get_card_data(player, index);
      if ((land_data->type & TYPE_LAND) && in_play(player, index))
      {
        int land_id = land_data->id;
        if (land_id != CARD_ID_NAGA_VITALIST &&
            land_id != CARD_ID_REFLECTING_POOL && land_id != CARD_ID_PLAZA_OF_HARMONY)
        {
          int eligible = 1;
          if (csvid == CARD_ID_STAR_COMPASS)
            eligible = has_subtype(player, index, SUBTYPE_BASIC);
          else if (csvid == CARD_ID_PLAZA_OF_HARMONY)
            eligible = has_subtype(player, index, SUBTYPE_GATE);
          if (eligible)
            colors |= get_colors_of_mana_land_could_produce_ignoring_costs(player, index);
        }
      }
      ++index;
    } while (index < active_cards_count[player] &&
             (colors & COLOR_TEST_ANY) != COLOR_TEST_ANY);
  }
  if (csvid == CARD_ID_REFLECTING_POOL || csvid == CARD_ID_SQUANDERED_RESOURCES ||
      csvid == CARD_ID_NAGA_VITALIST || csvid == CARD_ID_INCUBATION_DRUID ||
      csvid == CARD_ID_BENTHIC_EXPLORERS || csvid == CARD_ID_PLAZA_OF_HARMONY)
    return (colors & COLOR_TEST_ARTIFACT) ?
        (colors & ~COLOR_TEST_ARTIFACT) | COLOR_TEST_COLORLESS : colors;
  return colors & ~(COLOR_TEST_ARTIFACT | COLOR_TEST_COLORLESS);
}

/* ManalinkEh 2022: 0x266bbc0; original functions/produce_mana.c:2035. */
color_test_t basic_subtype_to_color_test(subtype_t subtype)
{
  switch (subtype)
  {
  case SUBTYPE_SWAMP: return COLOR_TEST_BLACK;
  case SUBTYPE_ISLAND: return COLOR_TEST_BLUE;
  case SUBTYPE_FOREST: return COLOR_TEST_GREEN;
  case SUBTYPE_MOUNTAIN: return COLOR_TEST_RED;
  case SUBTYPE_PLAINS: return COLOR_TEST_WHITE;
  default: return COLOR_TEST_0;
  }
}

/* ManalinkEh 2022: 0x266c040; original functions/produce_mana.c:2137. */
int mana_doublers(int player, type_t type)
{
  int multiplier = 1;
  for (int card = 0; card < active_cards_count[player]; ++card)
    if (in_play(player, card) && !is_humiliated(player, card, AL_STATIC) &&
        (type & TYPE_PERMANENT) != TYPE_NONE)
    {
      if (get_id(player, card) == CARD_ID_MANA_REFLECTION)
        multiplier *= 2;
      else if (get_id(player, card) == CARD_ID_NYXBLOOM_ANCIENT)
        multiplier *= 3;
    }
  return multiplier;
}

/* ManalinkEh 2022: 0x266c120; original functions/produce_mana.c:882. */
int produce_mana_tapped_all_one_color_with_default_impl(int player, int card,
    color_test_t available, int num_mana, color_test_t default_color, int mca, int contaminate)
{
  bool choose_color = true;
  chosen_colors = COLOR_TEST_0;
  spell_fizzled = 0;
  if (is_what(player, card, TYPE_LAND) && mca && mana_colors_added[player])
  {
    if (num_mana == 1)
    {
      produce_mana_tapped_all_one_color_impl(player, card,
          mana_colors_added[player] | available, 1, 0);
      return 1;
    }
    if (num_mana > 1)
    {
      int choice = do_dialog(player, player, card, -1, -1,
          " Basic land ability\n Multiple Mana ability\n Cancel", 1);
      if (choice == 0)
      {
        produce_mana_tapped_all_one_color_impl(player, card, mana_colors_added[player], 1, 0);
        return 1;
      }
      if (choice == 2)
      {
        spell_fizzled = 1;
        return 0;
      }
      choose_color = choice == 1;
    }
  }
  if (choose_color)
  {
    int color = choose_mana_all_one_color(player, available, num_mana,
        "What kind of mana?", default_color);
    if (spell_fizzled == 1 || color == -1)
    {
      spell_fizzled = 1;
      return 0;
    }
    if (multi_mana_transmutation[player][0] &&
        (num_mana > 1 || (num_mana == 1 && mana_doublers(player, TYPE_LAND) >= 2)) &&
        is_what(player, card, TYPE_LAND))
    {
      color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
      produce_mana(player, color, multi_mana_transmutation[player][1]);
      tapped_for_mana_color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
    }
    else
    {
      if (get_id(player, card) == CARD_ID_NYX_LOTUS)
        num_mana = devotion(player, -1, color, 0);
      int contamination = check_special_flags2(player, card, SF2_CONTAMINATION);
      if (contamination && card >= 0 && contaminate)
      {
        if (mana_quantity_override[player][color] >= 0)
          num_mana = mana_quantity_override[player][color];
        color = mana_color_override[player][color];
      }
      if (num_mana > 0)
      {
        produce_mana(player, color, num_mana);
        chosen_colors |= 1U << (color & 31);
      }
      for (int i = 0; i < 7; ++i)
        tapped_for_mana[i] = 0;
      tapped_for_mana[color] = num_mana;
      tapped_for_mana_color = num_mana == 1 ? color : 0x100;
      if (player == AI && !(trace_mode & 2) && ai_is_speculating != 1)
      {
        load_text(NULL, "FELLWAR_STONE");
        int line = color - 1U < 6 ? color : 6;
        do_dialog(AI, AI, card, -1, -1, text_lines[line], 0);
      }
    }
  }
  if (card >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= STATE_TAPPED;
  }
  return 1;
}

/* ManalinkEh 2022: 0x266cbf0; original functions/produce_mana.c:191. */
void produce_mana_tapped_impl(int player, int card, color_t color, int amount, int mca,
                              int contaminate)
{
  int flags = check_special_flags2(player, card, SF2_QUARUM_TRENCH_GNOMES);
  if (flags && color == COLOR_WHITE && card >= 0 && is_what(player, card, TYPE_LAND))
  {
    produce_mana(player, COLOR_COLORLESS, 1);
    tapped_for_mana_color = COLOR_COLORLESS;
  }
  else
  {
    flags = check_special_flags2(player, card, SF2_DEEP_WATER);
    if (flags && card >= 0 && is_what(player, card, TYPE_LAND))
    {
      produce_mana(player, COLOR_BLUE, 1);
      tapped_for_mana_color = COLOR_BLUE;
    }
    else
    {
      bool transmute = false;
      if (!is_what(player, card, TYPE_LAND) || !mana_colors_added[player] || !mca)
      {
        transmute = amount > 1 || (amount == 1 && mana_doublers(player, TYPE_LAND) > 1);
      }
      else if (amount == 1)
      {
        color_test_t available = color_to_color_test(color);
        produce_mana_tapped_all_one_color_with_default_impl(player, card,
            available | mana_colors_added[player], 1, COLOR_TEST_COLORLESS, 0, contaminate);
        return;
      }
      else if (amount > 1)
      {
        int choice = do_dialog(player, player, card, -1, -1,
            " Basic land ability\n Multiple Mana ability\n Cancel", 1);
        if (choice == 0)
        {
          produce_mana_tapped_all_one_color_with_default_impl(player, card,
              mana_colors_added[player], 1, COLOR_TEST_COLORLESS, 0, contaminate);
          return;
        }
        if (choice == 2)
        {
          spell_fizzled = 1;
          return;
        }
        transmute = true;
      }

      if (transmute && is_what(player, card, TYPE_LAND) && multi_mana_transmutation[player][0])
      {
        int produced_color;
        if (!check_special_flags2(player, card, SF2_CONTAMINATION))
          produced_color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
        else
          produced_color = single_color_test_bit_to_color(
              mana_color_override[player][multi_mana_transmutation[player][0]]);
        produce_mana(player, produced_color, multi_mana_transmutation[player][1]);
        tapped_for_mana_color = produced_color;
        tapped_for_mana[produced_color] += multi_mana_transmutation[player][1] > 0 ?
            multi_mana_transmutation[player][1] : 0;
      }
      else
      {
        if (check_special_flags2(player, card, SF2_CONTAMINATION) && card >= 0)
        {
          if (mana_quantity_override[player][color] >= 0)
            amount = mana_quantity_override[player][color];
          if (contaminate && mana_color_override[player][color] != COLOR_COLORLESS)
            color = mana_color_override[player][color];
        }
        if (amount > 0)
          produce_mana(player, color, amount);
        for (int i = 0; i < 7; ++i)
          tapped_for_mana[i] = 0;
        tapped_for_mana[color] = amount > 0 ? amount : 0;
        tapped_for_mana_color = amount == 1 ? color : 0x100;
      }
    }
  }
  if (card >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= STATE_TAPPED;
  }
}

/* ManalinkEh 2022: 0x266d580; original functions/produce_mana.c:267. */
void produce_mana_tapped2_impl(int player, int card, color_t color1, int amount1, color_t color2,
    int amount2, int mca)
{
  if (mana_colors_added[player] && is_what(player, card, TYPE_LAND) && mca)
  {
    int choice = do_dialog(player, player, card, -1, -1,
        " Basic land ability\n Multi Mana ability\n Cancel", 1);
    if (choice == 0)
    {
      if (num_bits_set(mana_colors_added[player]) != 1)
        produce_mana_tapped_all_one_color_with_default_impl(player, card,
            mana_colors_added[player], 1, COLOR_TEST_COLORLESS, 0, 1);
      else
      {
        color1 = single_color_test_bit_to_color(mana_colors_added[player]);
        produce_mana_tapped_impl(player, card, color1, 1, 0, 1);
      }
      return;
    }
    if (choice == 2)
    {
      spell_fizzled = 1;
      return;
    }
  }
  if (color1 == color2)
  {
    produce_mana_tapped_impl(player, card, color1, amount1 + amount2, 0, 1);
    return;
  }
  if (multi_mana_transmutation[player][0] &&
      (amount1 + amount2 > 1 ||
       (amount1 + amount2 == 1 && mana_doublers(player, TYPE_LAND) >= 2)) &&
      is_what(player, card, TYPE_LAND))
  {
    int color;
    if (!check_special_flags2(player, card, SF2_CONTAMINATION))
      color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
    else
      color = single_color_test_bit_to_color(
          mana_color_override[player][multi_mana_transmutation[player][0]]);
    produce_mana(player, color, multi_mana_transmutation[player][1]);
    tapped_for_mana_color = color;
    tapped_for_mana[color] += multi_mana_transmutation[player][1] > 0 ?
        multi_mana_transmutation[player][1] : 0;
  }
  else
  {
    int second_amount = amount2 > 0 ? amount2 : 0;
    int contamination = check_special_flags2(player, card, SF2_CONTAMINATION);
    if (card >= 0 && contamination)
    {
      if (mana_quantity_override[player][color1] >= 0 ||
          mana_quantity_override[player][color2] >= 0)
      {
        second_amount = 0;
        amount2 = 0;
        amount1 = mana_quantity_override[player][color1];
      }
      color1 = mana_color_override[player][color1];
      color2 = mana_color_override[player][color2];
    }
    if (amount1 > 0)
      produce_mana(player, color1, amount1);
    if (amount2 > 0)
      produce_mana(player, color2, amount2);
    for (int color = 0; color < 7; ++color)
      tapped_for_mana[color] = 0;
    tapped_for_mana_color = 0x100;
    tapped_for_mana[color1] += amount1 > 0 ? amount1 : 0;
    tapped_for_mana[color2] += second_amount;
  }
  if (card >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= STATE_TAPPED;
  }
}

/* ManalinkEh 2022: 0x266dee0; original functions/produce_mana.c:333. */
void produce_mana_tapped_multi_impl(int player, int card, int colorless, int black, int blue,
                                    int green, int red, int white, int mca)
{
  int amounts[6] = {colorless, black, blue, green, red, white};
  int total = colorless + black + blue + green + red + white;
  bool transmute = false;
  if (!is_what(player, card, TYPE_LAND) || !mca || !mana_colors_added[player])
  {
    transmute = total > 1 || (total == 1 && mana_doublers(player, TYPE_LAND) > 1);
  }
  else if (total == 1)
  {
    produce_mana_tapped_all_one_color_with_default_impl(player, card,
        COLOR_TEST_COLORLESS, 1, COLOR_TEST_COLORLESS, 0, 1);
    return;
  }
  else if (total > 1)
  {
    int choice = do_dialog(player, player, card, -1, -1,
        " Basic land ability\n Multiple Mana ability\n Cancel", 1);
    if (choice == 0)
    {
      produce_mana_tapped_all_one_color_with_default_impl(player, card,
          mana_colors_added[player], 1, COLOR_TEST_COLORLESS, 0, 1);
      return;
    }
    if (choice == 2)
    {
      spell_fizzled = 1;
      return;
    }
    transmute = true;
  }

  if (transmute && multi_mana_transmutation[player][0])
  {
    int color;
    if (!check_special_flags2(player, card, SF2_CONTAMINATION))
      color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
    else
      color = single_color_test_bit_to_color(
          mana_color_override[player][multi_mana_transmutation[player][0]]);
    produce_mana(player, color, multi_mana_transmutation[player][1]);
    tapped_for_mana_color = color;
    tapped_for_mana[color] += multi_mana_transmutation[player][1] > 0 ?
        multi_mana_transmutation[player][1] : 0;
  }
  else
  {
    int contamination = check_special_flags2(player, card, SF2_CONTAMINATION);
    if (card >= 0 && contamination)
    {
      bool quantity_override = false;
      for (int color = 0; color < 7; ++color)
        if (mana_quantity_override[player][color] != 0)
        {
          produce_mana(player, mana_color_override[player][color],
              mana_quantity_override[player][color]);
          tapped_for_mana_color = mana_color_override[player][color];
          tapped_for_mana[tapped_for_mana_color] = mana_quantity_override[player][color];
          quantity_override = true;
          break;
        }
      if (!quantity_override)
      {
        for (int color = 0; color < 6; ++color)
          if (amounts[color] > 0)
            produce_mana(player, mana_color_override[player][color], amounts[color]);
        int white_color = mana_color_override[player][COLOR_WHITE];
        // The native bookkeeping applies the override map a second time.
        for (int color = 0; color < 5; ++color)
          tapped_for_mana[mana_color_override[player][mana_color_override[player][color]]] +=
              amounts[color] > 0 ? amounts[color] : 0;
        tapped_for_mana_color = 0x100;
        int artifact_color = mana_color_override[player][COLOR_ARTIFACT];
        tapped_for_mana[mana_color_override[player][white_color]] += white > 0 ? white : 0;
        tapped_for_mana[artifact_color] = 0;
      }
    }
    else
    {
      for (int color = 0; color < 6; ++color)
        if (amounts[color] > 0)
          produce_mana(player, color, amounts[color]);
      tapped_for_mana[COLOR_ARTIFACT] = 0;
      tapped_for_mana_color = 0x100;
      for (int color = 0; color < 6; ++color)
        tapped_for_mana[color] = amounts[color] > 0 ? amounts[color] : 0;
    }
  }
  if (card >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= STATE_TAPPED;
  }
}

/* ManalinkEh 2022: 0x266e8a0; original functions/produce_mana.c:594. */
int produce_mana_tapped_any_combination_of_colors_impl(int player, int card,
    color_test_t available, int num_mana, const char *prompt, int mca)
{
  int cols[7];
  chosen_colors = COLOR_TEST_0;
  spell_fizzled = 0;
  if (multi_mana_transmutation[player][0] && is_what(player, card, TYPE_LAND) &&
      (num_mana > 1 || (num_mana == 1 && mana_doublers(player, TYPE_LAND) >= 2)))
  {
    int color;
    if (!check_special_flags2(player, card, SF2_CONTAMINATION))
      color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
    else
      color = single_color_test_bit_to_color(
          mana_color_override[player][multi_mana_transmutation[player][0]]);
    produce_mana(player, color, multi_mana_transmutation[player][1]);
    tapped_for_mana_color = color;
    tapped_for_mana[color] += multi_mana_transmutation[player][1] > 0 ?
        multi_mana_transmutation[player][1] : 0;
  }
  else
  {
    bool choose_colors = true;
    if (is_what(player, card, TYPE_LAND) && mca && mana_colors_added[player])
    {
      if (num_mana == 1)
      {
        produce_mana_tapped_all_one_color_with_default_impl(player, card,
            mana_colors_added[player] | available, 1, COLOR_TEST_COLORLESS, 0, 1);
        return 1;
      }
      if (num_mana > 1)
      {
        int choice = do_dialog(player, player, card, -1, -1,
            " Basic land ability\n Multiple Mana ability\n Cancel", 1);
        if (choice == 0)
        {
          produce_mana_tapped_all_one_color_with_default_impl(player, card,
              mana_colors_added[player], 1, COLOR_TEST_COLORLESS, 0, 1);
          return 1;
        }
        if (choice == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        choose_colors = choice == 1;
      }
    }
    if (choose_colors)
    {
      for (int color = 0; color < 7; ++color)
        cols[color] = 0;
      choose_combination_of_colors_of_mana_to_produce_into_arr(
          cols, player, available, num_mana, prompt);
      if (spell_fizzled == 1)
        return 0;
      bool quantity_override = false;
      for (int color = 0; color < 7; ++color)
      {
        int amount = cols[color];
        if (amount > 0)
        {
          if (check_special_flags2(player, card, SF2_CONTAMINATION))
          {
            if (mana_quantity_override[player][color] != 0)
            {
              produce_mana(player, mana_color_override[player][color],
                  mana_quantity_override[player][color]);
              tapped_for_mana_color = mana_color_override[player][color];
              tapped_for_mana[tapped_for_mana_color] = mana_quantity_override[player][color];
              quantity_override = true;
              break;
            }
            produce_mana(player, mana_color_override[player][color], cols[color]);
          }
          else
          {
            if (color == COLOR_WHITE &&
                check_special_flags2(player, card, SF2_QUARUM_TRENCH_GNOMES))
            {
              produce_mana(player, COLOR_COLORLESS, cols[COLOR_COLORLESS]);
              chosen_colors |= COLOR_TEST_COLORLESS;
              tapped_for_mana[COLOR_WHITE] = cols[COLOR_WHITE] > 0 ? cols[COLOR_WHITE] : 0;
              continue;
            }
            produce_mana(player, color, cols[color]);
          }
          chosen_colors |= 1U << color;
          amount = cols[color];
        }
        tapped_for_mana[color] = amount > 0 ? amount : 0;
      }
      if (!quantity_override)
        tapped_for_mana_color = 0x100;
    }
  }
  if (card >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= STATE_TAPPED;
  }
  return 1;
}

/* ManalinkEh 2022: 0x2670e00; original functions/produce_mana.c:698. */
/* The release specializes mca to 1. */
static int produce_mana_tapped_all_different_colors_impl(int player, int card,
    color_test_t available, int num_mana, const char *prompt, int mca)
{
  bool transmute = false;
  bool check_added_colors = true;
  bool choose_colors = true;
  color_test_t added;
  int cols[7];
  chosen_colors = COLOR_TEST_0;
  spell_fizzled = 0;
  if (multi_mana_transmutation[player][0] && is_what(player, card, TYPE_LAND))
  {
    if (num_mana > 1 || (num_mana == 1 && mana_doublers(player, TYPE_LAND) > 1))
      transmute = true;
    else if (num_mana == 1)
    {
      check_added_colors = false;
      if (is_what(player, card, TYPE_LAND) && (added = mana_colors_added[player]) != COLOR_TEST_0)
      {
        produce_mana_tapped_all_one_color_with_default_impl(player, card,
            added | available, 1, COLOR_TEST_COLORLESS, 0, 1);
        return 1;
      }
    }
  }
  if (transmute)
  {
    int color;
    if (!check_special_flags2(player, card, SF2_CONTAMINATION))
      color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
    else
      color = single_color_test_bit_to_color(
          mana_color_override[player][multi_mana_transmutation[player][0]]);
    produce_mana(player, color, multi_mana_transmutation[player][1]);
    tapped_for_mana_color = color;
    tapped_for_mana[color] += multi_mana_transmutation[player][1] > 0 ?
        multi_mana_transmutation[player][1] : 0;
  }
  else
  {
    if (check_added_colors && is_what(player, card, TYPE_LAND) &&
        (added = mana_colors_added[player]) != COLOR_TEST_0)
    {
      if (num_mana == 1)
      {
        produce_mana_tapped_all_one_color_with_default_impl(player, card,
            added | available, 1, COLOR_TEST_COLORLESS, 0, 1);
        return 1;
      }
      if (num_mana > 1)
      {
        int choice = do_dialog(player, player, card, -1, -1,
            " Basic land ability\n Multiple Mana ability\n Cancel", 1);
        if (choice == 0)
        {
          produce_mana_tapped_all_one_color_with_default_impl(player, card,
              mana_colors_added[player], 1, COLOR_TEST_COLORLESS, 0, 1);
          return 1;
        }
        if (choice == 2)
        {
          spell_fizzled = 1;
          return 0;
        }
        choose_colors = choice == 1;
      }
    }
    if (choose_colors)
    {
      for (int color = 0; color < 7; ++color)
        cols[color] = 0;
      for (int i = 0; i < num_mana; ++i)
      {
        choose_combination_of_colors_of_mana_to_produce_into_arr(cols, player, available, 1, prompt);
        for (int color = 0; color < 7; ++color)
          if (cols[color] > 0)
            available &= ~color_to_color_test(color);
      }
      if (spell_fizzled == 1)
        return 0;
      bool quantity_override = false;
      for (int color = 0; color < 7; ++color)
      {
        int amount = cols[color];
        if (amount > 0)
        {
          if (check_special_flags2(player, card, SF2_CONTAMINATION))
          {
            if (mana_quantity_override[player][color] != 0)
            {
              produce_mana(player, mana_color_override[player][color],
                  mana_quantity_override[player][color]);
              tapped_for_mana_color = mana_color_override[player][color];
              tapped_for_mana[tapped_for_mana_color] = mana_quantity_override[player][color];
              quantity_override = true;
              break;
            }
            produce_mana(player, mana_color_override[player][color], cols[color]);
          }
          else
          {
            if (color == COLOR_WHITE &&
                check_special_flags2(player, card, SF2_QUARUM_TRENCH_GNOMES))
            {
              produce_mana(player, COLOR_COLORLESS, cols[COLOR_COLORLESS]);
              chosen_colors |= COLOR_TEST_COLORLESS;
              tapped_for_mana[COLOR_WHITE] = cols[COLOR_WHITE] > 0 ? cols[COLOR_WHITE] : 0;
              continue;
            }
            produce_mana(player, color, cols[color]);
          }
          chosen_colors |= 1U << color;
          amount = cols[color];
        }
        tapped_for_mana[color] = amount > 0 ? amount : 0;
      }
      if (!quantity_override)
        tapped_for_mana_color = 0x100;
    }
  }
  if (card >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    instance->state |= STATE_TAPPED;
  }
  return 1;
}

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266ab80; original functions/produce_mana.c:21. */
static void declare_mana_available_any_combination_of_colors_impl(int player, color_test_t available,
                                                                int amount)
{
  unsigned int num_bits = num_bits_set(available & (COLOR_TEST_ANY | COLOR_TEST_ARTIFACT));
  for (unsigned int i = 0; i < num_bits; ++i)
    if ((unsigned int)amount + i >= num_bits)
      declare_mana_available_hex(player, available, ((unsigned int)amount + i) / num_bits);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266abf0; original functions/produce_mana.c:21. */
void declare_mana_available_any_combination_of_colors(int player, color_test_t available,
                                                           int amount)
{
  if (amount > 0)
    declare_mana_available_any_combination_of_colors_impl(player, available, amount);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266d3f0; original functions/produce_mana.c:130. */
int mana_producer_fixed(int player, int card, event_t event, int base_color)
{
  if (event == EVENT_ACTIVATE)
  {
    if (is_what(player, card, 1))
    {
      int added = mana_colors_added[player];
      if (added != 0)
        return produce_mana_tapped_all_one_color_with_default_impl(
            player, card, added | color_to_color_test(base_color), 1, COLOR_TEST_COLORLESS, 0, 1);
      if (multi_mana_transmutation[player][0] != 0 && mana_doublers(player, TYPE_LAND) > 1)
      {
        color_t color = single_color_test_bit_to_color(multi_mana_transmutation[player][0]);
        produce_mana_tapped_impl(player, card, color, multi_mana_transmutation[player][1], 1, 1);
        return 0;
      }
    }
    int override = check_special_flags2(player, card, 0x40);
    if (card >= 0 && override && mana_quantity_override[player][base_color] != 0)
    {
      produce_mana_tapped_impl(player, card, mana_color_override[player][base_color],
                               mana_quantity_override[player][base_color], 1, 1);
      return 0;
    }
  }
  return engine_mana_producer_fixed(player, card, event, base_color);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266ac10; original functions/produce_mana.c:168. */
void produce_mana_multi(int player, int cless, int black, int blue, int green, int red,
                             int white)
{

  produce_mana(player, COLOR_COLORLESS, cless);
  produce_mana(player, COLOR_BLACK, black);
  produce_mana(player, COLOR_BLUE, blue);
  produce_mana(player, COLOR_GREEN, green);
  produce_mana(player, COLOR_RED, red);
  produce_mana(player, COLOR_WHITE, white);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266acc0; original functions/produce_mana.c:179. */
int single_color_test_bit_to_color(color_test_t col)
{
  return (col & COLOR_TEST_BLACK      ? COLOR_BLACK
          : col & COLOR_TEST_BLUE     ? COLOR_BLUE
          : col & COLOR_TEST_GREEN    ? COLOR_GREEN
          : col & COLOR_TEST_RED      ? COLOR_RED
          : col & COLOR_TEST_WHITE    ? COLOR_WHITE
          : col & COLOR_TEST_ARTIFACT ? COLOR_ARTIFACT
                                      : COLOR_COLORLESS);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266ad20; original functions/produce_mana.c:13. */
void declare_mana_available_maybe_hex(int player, color_test_t colors, int amount)
{
  if (num_bits_set(colors) == 1)
    declare_mana_available(player, single_color_test_bit_to_color(colors), amount);
  else
    declare_mana_available_hex(player, colors, amount);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266ed00; original functions/produce_mana.c:43. */
int mana_producer(int player, int card, event_t event)
{
  unsigned char printed_colors;
  card_instance_t *instance;
  int result;
  color_test_t color_mask;
  unsigned int color_value;
  color_t color;
  int amount;
  color_test_t available;
  int mana_amount;

  instance = get_card_instance(player, card);
  if (event == EVENT_CHANGE_TYPE)
  {
    if ((((card == affected_card) && (player == affected_card_controller)) &&
         (result = instance->targets[0xc].card, result != -1)) &&
        (instance->parent_card < 0))
    {
      event_result = result;
      return 0;
    }
  }
  else
  {
    switch (event)
    {
    case EVENT_CAST_SPELL:
      if ((card == affected_card) && (player == affected_card_controller))
      {
        ai_modifier = ai_modifier + 100;
        return 0;
      }
      break;
    case EVENT_ACTIVATE:
      result = can_produce_mana(player, card);
      if (result == 0)
      {
        spell_fizzled = 1;
        return 0;
      }
      printed_colors = instance->card_color;
      result = is_what(player, card, TYPE_LAND);
      available = printed_colors & (COLOR_TEST_ARTIFACT | COLOR_TEST_ANY);
      if (result != 0)
      {
        available = available | mana_colors_added[player];
      }
      color_value = num_bits_set(available);
      if ((color_value != 1) && (color_value = num_bits_set(available), color_value != 0))
      {
        produce_mana_tapped_all_one_color_with_default_impl(player, card, available, 1,
                                                            COLOR_TEST_COLORLESS, 1, 1);
        return 0;
      }
      amount = num_bits_set(available);
      color = single_color_test_bit_to_color(available);
      produce_mana_tapped_impl(player, card, color, amount, 0, 0);
      return 0;
    case EVENT_RESOLVE_SPELL:
      play_land_sound_effect(player, card);
      result = get_id(player, card);
      instance->targets[0xd].player = result;
      return 0;
    case EVENT_CAN_ACTIVATE:
      result = is_tapped(player, card);
      if ((result == 0) && (result = is_animated_and_sick(player, card), result == 0))
      {
        result = can_produce_mana(player, card);
        return (unsigned int)(result != 0);
      }
      break;
    case EVENT_COUNT_MANA:
      if (((card == affected_card) && (player == affected_card_controller)) &&
          ((result = is_tapped(player, card),
            result == 0 &&
                ((result = is_animated_and_sick(player, card),
                  result == 0 && (result = can_produce_mana(player, card), result != 0))))))
      {
        available = (color_test_t)instance->card_color;
        result = is_what(player, card, TYPE_LAND);
        if (result != 0)
        {
          available = available | mana_colors_added[player];
        }
        color_mask = check_special_flags2(player, card, SF2_QUARUM_TRENCH_GNOMES);
        if ((color_mask & available & COLOR_TEST_WHITE) != COLOR_TEST_0)
        {
          available = available & ~COLOR_TEST_WHITE | COLOR_TEST_COLORLESS;
        }
        result = check_special_flags2(player, card, SF2_DEEP_WATER);
        if (result != 0)
        {
          available = COLOR_TEST_BLUE;
        }
        result = check_special_flags2(player, card, SF2_CONTAMINATION);
        if (result != 0)
        {
          result = 0;
          do
          {
            color_mask = color_to_color_test(result);
            if ((color_mask & available) != COLOR_TEST_0)
            {
              color_value = color_to_color_test(result);
              color_mask = color_to_color_test(mana_color_override[player][result]);
              available = available & ~color_value | color_mask;
            }
            result = result + 1;
          } while (result != 7);
        }
        mana_amount = 1;
        if (((multi_mana_transmutation[player][0] != 0) &&
             (result = mana_doublers(player, TYPE_LAND), 1 < result)) &&
            (result = is_what(player, card, TYPE_LAND), result != 0))
        {
          mana_amount = multi_mana_transmutation[player][1];
          available = multi_mana_transmutation[player][0];
        }
        if ((available & (COLOR_TEST_ARTIFACT | COLOR_TEST_ANY)) != COLOR_TEST_0)
        {
          declare_mana_available_maybe_hex(
              player, available & (COLOR_TEST_ARTIFACT | COLOR_TEST_ANY), mana_amount);
          return 0;
        }
      }
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266f100; original functions/produce_mana.c:154. */
int mana_producer_tapped(int player, int card, event_t event)
{
  if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);
  return mana_producer(player, card, event);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266a640; original functions/produce_mana.c:540. */
static void choose_combination_of_colors_of_mana_to_produce_into_arr(int *cols, int player,
                                                                   color_test_t available,
                                                                   int num_mana, const char *prompt)
{
  char buf[300];
  int num_left = num_mana;
  for (; num_left > 0; --num_left)
  {
    if (prompt || player == 1 || ai_is_speculating == 1)
    {
      buf[0] = 0;
    }
    else
    {
      if (num_left == num_mana)
      {
        if (num_left == 1)
        {
          prompt = "What kind of mana?";
          buf[0] = 0;
        }
        else
        {
          sprintf(buf, "%d mana left", num_left);
        }
      }
      else
      {
        char *p = buf + sprintf(buf, "%d mana left (producing ", num_left);
        int i;
        if (cols[COLOR_COLORLESS] > 0)
        {
          p += sprintf(p, "%d", cols[COLOR_COLORLESS]);
        }
        for (i = 0; i < cols[COLOR_WHITE]; ++i)
        {
          *p++ = 'W';
        }
        for (i = 0; i < cols[COLOR_BLUE]; ++i)
        {
          *p++ = 'U';
        }
        for (i = 0; i < cols[COLOR_BLACK]; ++i)
        {
          *p++ = 'B';
        }
        for (i = 0; i < cols[COLOR_RED]; ++i)
        {
          *p++ = 'R';
        }
        for (i = 0; i < cols[COLOR_GREEN]; ++i)
        {
          *p++ = 'G';
        }
        for (i = 0; i < cols[COLOR_ARTIFACT]; ++i)
        {
          *p++ = 'A';
        }
        *p++ = ')';
        *p = 0;
      }
    }
    int result;
    {
      int choice_budget = num_left;
      const char *choice_prompt = prompt ? prompt : buf;

      int i, highest = -1, num_available_needed = 0;
      color_test_t needed = 0;
      int charging[COLOR_ARTIFACT + 1] = {0};

      if (needed_mana_colors > 0)
      {
        for (i = COLOR_COLORLESS; i <= COLOR_ARTIFACT; ++i)
        {
          if ((*charge_mana_addr_of_pay_mana_xbugrw)[i] == -1)
          {
            charging[i] = 1000;
          }
          else
          {
            charging[i] = ((*charge_mana_addr_of_pay_mana_xbugrw)[i] - cols[i] +
                           (charge_mana_addr_of_pre_mana_pool
                                ? (*charge_mana_addr_of_pre_mana_pool)[i] - mana_pool[player][i]
                                : 0));
          }
        }

        for (i = COLOR_COLORLESS; i <= COLOR_ARTIFACT; ++i)
        {
          if (charging[i] < 0)
          {
            if (charging[COLOR_ARTIFACT] > 0 && i != COLOR_ARTIFACT)
            {
              int amt_to_apply = MIN(-charging[i], charging[COLOR_ARTIFACT]);
              charging[COLOR_ARTIFACT] -= amt_to_apply;
              charging[i] += amt_to_apply;
              if (charging[i] >= 0)
                continue;
            }
            if (charging[COLOR_COLORLESS] > 0 && i != COLOR_COLORLESS)
            {
              int amt_to_apply = MIN(-charging[i], charging[COLOR_COLORLESS]);
              charging[COLOR_COLORLESS] -= amt_to_apply;
              charging[i] += amt_to_apply;
            }
          }
          else if (charging[i] > 0)
          {
            needed |= 1 << i;

            if (i != COLOR_COLORLESS && i != COLOR_ARTIFACT && (available & (1 << i)))
            {
              ++num_available_needed;

              choice_budget -= charging[i];

              if (highest == -1 || charging[i] > charging[highest])
              {
                highest = i;
              }
            }
          }
        }

        if (choice_budget >= 0)
        {
          if (highest >= 0)
          {
            {
              result = highest;
              goto mana_choice_done;
            }
          }
          else if (always_prompt_for_color)
          {
            highest = choose_any_color_from_available(available);
            if ((charging[COLOR_COLORLESS] > 0 || charging[COLOR_ARTIFACT] > 0) &&
                !always_prompt_for_color)
            {
              {
                result = highest;
                goto mana_choice_done;
              }
            }
            else
            {
              {
                result = choose_a_color_exe(player, choice_prompt, 1, highest, available);
                goto mana_choice_done;
              }
            }
          }
        }
      }

      if (num_available_needed == 1)
      {
        {
          result = highest;
          goto mana_choice_done;
        }
      }

      if (highest == -1 && !always_prompt_for_color)
      {
        highest = choose_any_color_from_available(available);

        if (needed_mana_colors > 0 &&
            (charging[COLOR_COLORLESS] > 0 || charging[COLOR_ARTIFACT] > 0))
        {
          {
            result = highest;
            goto mana_choice_done;
          }
        }
      }

      {
        result = choose_a_color_exe(player, choice_prompt, 1, highest, available);
        goto mana_choice_done;
      }
    }
  mana_choice_done:
    if (cancel == 1 || result == -1)
    {
      cancel = 1;
      return;
    }
    ++cols[result];
    prompt = NULL;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266ad70; original functions/produce_mana.c:679. */
int produce_mana_any_combination_of_colors(int player, color_test_t available, int num_mana,
                                                const char *prompt)
{
  chosen_colors = 0;
  cancel = 0;
  int cols[COLOR_ARTIFACT + 1] = {0};
  choose_combination_of_colors_of_mana_to_produce_into_arr(cols, player, available, num_mana,
                                                                prompt);
  if (cancel == 1)
  {
    return 0;
  }
  else
  {
    int i;
    for (i = COLOR_COLORLESS; i <= COLOR_ARTIFACT; ++i)
    {
      if (cols[i] > 0)
      {
        produce_mana(player, i, cols[i]);
        chosen_colors |= 1 << i;
      }
    }
    return 1;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b160; original functions/produce_mana.c:972. */
static int produce_mana_all_one_color_impl(int player, color_test_t available, int num_mana)
{
  int choice = choose_mana_all_one_color(player, available, num_mana, "What kind of mana?",
                                              COLOR_TEST_COLORLESS);
  if (cancel == 1 || choice == -1)
  {
    cancel = 1;
    return 0;
  }
  produce_mana(player, choice, num_mana);
  chosen_colors |= 1 << choice;
  return 1;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b520; original functions/produce_mana.c:972. */
int produce_mana_all_one_color(int player, color_test_t available, int num_mana)
{
  chosen_colors = 0;
  if (num_mana <= 0)
    return 1;
  return produce_mana_all_one_color_impl(player, available, num_mana);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b550; original functions/produce_mana.c:989. */
void produce_mana_of_any_type_tapped_for(int player, int card, int amount)
{
  chosen_colors = 0;
  switch (tapped_for_mana_color)
  {
  case 0x100:
  {
    color_test_t colors = 0;
    color_t col, first_found = 0x100;
    int num_found = 0;
    for (col = COLOR_COLORLESS; col < COLOR_ARTIFACT; ++col)
    {
      if (tapped_for_mana[col] > 0)
      {
        ++num_found;
        colors |= (1 << col);
        if (first_found == 0x100)
          first_found = col;
      }
    }

    if (tapped_for_mana[COLOR_ARTIFACT] > 0 && !(colors & COLOR_TEST_COLORLESS))
    {
      ++num_found;
      colors |= COLOR_TEST_COLORLESS;
      if (first_found == 0x100)
        first_found = COLOR_COLORLESS;
    }

    if (num_found == 0)
    {
      break;
    }
    else if (num_found == 1)
    {
      produce_mana(affected_card_controller, first_found, amount);
      chosen_colors |= 1 << first_found;
    }
    else
    {
      char prompt[300];
      sprintf(prompt, "%s (%s): What kind of mana?",
              cards_ptr[cards_at_7c7000[get_card_instance(player, card)->internal_card_id]->id]
                  ->full_name,
              cards_ptr[cards_at_7c7000[get_card_instance(affected_card_controller, affected_card)
                                            ->internal_card_id]
                            ->id]
                  ->full_name);

      produce_mana_any_combination_of_colors(player, colors, amount, prompt);
      if (cancel == 1)
      {
        produce_mana(affected_card_controller, first_found, amount);
        chosen_colors |= 1 << first_found;
      }
    }
    break;
  }

  case COLOR_ARTIFACT:
    produce_mana(affected_card_controller, COLOR_COLORLESS, amount);
    chosen_colors = 1 << COLOR_COLORLESS;
    break;

  case COLOR_COLORLESS:
  case COLOR_BLACK:
  case COLOR_BLUE:
  case COLOR_GREEN:
  case COLOR_RED:
  case COLOR_WHITE:
    produce_mana(affected_card_controller, tapped_for_mana_color, amount);
    chosen_colors = 1 << tapped_for_mana_color;
    break;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b770; original functions/produce_mana.c:1056. */
void produce_mana_when_subtype_is_tapped(int allowed_player, event_t event, int type,
                                              int subtype, color_t color, int amount)
{
  if (event == EVENT_TAP_CARD && tapped_for_mana_color >= 0 &&
      (allowed_player == 2 || allowed_player == affected_card_controller) &&
      ((type & TYPE_PERMANENT) || is_what(affected_card_controller, affected_card, type)) &&
      (subtype <= 0 || has_subtype(affected_card_controller, affected_card, subtype)))
  {
    produce_mana(affected_card_controller, color, amount);
  }

  if (event == EVENT_COUNT_MANA &&
      (allowed_player == 2 || allowed_player == affected_card_controller) &&
      !is_tapped(affected_card_controller, affected_card) &&
      !is_animated_and_sick(affected_card_controller, affected_card) &&
      can_produce_mana(affected_card_controller, affected_card) &&
      ((type & TYPE_PERMANENT) || is_what(affected_card_controller, affected_card, type)) &&
      (subtype <= 0 || has_subtype(affected_card_controller, affected_card, subtype)))
  {
    declare_mana_available(affected_card_controller, color, amount);
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b1f0; original functions/produce_mana.c:1080. */
static int wga_backend(int player, int card, event_t event, int subtype,
                     produce_mana_t what_to_produce, int colors, int amount)
{
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.preferred_controller = player;
  if (subtype > 0)
  {
    td.required_subtype = subtype;
  }

  card_instance_t *instance = get_card_instance(player, card);

  switch (event)
  {
  case EVENT_CAN_CAST:
    return can_target(&td);

  case EVENT_CAST_SPELL:
    if (affect_me(player, card))
    {
      char buffer[100];
      scnprintf(buffer, 100, "Select target %s.",
                subtype > 0 ? get_hacked_land_text(player, card, "%s", subtype) : "land");
      if (new_pick_target(&td, buffer, 0, 1 | GS_LITERAL_PROMPT))
      {
        if (player == AI && instance->targets[0].player == player)
        {
          ai_modifier += 30;
        }
      }
      else
      {
        cancel = 1;
      }
    }
    break;

  case EVENT_RESOLVE_SPELL:
    if (valid_target(&td))
    {
      instance->damage_target_player = instance->targets[0].player;
      instance->damage_target_card = instance->targets[0].card;
    }
    else
    {
      kill_card(player, card, KILL_BURY);
      spell_fizzled = 1;
    }
    instance->number_of_targets = 0;
    break;

  case EVENT_TAP_CARD:
    if (instance->damage_target_card != -1 &&
        affect_me(instance->damage_target_player, instance->damage_target_card) &&
        tapped_for_mana_color >= 0 && amount > 0)
    {
      if (what_to_produce == PM_ALL_ONE)
      {
        chosen_colors = 0;
        produce_mana_all_one_color_impl(instance->damage_target_player, colors, amount);
      }
      else
        produce_mana_any_combination_of_colors(instance->damage_target_player, colors, amount,
                                                    NULL);
    }
    break;

  case EVENT_COUNT_MANA:
    if (instance->damage_target_card != -1 &&
        affect_me(instance->damage_target_player, instance->damage_target_card) &&
        !is_tapped(instance->damage_target_player, instance->damage_target_card) &&
        !is_animated_and_sick(instance->damage_target_player, instance->damage_target_card) &&
        can_produce_mana(instance->damage_target_player, instance->damage_target_card) &&
        amount > 0)
    {
      if (what_to_produce == PM_ALL_ONE)
        declare_mana_available_maybe_hex(instance->damage_target_player, colors, amount);
      else
        declare_mana_available_any_combination_of_colors_impl(instance->damage_target_player,
                                                                   colors, amount);
    }
    break;

  default:
    break;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b930; original functions/produce_mana.c:1151. */
int wild_growth_aura(int player, int card, event_t event, int subtype, color_t color,
                          int amount)
{
  return wga_backend(player, card, event, subtype, PM_ALL_ONE, 1 << color, amount);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b970; original functions/produce_mana.c:1156. */
int wild_growth_aura_all_one_color(int player, int card, event_t event, int subtype,
                                        color_test_t available, int num_mana)
{
  return wga_backend(player, card, event, subtype, PM_ALL_ONE, available, num_mana);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b9a0; original functions/produce_mana.c:1161. */
int wild_growth_aura_any_combination_of_colors(int player, int card, event_t event,
                                                    int subtype, color_test_t available,
                                                    int num_mana)
{
  return wga_backend(player, card, event, subtype, PM_ANY_COMBO, available, num_mana);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266f1a0; original functions/produce_mana.c:1165. */
int enchanted_land_has_T_add_n_mana_of_any_one_color(int player, int card, event_t event,
                                                          int amt)
{
  if (in_play(player, card) && get_card_instance(player, card)->damage_target_player >= 0)
  {
    card_instance_t *instance = get_card_instance(player, card);
    int p = instance->damage_target_player;
    int c = instance->damage_target_card;
    if (event == EVENT_CAN_ACTIVATE)
      return mana_producer(p, c, event);
    if (event == EVENT_ACTIVATE)
    {
      produce_mana_tapped_all_one_color_with_default_impl(p, c, COLOR_TEST_ANY_COLORED, amt,
                                                          COLOR_TEST_COLORLESS, 1, 1);
      if (spell_fizzled != 1)
        dispatch_event(p, c, EVENT_TAP_CARD);
    }
    if (event == EVENT_COUNT_MANA && affect_me(player, card) &&
        mana_producer(p, c, EVENT_CAN_ACTIVATE))
    {
      if (multi_mana_transmutation[player][0] &&
          (amt > 1 || (amt == 1 && mana_doublers(player, TYPE_LAND) > 1)))
      {
        int quantity = multi_mana_transmutation[p][1];
        color_test_t colors;
        if (check_special_flags2(p, c, SF2_CONTAMINATION))
          colors = mana_color_override[p][multi_mana_transmutation[p][0]];
        else
          colors = multi_mana_transmutation[p][0];
        declare_mana_available(p, single_color_test_bit_to_color(colors), quantity);
      }
      else if (check_special_flags2(p, c, SF2_CONTAMINATION))
      {
        color_test_t colors = 0;
        for (int i = 0; i < 7; ++i)
        {
          if (mana_quantity_override[p][i] >= 0)
          {
            declare_mana_available(p, mana_color_override[p][i], mana_quantity_override[p][i]);
            return 0;
          }
          colors |= color_to_color_test(mana_color_override[player][i]);
        }
        declare_mana_available_maybe_hex(player, colors, amt);
      }
      else
        declare_mana_available_hex(p, COLOR_TEST_ANY_COLORED, amt);
    }
  }

  if (!((event == EVENT_CAST_SPELL && affect_me(player, card)) || event == EVENT_CAN_CAST ||
        event == EVENT_RESOLVE_SPELL || event == EVENT_CHANGE_TARGET ||
        event == EVENT_CAN_CHANGE_TARGET || event == EVENT_CAN_MOVE_AURA ||
        event == EVENT_MOVE_AURA || event == EVENT_RESOLVE_MOVING_AURA || event == EVENT_POWER ||
        event == EVENT_TOUGHNESS || event == EVENT_ABILITIES || event == EVENT_CHANGE_TYPE ||
        event == EVENT_SET_COLOR || event == EVENT_GET_SELECTED_CARD ||
        event == EVENT_STATIC_EFFECTS))
    return 0;
  target_definition_t td;
  default_target_definition(player, card, &td, TYPE_LAND);
  td.preferred_controller = player;
  td.allowed_controller = player;
  card_instance_t *instance = get_card_instance(player, card);
  if (event == EVENT_CAN_CAST)
    return targeted_aura(player, card, event, &td, NULL);
  if (event == EVENT_CAST_SPELL && affect_me(player, card))
  {
    instance->number_of_targets = 0;
    ai_modifier += 100;
    new_pick_target(&td, "Select target land you control.", 0, 1 | GS_LITERAL_PROMPT);
  }
  if (event == EVENT_RESOLVE_SPELL)
    return targeted_aura(player, card, event, &td, NULL);
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266d050; original functions/produce_mana.c:1239. */
int mana_producing_creature(int player, int card, event_t event, int reluctance_to_fight,
                                 color_t color, int amount)
{
  if (event == EVENT_CAN_ACTIVATE)
  {
    return !is_tapped(player, card) && !is_sick(player, card) &&
           can_produce_mana(player, card) && (player != AI || amount > 0) &&
           !check_special_flags2(player, card, SF2_KATABATIC_WINDS);
  }
  else if (event == EVENT_ACTIVATE)
  {
    produce_mana_tapped_impl(player, card, color, amount, 1, 1);
  }
  else if (event == EVENT_COUNT_MANA)
  {
    if (affect_me(player, card) && mana_producing_creature(player, card, EVENT_CAN_ACTIVATE,
                                                                reluctance_to_fight, color, amount))
    {
      declare_mana_available(player, color, amount);
    }
  }
  else if (reluctance_to_fight && (event == EVENT_ATTACK_RATING || event == EVENT_BLOCK_RATING) &&
           affect_me(player, card))
  {
    if (color == COLOR_COLORLESS || color == COLOR_ARTIFACT)
    {
      color = COLOR_ANY;
    }
    if (event == EVENT_ATTACK_RATING)
    {
      ai_defensive_modifier += reluctance_to_fight / (landsofcolor_controlled[player][color] + 2);
    }
    else
    {
      ai_defensive_modifier -=
          4 * reluctance_to_fight / (landsofcolor_controlled[player][color] + 2);
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266e5e0; original functions/produce_mana.c:1262. */
int mana_producing_creature_multi(int player, int card, event_t event, int reluctance_to_fight,
                                       int colorless, int black, int blue, int green, int red,
                                       int white)
{
  if (event == EVENT_CAN_ACTIVATE)
  {
    return !is_tapped(player, card) && !is_sick(player, card) &&
           can_produce_mana(player, card) &&
           (player != AI || colorless > 0 || black > 0 || blue > 0 || green > 0 || red > 0 ||
            white > 0) &&
           !check_special_flags2(player, card, SF2_KATABATIC_WINDS);
  }
  else if (event == EVENT_ACTIVATE)
  {
    produce_mana_tapped_multi_impl(player, card, colorless, black, blue, green, red, white, 1);
  }
  else if (event == EVENT_COUNT_MANA)
  {
    if (affect_me(player, card) &&
        mana_producing_creature_multi(player, card, EVENT_CAN_ACTIVATE, reluctance_to_fight,
                                           colorless, black, blue, green, red, white))
    {
      declare_mana_available(player, COLOR_COLORLESS, colorless);
      declare_mana_available(player, COLOR_BLACK, black);
      declare_mana_available(player, COLOR_BLUE, blue);
      declare_mana_available(player, COLOR_GREEN, green);
      declare_mana_available(player, COLOR_RED, red);
      declare_mana_available(player, COLOR_WHITE, white);
    }
  }
  else if (reluctance_to_fight && (event == EVENT_ATTACK_RATING || event == EVENT_BLOCK_RATING) &&
           affect_me(player, card))
  {
    if (event == EVENT_ATTACK_RATING)
    {
      ai_defensive_modifier +=
          reluctance_to_fight / (landsofcolor_controlled[player][COLOR_ANY] + 2);
    }
    else
    {
      ai_defensive_modifier -=
          4 * reluctance_to_fight / (landsofcolor_controlled[player][COLOR_ANY] + 2);
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266c5c0; original functions/produce_mana.c:1288. */
int mana_producing_creature_all_one_color(int player, int card, event_t event,
                                               int reluctance_to_fight, color_test_t available,
                                               int num_mana)
{
  int result;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = is_tapped(player, card);
    if ((((result == 0) && (result = is_sick(player, card), result == 0)) &&
         (result = can_produce_mana(player, card), result != 0)) &&
        ((player != 1 || (0 < num_mana))))
    {
      result = check_special_flags2(player, card, 0x10);
      return (unsigned int)(result == 0);
    }
  }
  else
  {
    if (event == EVENT_ACTIVATE)
    {
      produce_mana_tapped_all_one_color_with_default_impl(player, card, available, num_mana,
                                                          COLOR_TEST_COLORLESS, 1, 1);
      return 0;
    }
    if (event == EVENT_COUNT_MANA)
    {
      if (((card == affected_card) && (player == affected_card_controller)) &&
          (result = mana_producing_creature_all_one_color(
               player, card, EVENT_CAN_ACTIVATE, reluctance_to_fight, available, num_mana),
           result != 0))
      {
        declare_mana_available_maybe_hex(player, available, num_mana);
        return 0;
      }
    }
    else if ((((reluctance_to_fight != 0) &&
               ((unsigned int)event - (unsigned int)EVENT_ATTACK_RATING < 2U)) &&
              (card == affected_card)) &&
             (player == affected_card_controller))
    {
      result = landsofcolor_controlled[player][7] + 2;
      if (event == EVENT_ATTACK_RATING)
      {
        ai_defensive_modifier = reluctance_to_fight / result + ai_defensive_modifier;
        return 0;
      }
      ai_defensive_modifier = ai_defensive_modifier - (reluctance_to_fight << 2) / result;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266c7a0; original functions/produce_mana.c:1308. */
int artifact_mana_all_one_color(int player, int card, event_t event, int amount, int sac)
{
  int result;
  card_instance_t *instance;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = is_tapped(player, card);
    if (((result == 0) && (result = is_animated_and_sick(player, card), result == 0)) &&
        (result = can_produce_mana(player, card), result != 0))
    {
      if (sac == 0)
      {
        return 1;
      }
      result = can_sacrifice_as_cost(player, 1, 0x40, 0, 0, 0, 0, 0, 0, 0, -1, 0);
      return (unsigned int)(result != 0);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    if (sac == 0)
    {
      ai_modifier = ai_modifier + -0xc;
      instance = get_card_instance(player, card);
      produce_mana_tapped_all_one_color_with_default_impl(
          player, card, (unsigned int)instance->mana_color, amount, COLOR_TEST_COLORLESS, 1, 1);
    }
    else
    {
      ai_modifier = ai_modifier + -0x24;
      instance = get_card_instance(player, card);
      produce_mana_tapped_all_one_color_with_default_impl(
          player, card, (unsigned int)instance->mana_color, amount, COLOR_TEST_COLORLESS, 1, 1);
      if (spell_fizzled != 1)
      {
        kill_card(player, card, KILL_SACRIFICE);
        return 0;
      }
    }
  }
  else if (((event == EVENT_COUNT_MANA) && (card == affected_card)) &&
           ((player == affected_card_controller &&
             (result =
                  artifact_mana_all_one_color(player, card, EVENT_CAN_ACTIVATE, amount, sac),
              result != 0))))
  {
    instance = get_card_instance(player, card);
    declare_mana_available_maybe_hex(player, (unsigned int)instance->mana_color, amount);
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x26712f0; original functions/produce_mana.c:1327. */
int artifact_mana_all_different_colors(int player, int card, event_t event, int amount,
                                            int sac)
{
  int result;
  card_instance_t *instance;
  if (event == EVENT_CAN_ACTIVATE)
  {
    result = is_tapped(player, card);
    if (((result == 0) && (result = is_animated_and_sick(player, card), result == 0)) &&
        (result = can_produce_mana(player, card), result != 0))
    {
      if (sac == 0)
      {
        return 1;
      }
      result = can_sacrifice_as_cost(player, 1, 0x40, 0, 0, 0, 0, 0, 0, 0, -1, 0);
      return (unsigned int)(result != 0);
    }
  }
  else if (event == EVENT_ACTIVATE)
  {
    if (sac == 0)
    {
      ai_modifier = ai_modifier + -0xc;
      instance = get_card_instance(player, card);
      produce_mana_tapped_all_different_colors_impl(player, card, (unsigned int)instance->mana_color,
          amount, NULL, 1);
      return 0;
    }
    ai_modifier = ai_modifier + -0x24;
    instance = get_card_instance(player, card);
    produce_mana_tapped_all_different_colors_impl(player, card, (unsigned int)instance->mana_color,
        amount, NULL, 1);
    if (spell_fizzled != 1)
    {
      kill_card(player, card, KILL_SACRIFICE);
      return 0;
    }
  }
  else if (((event == EVENT_COUNT_MANA) && (card == affected_card)) &&
           ((player == affected_card_controller &&
             (result =
                  artifact_mana_all_one_color(player, card, EVENT_CAN_ACTIVATE, amount, sac),
              result != 0))))
  {
    instance = get_card_instance(player, card);
    declare_mana_available_maybe_hex(player, (unsigned int)instance->mana_color, amount);
    return 0;
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266d9b0; original functions/produce_mana.c:1346. */
int two_mana_land(int player, int card, event_t event, color_t color1, color_t color2)
{
  switch (event)
  {
  case EVENT_RESOLVE_SPELL:
    play_land_sound_effect(player, card);
    return 0;
  case EVENT_CAN_ACTIVATE:
    return !is_tapped(player, card) && !is_animated_and_sick(player, card) &&
           can_produce_mana(player, card);
  case EVENT_ACTIVATE:
    if (check_special_flags2(player, card, SF2_CONTAMINATION))
    {
      declare_mana_available(player, mana_color_override[player][color2], -1);
      declare_mana_available(player, mana_color_override[player][color1], -1);
    }
    else
    {
      declare_mana_available(player, color2, -1);
      declare_mana_available(player, color1, -1);
    }
    produce_mana_tapped2_impl(player, card, color1, 1, color2, 1, 1);
    return 0;
  case EVENT_COUNT_MANA:
    if (!affect_me(player, card) ||
        !two_mana_land(player, card, EVENT_CAN_ACTIVATE, color1, color2))
      return 0;
    if (mana_colors_added[player])
    {
      declare_mana_available_maybe_hex(
          player, color_to_color_test(color1) | mana_colors_added[player], 1);
      declare_mana_available_maybe_hex(
          player, color_to_color_test(color2) | mana_colors_added[player], 1);
    }
    else if (multi_mana_transmutation[player][0])
      declare_mana_available(
          player, single_color_test_bit_to_color(multi_mana_transmutation[player][0]),
          multi_mana_transmutation[player][1]);
    else if (check_special_flags2(player, card, SF2_CONTAMINATION))
    {
      declare_mana_available(player, mana_color_override[player][color2], 1);
      declare_mana_available(player, mana_color_override[player][color1], 1);
    }
    else
    {
      declare_mana_available(player, color2, 1);
      declare_mana_available(player, color1, 1);
    }
    return 0;
  default:
    return 0;
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266dca0; original functions/produce_mana.c:1401. */
int sac_land(int player, int card, event_t event, int base_color, int color1, int color2)
{
  if (event == EVENT_RESOLVE_SPELL)
    get_card_instance(player, card)->info_slot = (1 << base_color) | (1 << color1) | (1 << color2);
  else if (event == EVENT_ACTIVATE)
  {
    int choice = 0;
    if (player != AI && ai_is_speculating != 1 && !ldoubleclicked)
      choice =
          do_dialog(player, player, card, -1, -1, " One Mana Ability\n Sac Ability\n Cancel", 0);
    if (choice == 1)
    {
      produce_mana_tapped2_impl(player, card, color1, 1, color2, 1, 0);
      kill_card(player, card, KILL_SACRIFICE);
    }
    else if (choice == 2)
      spell_fizzled = 1;
    else
    {
      color_test_t colors = color_to_color_test(base_color) | mana_colors_added[player];
      if (num_bits_set(colors) == 1)
        mana_producer_fixed(player, card, event, base_color);
      else
        produce_mana_tapped_all_one_color_with_default_impl(player, card, colors, 1,
                                                            COLOR_TEST_COLORLESS, 0, 1);
    }
    return 0;
  }
  return mana_producer_fixed(player, card, event, base_color);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266de60; original functions/produce_mana.c:1430. */
int sac_land_tapped(int player, int card, event_t event, int base_color, int color1,
                         int color2)
{
  if (!is_humiliated(player, card, AL_TRIG))
    comes_into_play_tapped(player, card, event);
  return sac_land(player, card, event, base_color, color1, color2);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266d250; original functions/produce_mana.c:1440. */
int permanents_you_control_can_tap_for_mana(int player, int card, event_t event, type_t type,
                                                 int32_t subtype, color_t color, int amount)
{
  if (event == EVENT_CAN_ACTIVATE)
  {
    return check_for_untapped_nonsick_subtype(player, type, subtype);
  }

  if (event == EVENT_ACTIVATE)
  {

    target_definition_t td;
    default_target_definition(player, card, &td, type);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_SUMMONSICK_NOTAP;
    td.illegal_abilities = 0;
    if (subtype > 0)
    {
      td.required_subtype = subtype;
    }

    if (pick_target(&td, type == TYPE_CREATURE ? "TARGET_CREATURE"
                              : type == TYPE_LAND   ? "TARGET_LAND"
                                                    : "TARGET_PERMANENT"))
    {
      card_instance_t *instance = get_card_instance(player, card);
      instance->number_of_targets = 1;
      produce_mana_tapped_impl(instance->targets[0].player, instance->targets[0].card, color,
                               amount, 1, 1);
      if (!(instance->targets[0].player == player && instance->targets[0].card == card))
      {
        dispatch_event(instance->targets[0].player, instance->targets[0].card, EVENT_TAP_CARD);
      }
    }
    else
    {
      cancel = 1;
    }
  }

  if (event == EVENT_COUNT_MANA && affect_me(player, card))
  {
    int count = amount * count_untapped_nonsick_subtype(player, type, subtype);
    if (count > 0)
    {
      declare_mana_available(player, color, count);
    }
  }

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266ca00; original functions/produce_mana.c:1483. */
int permanents_you_control_can_tap_for_mana_all_one_color(int player, int card, event_t event,
                                                               type_t type, int32_t subtype,
                                                               color_test_t available, int num_mana)
{
  if ((available & (COLOR_TEST_ANY | COLOR_TEST_ARTIFACT)) == 0)
  {
    return 0;
  }
  if (event == EVENT_CAN_ACTIVATE && !is_humiliated(player, card, AL_STATIC))
  {
    return check_for_untapped_nonsick_subtype(player, type, subtype);
  }

  if (event == EVENT_ACTIVATE)
  {

    target_definition_t td;
    default_target_definition(player, card, &td, type);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED | TARGET_STATE_SUMMONSICK_NOTAP;
    td.illegal_abilities = 0;
    if (subtype > 0)
    {
      td.required_subtype = subtype;
    }

    if (pick_target(&td, type == TYPE_CREATURE ? "TARGET_CREATURE"
                              : type == TYPE_LAND   ? "TARGET_LAND"
                                                    : "TARGET_PERMANENT"))
    {
      card_instance_t *instance = get_card_instance(player, card);
      instance->number_of_targets = 1;
      produce_mana_tapped_all_one_color_with_default_impl(instance->targets[0].player,
                                                          instance->targets[0].card, available,
                                                          num_mana, COLOR_TEST_COLORLESS, 1, 1);
      if (!(instance->targets[0].player == player && instance->targets[0].card == card))
      {
        dispatch_event(instance->targets[0].player, instance->targets[0].card, EVENT_TAP_CARD);
      }
    }
    else
    {
      cancel = 1;
    }
  }

  if (event == EVENT_COUNT_MANA && affect_me(player, card))
  {
    int count = count_untapped_nonsick_subtype(player, type, subtype) * num_mana;
    if (count > 0)
      declare_mana_available_any_combination_of_colors_impl(player, available, count);
  }

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266b9d0; original functions/produce_mana.c:1532. */
int tap_a_permanent_you_control_for_mana(int player, int card, event_t event, type_t type,
                                              int32_t subtype, color_t color, int amount)
{
  if (event == EVENT_CAN_ACTIVATE && can_use_activated_abilities(player, card))
  {
    return count_untapped_subtype(player, type, subtype) > 0;
  }

  if (event == EVENT_ACTIVATE)
  {

    target_definition_t td;
    default_target_definition(player, card, &td, type);
    td.allowed_controller = player;
    td.preferred_controller = player;
    td.illegal_state = TARGET_STATE_TAPPED;
    td.illegal_abilities = 0;
    if (subtype > 0)
    {
      td.required_subtype = subtype;
    }

    if (pick_target(&td, type == TYPE_CREATURE   ? "TARGET_CREATURE"
                              : type == TYPE_LAND     ? "TARGET_LAND"
                              : type == TYPE_ARTIFACT ? "TARGET_ARTIFACT"
                                                      : "TARGET_PERMANENT"))
    {
      card_instance_t *instance = get_card_instance(player, card);
      instance->number_of_targets = 1;
      produce_mana(player, color, amount);
      tapped_for_mana_color = -2;
      if (instance->targets[0].player == player && instance->targets[0].card == card)
      {
        instance->state |= STATE_TAPPED;
      }
      else
      {
        get_card_instance(instance->targets[0].player, instance->targets[0].card)->state |=
            STATE_TAPPED;
        dispatch_event(instance->targets[0].player, instance->targets[0].card, EVENT_TAP_CARD);
      }
    }
    else
    {
      cancel = 1;
    }
  }

  if (event == EVENT_COUNT_MANA && affect_me(player, card))
  {
    int count = amount * count_untapped_subtype(player, type, subtype);
    if (count > 0)
    {
      declare_mana_available(player, color, count);
    }
  }

  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266f670; original functions/produce_mana.c:1581. */
int get_colors_of_mana_land_could_produce_ignoring_costs(int player, int card)
{
  card_data_t *card_data = get_card_data(player, card);
  card_instance_t *instance;
  int count;
  unsigned int colors;
  if (!(card_data->extra_ability & EA_MANA_SOURCE))
    return mana_colors_added[player];
  int csvid = card_data->id;
  if (multi_mana_transmutation[player][0])
  {
    count = mana_doublers(player, TYPE_LAND);
    if (count > 1 && (card_data->type & TYPE_LAND) &&
        ((card_data->extra_ability & EA_MANA_SOURCE) || mana_colors_added[player]))
      return multi_mana_transmutation[player][0];
    colors = multi_mana_transmutation[player][0];
    if (colors)
    {
      switch (csvid)
      {
      case CARD_ID_CABAL_COFFERS:
        count = count_subtype(player, TYPE_LAND, SUBTYPE_SWAMP);
        return 1 < count ? multi_mana_transmutation[player][0] : -1;

      case CARD_ID_TOLARIAN_ACADEMY:
        count = count_subtype(player, TYPE_ARTIFACT, -1);
        return 1 < count ? multi_mana_transmutation[player][0] : -1;

      case CARD_ID_GAEAS_CRADLE:
        count = count_subtype(player, TYPE_CREATURE, -1);
        return 1 < count ? multi_mana_transmutation[player][0] : -1;

      case CARD_ID_ORZHOV_BASILICA:
      case CARD_ID_GOLGARI_ROT_FARM:
      case CARD_ID_DIMIR_AQUEDUCT:
      case CARD_ID_GRUUL_TURF:
      case CARD_ID_IZZET_BOILERWORKS:
      case CARD_ID_AZORIUS_CHANCERY:
      case CARD_ID_RAKDOS_CARNARIUM:
      case CARD_ID_SIMIC_GROWTH_CHAMBER:
      case CARD_ID_BALDUVIAN_TRADING_POST:
      case CARD_ID_SCHOOL_OF_THE_UNSEEN:
      case CARD_ID_SOLDEVI_EXCAVATIONS:
      case CARD_ID_TEFERIS_ISLE:
      case CARD_ID_CORAL_ATOLL:
      case CARD_ID_DORMANT_VOLCANO:
      case CARD_ID_EVERGLADES:
      case CARD_ID_JUNGLE_BASIN:
      case CARD_ID_KAROO:
      case CARD_ID_LOTUS_VALE:
      case CARD_ID_PHYREXIAN_TOWER:
      case CARD_ID_HICKORY_WOODLOT:
      case CARD_ID_PEAT_BOG:
      case CARD_ID_REMOTE_FARM:
      case CARD_ID_SANDSTONE_NEEDLE:
      case CARD_ID_SAPRAZZAN_SKERRY:
      case CARD_ID_SHIVAN_REEF:
      case CARD_ID_CRYSTAL_QUARRY:
      case CARD_ID_DARKWATER_CATACOMBS:
      case CARD_ID_MOSSFIRE_VALLEY:
      case CARD_ID_SHADOWBLOOD_RIDGE:
      case CARD_ID_SKYCLOUD_EXPANSE:
      case CARD_ID_SUNGRASS_PRAIRIE:
      case CARD_ID_GRAVEN_CAIRNS:
      case CARD_ID_FIRE_LIT_THICKET:
      case CARD_ID_MYSTIC_GATE:
      case CARD_ID_SUNKEN_RUINS:
      case CARD_ID_WOODED_BASTION:
      case CARD_ID_CASCADE_BLUFFS:
      case CARD_ID_FETID_HEATH:
      case CARD_ID_FLOODED_GROVE:
      case CARD_ID_RUGGED_PRAIRIE:
      case CARD_ID_TWILIGHT_MIRE:
      case CARD_ID_CASCADING_CATARACTS:
      case CARD_ID_LOST_VALE:
      case CARD_ID_GOLD_FORGE_GARRISON:
        return colors;

      case CARD_ID_BOTTOMLESS_VAULT:
      case CARD_ID_EBON_STRONGHOLD:
      case CARD_ID_SULFUR_VENT:
        return colors | COLOR_TEST_BLACK;

      case CARD_ID_DWARVEN_HOLD:
      case CARD_ID_DWARVEN_RUINS:
      case CARD_ID_GEOTHERMAL_CREVICE:
        return colors | COLOR_TEST_RED;

      case CARD_ID_HAVENWOOD_BATTLEGROUND:
      case CARD_ID_HOLLOW_TREES:
      case CARD_ID_TINDER_FARM:
        return colors | COLOR_TEST_GREEN;

      case CARD_ID_ICATIAN_STORE:
      case CARD_ID_RUINS_OF_TROKAIR:
      case CARD_ID_IRRIGATION_DITCH:
        return colors | COLOR_TEST_WHITE;

      case CARD_ID_SAND_SILOS:
      case CARD_ID_SVYELUNITE_TEMPLE:
      case CARD_ID_ANCIENT_SPRING:
        return colors | COLOR_TEST_BLUE;

      case CARD_ID_SERRAS_SANCTUM:
        count = count_subtype(player, TYPE_ENCHANTMENT, -1);
        return 1 < count ? multi_mana_transmutation[player][0] : -1;

      case CARD_ID_ITLIMOC_CRADLE_OF_THE_SUN:
        count = count_subtype(player, TYPE_CREATURE, -1);
        return 1 < count ? multi_mana_transmutation[player][0] | COLOR_TEST_COLORLESS | COLOR_TEST_BLACK : -1;

      case CARD_ID_SANCTUM_OF_THE_SUN:
        return 1 < life[player] ? colors : -1;

      case CARD_ID_VAULT_OF_CATLACAN:
        count = count_subtype(player, TYPE_ARTIFACT, -1);
        if (1 < count)
        {
          colors = multi_mana_transmutation[player][0];
          return colors | COLOR_TEST_BLACK;
        }
        else
        {
          return -1;
        }

      case CARD_ID_CABAL_STRONGHOLD:
        count = count_basic_lands(player, SUBTYPE_SWAMP);
        return 1 < count ? multi_mana_transmutation[player][0] : -1;

      default:
        break;
      }
    }
  }
  switch (csvid)
  {
  case CARD_ID_GEM_BAZAAR:
  case CARD_ID_ANCIENT_SPRING:
  case CARD_ID_GEOTHERMAL_CREVICE:
  case CARD_ID_IRRIGATION_DITCH:
  case CARD_ID_SULFUR_VENT:
  case CARD_ID_TINDER_FARM:
  case CARD_ID_TAINTED_FIELD:
  case CARD_ID_TAINTED_ISLE:
  case CARD_ID_TAINTED_PEAK:
  case CARD_ID_TAINTED_WOOD:
  case CARD_ID_GEMSTONE_CAVERNS:
  case CARD_ID_URBORG_TOMB_OF_YAWGMOTH:
  case CARD_ID_NIMBUS_MAZE:
  case CARD_ID_RIVER_OF_TEARS:
  case CARD_ID_COMMAND_TOWER:
  case CARD_ID_NYKTHOS_SHRINE_TO_NYX:
  case CARD_ID_OPAL_PALACE:
  case CARD_ID_COMMANDERS_SPHERE:
  case CARD_ID_PATH_OF_ANCESTRY:
  case CARD_ID_ARCANE_SIGNET:
    instance = get_card_instance(player, card);
    return instance->info_slot | mana_colors_added[player];

  case CARD_ID_EXOTIC_ORCHARD:
  case CARD_ID_REFLECTING_POOL:
    instance = get_card_instance(player, card);
    return mana_colors_added[player] | ((unsigned int) instance->mana_color);

  case CARD_ID_SCHOOL_OF_THE_UNSEEN:
  case CARD_ID_MIRRODINS_CORE:
  case CARD_ID_TENDO_ICE_BRIDGE:
  case CARD_ID_SHIMMERING_GROTTO:
  case CARD_ID_VIVID_CRAG:
  case CARD_ID_VIVID_CREEK:
  case CARD_ID_VIVID_GROVE:
  case CARD_ID_VIVID_MARSH:
  case CARD_ID_VIVID_MEADOW:
  case CARD_ID_CAVERN_OF_SOULS:
  case CARD_ID_UNKNOWN_SHORES:
  case CARD_ID_CASCADING_CATARACTS:
    return COLOR_TEST_ANY_COLORED;

  case CARD_ID_SPRINGJACK_PASTURE:
    return COLOR_TEST_ANY;

  default:
    instance = get_card_instance(player, card);
    return (instance->info_slot | mana_colors_added[player]) | ((unsigned int) instance->mana_color);
  }
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266bc20; original functions/produce_mana.c:2051. */
int all_lands_are_basiclandtype(int player, int card, int event, int whose_lands,
                                     int land_color, int land_subtype)
{
  if (event == EVENT_MODIFY_MANA_PROD && in_play(player, card))
  {
    for (int p = 0; p < 2; ++p)
      if (p == whose_lands || whose_lands == ANYBODY)
      {
        mana_colors_added[p] |=
            basic_subtype_to_color_test(get_hacked_subtype(player, card, land_subtype));
        mana_colors_added[p] |=
            color_to_color_test(get_hacked_color(player, card, land_color)) & 0x7f;
      }
  }
  else if (event == EVENT_CHANGE_TYPE && in_play(player, card) &&
           (affected_card_controller == whose_lands || whose_lands == ANYBODY))
  {
    card_instance_t *instance = get_card_instance(player, card);
    unsigned int col = 1U << (color_to_color_test(get_hacked_color(player, card, land_color)) & 31);
    if ((unsigned int)instance->info_slot != col)
    {
      instance->info_slot = col;
      int subtype = get_hacked_subtype(player, card, land_subtype);
      for (int p = 0; p < 2; ++p)
        if (p == whose_lands || whose_lands == ANYBODY)
          for (int c = 0; c < active_cards_count[p]; ++c)
            if (in_play(p, c) && is_what(p, c, TYPE_LAND))
              add_a_subtype(p, c, subtype);
      col = 1U << (color_to_color_test(get_hacked_color(player, card, land_color)) & 31);
      get_card_instance(player, card)->info_slot = col;
    }
  }
  return 0;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266bf60; original functions/produce_mana.c:2110. */
void minimize_nondraining_mana(void)
{
  int p, c, nondraining;
  for (p = 0; p <= 1; ++p)
    for (c = 0; c < 7; ++c)
      if ((nondraining = (mana_doesnt_drain_from_pool[p][c] & MANADRAIN_AMT_MASK)) &&
          mana_pool[p][c] < nondraining)
        mana_doesnt_drain_from_pool[p][c] =
            mana_pool[p][c] | (mana_doesnt_drain_from_pool[p][c] & ~MANADRAIN_AMT_MASK);
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x266bfd0; original functions/produce_mana.c:2122. */
void mana_doesnt_drain_until_eot(int player, color_t color, int amt)
{
  if (amt < 0)
    return;

  int nondraining = mana_doesnt_drain_from_pool[player][color] & MANADRAIN_AMT_MASK;

  nondraining += amt;
  nondraining = MIN(nondraining, MANADRAIN_AMT_MASK);
  nondraining = MIN(nondraining, mana_pool[player][color]);

  mana_doesnt_drain_from_pool[player][color] &= ~MANADRAIN_AMT_MASK;
  mana_doesnt_drain_from_pool[player][color] |= nondraining;
}

#endif

#ifndef SHANDALAR
/* ManalinkEh 2022: 0x2671530; original functions/produce_mana.c:2157. */
void mana_burn(void)
{

  if (!(trace_mode & 2))
  {
  restart:;
    int c;
    card_instance_t *instance;
    if (mana_pool[AI][7] && current_phase > PHASE_NORMAL_COMBAT_DAMAGE &&
        (unsigned int)(active_cards_count[AI] - 1) < 149)
      for (c = 0; c < active_cards_count[AI]; ++c)
        if ((instance = in_play(AI, c)) &&
            dispatch_event_with_attacker_to_one_card(AI, c, EVENT_CAN_WASTE_MANA, 1 - AI, -1))
        {
          if (activate(AI, AI, c))
            finalize_activation(AI, c);
          goto restart;
        }
  }

  int p, c;
  for (p = 0; p <= 1; ++p)
    for (c = 0; c < 7; ++c)
      mana_doesnt_drain_from_pool[p][c] &= MANADRAIN_AMT_MASK;
  dispatch_event(0, 0, EVENT_MANA_POOL_DRAINING);

  for (p = 0; p < 2; ++p)
  {
    int amt_drained = 0, amt_left = 0;
    int clr;
    if (mana_pool[p][COLOR_ANY] > 0)
    {
      for (clr = COLOR_ARTIFACT; clr >= COLOR_COLORLESS; --clr)
        if (mana_doesnt_drain_from_pool[p][clr] & MANADRAIN_DOESNT_DRAIN)
          amt_left += mana_pool[p][clr];
        else
        {
          uint8_t nondraining;
          if ((nondraining = (mana_doesnt_drain_from_pool[p][clr] & MANADRAIN_AMT_MASK)))
          {
            if (nondraining > mana_pool[p][clr])
            {
              nondraining = mana_pool[p][clr];
              mana_doesnt_drain_from_pool[p][clr] &= ~MANADRAIN_AMT_MASK;
              mana_doesnt_drain_from_pool[p][clr] |= nondraining;
            }
            mana_pool[p][clr] -= nondraining;
          }

          if (mana_doesnt_drain_from_pool[p][clr] & MANADRAIN_BECOMES_COLORLESS)
          {
            amt_left += mana_pool[p][clr];
            mana_pool[p][COLOR_COLORLESS] += mana_pool[p][clr];
          }
          else
            amt_drained += mana_pool[p][clr];

          mana_pool[p][clr] = nondraining;
          amt_left += nondraining;
        }
    }

    mana_pool[p][COLOR_ANY] = amt_left;

    int setting_mana_burn;
    if (amt_drained > 0 && (setting_mana_burn = get_setting(SETTING_MANA_BURN)) &&
        !(setting_mana_burn == 2 && IS_AI(p)))
    {
      if (ai_is_speculating != 1)
      {
        play_sound_effect(WAV_MANABURN);
        display_mana_burn_dialog(p, amt_drained);
      }
      life[p] -= amt_drained;
    }
  }
}

#endif
