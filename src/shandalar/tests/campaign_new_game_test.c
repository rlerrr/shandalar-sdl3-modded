/* Exercise campaign setup with the actual imported table and selection code. */
#define GLOBAL_STATE_IMPL
#define GLOBALS_OTHERS_IMPL
#include "shandalar/src/shandalar_internal.h"
#include "magic/src/game_support.h"
#include "magic/src/global_other.h"
#ifdef _WIN32
#include <direct.h>
#define campaign_chdir _chdir
#else
#include <unistd.h>
#define campaign_chdir chdir
#endif
#include <stdlib.h>
#include <string.h>

int g_duel_ai_mode_state;
int g_duel_special_rules_by_color[7];
int g_last_matching_deck_card_index;
int AddCardToDeckSorted(int);
int GetRemainingAllowedCardCopies(unsigned int);
int CardIDFromType(unsigned int);
static int failures;
static int original_counts[SHANDALAR_CARD_DATA_COUNT];

void AddJournalEntry(int type, int arg) { (void)type; (void)arg; }
static void check(int condition, const char *message)
{
  if (!condition) { fprintf(stderr, "%s\n", message); ++failures; }
}

int main(int argc, char **argv)
{
  const int expected_counts[4] = {36, 40, 46, 46};
  int color, difficulty, slot, iid, count, basic_lands, high_ids, total_high_ids = 0;
  if (argc != 2 || campaign_chdir(argv[1]) != 0) return 2;
  g_card_count = SHANDALAR_CARD_DATA_ACTIVE_COUNT;
  LoadCardRaritiesAndCsvOffsets();
  for (iid = 0; iid < 5; ++iid)
    check((global_cards_data[iid].expansion & 0xc1) != 0,
          "Every basic land must be eligible for campaign starters");

  for (difficulty = 0; difficulty < 4; ++difficulty)
  {
    for (color = 1; color <= 5; ++color)
    {
      srand(17 + difficulty * 5 + color);
      g_selected_wizard_color = color;
      g_shandalar_difficulty = difficulty;
      g_skip_new_game_state_initialization = g_world_magic_bitmap = 0;
      InitializeNewGameState();
      count = basic_lands = high_ids = 0;
      memset(original_counts, 0, sizeof(original_counts));
      for (slot = 0; slot < 500; ++slot)
      {
        if (deck[slot] == -1) continue;
        ++count;
        iid = deck[slot] & ADVENTURE_DECK_CARD_ID_MASK;
        ++original_counts[iid];
        if (iid < 5) ++basic_lands;
        if (iid > 4095) ++high_ids;
        check(iid < g_card_count && (global_cards_data[iid].expansion & 0xc1) != 0,
              "Starters must contain valid campaign cards");
        check(CardIDFromType(deck[slot]) == global_cards_data[iid].id,
              "New-card flags must preserve the card's CSV identity");
      }
      check(count == expected_counts[difficulty], "Starter deck size changed");
      check(basic_lands > 0, "Starter deck must contain basic lands");
      total_high_ids += high_ids;
      RebuildDeckEntriesByCardGroup();
      for (slot = 0; slot < count; ++slot)
      {
        check(deck[slot] != -1 && (deck[slot] & 0x10000) != 0,
              "Deck rebuild must retain new-card flags");
        iid = deck[slot] & ADVENTURE_DECK_CARD_ID_MASK;
        check(original_counts[iid]-- > 0, "Deck rebuild must preserve every card's identity");
      }
    }
  }
  check(total_high_ids > 0, "Exercise imported campaign IDs above the old 12-bit limit");

  /* Mesa Pegasus has an imported ID above 4095. Its highlight/selection flags
     must not change copy limits, and market copies must remain excluded. */
  iid = FindCardIndexByCsvid(CARD_ID_MESA_PEGASUS);
  check(iid > 4095 && iid <= ADVENTURE_DECK_CARD_ID_MASK, "Expected a large Mesa Pegasus ID");
  for (slot = 0; slot < 500; ++slot) deck[slot] = -1;
  deck[0] = iid | 0x10000 | 0x8000;
  deck[1] = iid | 0x4000;
  g_world_magic_bitmap = 0;
  check(GetRemainingAllowedCardCopies(iid) == 0,
        "A flagged active copy must count; a market copy must not count");
  check(AddCardToDeckSorted(ADVENTURE_DECK_CARD_ID_MASK + 1) == -1,
        "IDs overlapping market flags must be rejected");
  return failures ? 1 : 0;
}
