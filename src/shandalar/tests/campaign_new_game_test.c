/* Exercise campaign setup with the actual imported table and selection code. */
#define GLOBAL_STATE_IMPL
#define GLOBALS_OTHERS_IMPL
#include "shandalar/src/shandalar_internal.h"
#include "magic/src/game_support.h"
#include "magic/src/global_other.h"
#include "deckdll/src/card_db.h"
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
int CardTypeFromID(int csvid) { return FindCardIndexByCsvid(csvid); }
int card_dummy(int p, int c, event_t e) { (void)p; (void)c; (void)e; return 0; }
int campaign_test_handler(int p, int c, event_t e) { (void)p; (void)c; (void)e; return 1; }
int CopyRawCardNamesAndRarities(void);
int PickConfiguredTownShopCard(int, unsigned int, int);

void AddJournalEntry(int type, int arg) { (void)type; (void)arg; }
static void check(int condition, const char *message)
{
  if (!condition) { fprintf(stderr, "%s\n", message); ++failures; }
}

static void copy_fixture_asset(const char *directory, const char *name)
{
  char path[1200], buffer[4096];
  size_t n;
  FILE *in, *out;
  snprintf(path, sizeof(path), "%s/%s", directory, name);
  in = fopen(name, "rb"); out = fopen(path, "wb");
  if (!in || !out) { check(0, "Copy set metadata into writable test fixture"); }
  else while ((n = fread(buffer, 1, sizeof(buffer), in)) != 0) check(fwrite(buffer, 1, n, out) == n, "Write fixture metadata");
  if (in) fclose(in);
  if (out) fclose(out);
}

int main(int argc, char **argv)
{
  const int expected_counts[4] = {36, 40, 46, 46};
  int color, difficulty, slot, iid, count, basic_lands, high_ids, total_high_ids = 0;
  int configured_pool;
  char fixture_directory[1024], fixture_ini[1100];
  FILE *fixture;
  if (!_getcwd(fixture_directory, sizeof(fixture_directory) - 64)) return 2;
  strcat(fixture_directory, "/campaign_card_pool_test_assets");
  _mkdir(fixture_directory);
  snprintf(fixture_ini, sizeof(fixture_ini), "%s/Shandalar.ini", fixture_directory);
  if (argc != 2 || campaign_chdir(argv[1]) != 0) return 2;
  g_card_count = SHANDALAR_CARD_DATA_ACTIVE_COUNT;
  shandalar_card_pool_initialize(argv[1]);
  shandalar_card_pool_on_database_loaded(argv[1]);
  check(!shandalar_card_pool_ready(), "Opening-menu calls before CARDS.DAT loading must not cache an empty pool");
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

  check(read_db_guts("cards.dat"), "Load real card names, types, flags, and rarity");
  CopyRawCardNamesAndRarities();
  shandalar_card_pool_on_database_loaded(argv[1]);
  check(shandalar_card_pool_ready() && shandalar_card_pool_sale(0),
        "Database-load callback initializes a nonempty campaign pool");
  copy_fixture_asset(fixture_directory, "Menus.txt");
  copy_fixture_asset(fixture_directory, "Rarity.dat");
  fixture = fopen(fixture_ini, "w");
  if (!fixture) return 2;
  fputs("[Sets]\n--Ice Age=enable\n--=disable\n", fixture); fclose(fixture);
  shandalar_card_pool_initialize(fixture_directory);
  check(shandalar_card_pool_sale(FindCardIndexByCsvid(CARD_ID_GRIZZLY_BEARS)), "Real early printings survive later reprints");
  check(!shandalar_card_pool_random(FindCardIndexByCsvid(CARD_ID_DOOM_BLADE)), "Real later cards are excluded by an Ice Age cutoff");
  for (configured_pool = 1; configured_pool <= 2; ++configured_pool) {
    if (configured_pool == 1) shandalar_card_pool_initialize(argv[1]);
    else {
      fixture = fopen(fixture_ini, "w");
      if (!fixture) return 2;
      fputs("[Sets]\n--=disable\n", fixture); fclose(fixture);
      shandalar_card_pool_initialize(fixture_directory);
    }
    check(shandalar_card_pool_ready(), "Initialize real campaign policy");
    for (color = 1; color <= 5; ++color) {
      for (slot = 0; slot < 8; ++slot) {
        iid = PickConfiguredTownShopCard(slot, 1U << color, slot == 0);
        check(iid >= 0 && CampaignCardForSale(iid), "Shops choose an eligible card even in a basic-only pool");
        if (configured_pool == 2) check(iid < 5, "Restricted shops offer only enabled basics");
      }
    }
    check(PickConfiguredTownShopCard(0, 0, 1) == -1, "An empty shop category finishes without retrying forever");
    if (configured_pool == 1) {
      int available = 0, treasures = 0;
      for (iid = 0; iid < g_card_count; ++iid) {
        available += shandalar_card_pool_random(iid);
        treasures += shandalar_card_pool_treasure(iid);
      }
      check(available > 10000 && treasures == 30, "Real configuration expands the pool and places exactly 30 treasures");
      check(shandalar_card_pool_sale(FindCardIndexByCsvid(CARD_ID_GRIZZLY_BEARS)),
            "Vanilla creatures legitimately use card_dummy and must remain enabled");
    }
    for (difficulty = 0; difficulty < 4; ++difficulty) {
      for (color = 1; color <= 5; ++color) {
        g_selected_wizard_color = color;
        g_shandalar_difficulty = difficulty;
        g_skip_new_game_state_initialization = g_world_magic_bitmap = 0;
        InitializeNewGameState();
        count = 0;
        for (slot = 0; slot < 500; ++slot) {
          if (deck[slot] == -1) continue;
          ++count;
          iid = deck[slot] & ADVENTURE_DECK_CARD_ID_MASK;
          check(CampaignCardForSale(iid), "Configured starters must respect sale eligibility");
        }
        check(count == expected_counts[difficulty], "Configured starters must keep the original deck size even with only basic lands");
      }
    }
  }
  free(global_base_txt);
  return failures ? 1 : 0;
}
