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
extern int global_available_slots;
char card_coded[30000 / 8];
void shandalar_initialize_modded_card_database(void);
void initialize_for_shandalar(card_data_t *data, card_ptr_t *cards) { (void)data; (void)cards; }
void Cardartlib_initialize_for_shandalar(card_data_t *data, card_ptr_t *cards) { (void)data; (void)cards; }
void DrawCardModded_SetDisplayedCards(card_instance_t (*cards)[151]) { (void)cards; }
void DrawCardModded_SetInternalIdLookup(int (*lookup)(int)) { (void)lookup; }
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
int g_showlist_card_window_extra_bytes = 12;
HINSTANCE g_app_instance;
HWND g_duel_window_hwnd;
int campaign_test_cardlist_csvids[500], campaign_test_cardlist_count;
char g_last_parsed_deck_path[0x110];
int ParseDeckFileIntoInitialLibrary(char *, csvid_and_numcards *, unsigned int, int);
int DrawRandomCardFromInitialLibrary(int);
int CalculateCardShopPrice(int);
LRESULT CALLBACK wndproc_ShowListCard(HWND h, UINT m, WPARAM w, LPARAM l)
{ (void)h; (void)m; (void)w; (void)l; return 0; }
INT_PTR CALLBACK dlgfunc_show_deck(HWND h, UINT m, WPARAM w, LPARAM l)
{ (void)h; (void)m; (void)w; (void)l; return 0; }

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

static void check_card_prices(void)
{
  // Golden values from the modded DLL's --show-prices export. Check before the
  // campaign set policy overrides rarity. The export uses rare for Mana Crypt
  // and Black Lotus, and lacks the port's restored classic campaign bonuses.
  const struct { int csvid, price, rarity; } expected[] = {
    {6, 640, 3}, {17, 200, 3}, {48, 100, 2}, {55, 120, 1},
    {84, 100, 2}, {106, 58, 1}, {145, 140, 1}, {149, 120, 1},
    {221, 100, 2}, {894, 1000, 3}, {968, 75, 1},
    {1124, 80, 1}, {2049, 100, 2}, {11110, 100, 2}
  };
  unsigned int i;
  for (i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    int iid = FindCardIndexByCsvid(expected[i].csvid);
    int price = -1;
    card_data_t saved;
    if (iid >= 0) {
      saved = global_cards_data[iid];
      global_cards_data[iid].rarity = expected[i].rarity;
      global_cards_data[iid].extra_ability &= ~0x780U;
      price = CalculateCardShopPrice(iid);
      global_cards_data[iid] = saved;
    }
    if (price != expected[i].price) {
      fprintf(stderr, "Price for CSV %d (%s): expected %d, got %d\n",
              expected[i].csvid, iid >= 0 ? global_raw_cards_storage[expected[i].csvid].full_name : "missing",
              expected[i].price, price);
      ++failures;
    }
  }
  check(global_cards_data[FindCardIndexByCsvid(2049)].ai_base_value == 0,
        "Nature's Lore must use the engine rating, not its Cards.dat AI score of 60");
  check(global_cards_data[FindCardIndexByCsvid(106)].ai_base_value == 1,
        "Grizzly Bears must retain its signed ct_all engine rating");
  check(global_cards_data[FindCardIndexByCsvid(894)].ai_base_value == -2,
        "Mana Crypt's negative engine rating must remain signed");
  check(global_raw_cards_storage[2049].ai_base_value == 60,
        "Correcting the engine rating must preserve Nature's Lore's separate AI score");
  {
    int iid = FindCardIndexByCsvid(106);
    card_data_t saved = global_cards_data[iid];
    casting_cost_t saved_cost = global_raw_cards_storage[106].req;
    global_cards_data[iid].rarity = 1;
    global_cards_data[iid].power |= 0x4000;
    check(CalculateCardShopPrice(iid) == 58, "Power flags must not inflate creature prices");
    global_raw_cards_storage[106].req.req_hybrid = 2;
    check(CalculateCardShopPrice(iid) == 35, "Hybrid mana must count in the price divisor");
    global_raw_cards_storage[106].req = saved_cost;
    global_cards_data[iid].power = -2;
    global_cards_data[iid].ai_base_value = -2;
    check(CalculateCardShopPrice(iid) == 5, "Negative creature ratings must respect the minimum price");
    global_cards_data[iid].power = global_cards_data[iid].toughness = 1000;
    global_cards_data[iid].extra_ability = 0x600;
    check(CalculateCardShopPrice(iid) == 29997, "Price bonuses apply after the 9999 base-price cap");
    global_cards_data[iid] = saved;
  }
  {
    int iid = FindCardIndexByCsvid(84);
    card_data_t saved = global_cards_data[iid];
    global_cards_data[iid].rarity = 1;
    global_cards_data[iid].extra_ability = 0;
    check(CalculateCardShopPrice(iid) == 100, "X must count once instead of adding the CARDS.DAT sentinel of 40");
    global_cards_data[iid].type = 0x20;
    check(CalculateCardShopPrice(iid) == 240, "X-cost interrupts must include the DLL's price multiplier");
    global_cards_data[iid].type = 4;
    global_cards_data[iid].cc[2] = 9;
    check(CalculateCardShopPrice(iid) == 125, "Aura type encoding must use enchantment pricing");
    global_cards_data[iid] = saved;
  }
}

static int test_iid(const char *name)
{
  for (int csvid = 0; csvid < global_available_slots; ++csvid)
    if (strcmp(global_raw_cards_storage[csvid].full_name, name) == 0)
      return FindCardIndexByCsvid(csvid);
  check(0, name);
  return -1;
}

static void check_full_campaign_availability(const char *fixture_directory, const char *fixture_ini)
{
  FILE *fixture = fopen(fixture_ini, "w");
  if (!fixture) { check(0, "Create empty campaign config"); return; }
  fclose(fixture);
  shandalar_card_pool_initialize(fixture_directory);
  check(shandalar_card_pool_ready(), "Load default campaign pool");
  const char *enabled[] = {"Bastion Mastodon", "Blooming Marsh", "Aegis Automaton",
                          "Battlefield Scavenger", "Cowl Prowler", "Grizzly Bears"};
  for (unsigned int i = 0; i < sizeof(enabled) / sizeof(enabled[0]); ++i)
    check(shandalar_card_pool_sale(test_iid(enabled[i])), enabled[i]);
  const char *disabled[] = {"Besmirch", "Pointed Discussion", "Quick Sliver",
                           "Insectile Aberration", "Bala Ged Sanctuary"};
  for (unsigned int i = 0; i < sizeof(disabled) / sizeof(disabled[0]); ++i)
    check(!shandalar_card_pool_random(test_iid(disabled[i])), disabled[i]);
  check(CampaignCardIsTreasure(test_iid("Contract from Below")),
        "Format-ban flags must not exclude dungeon treasures");
  int total = 0, newer = 0, treasures = 0;
  for (int iid = 0; iid < g_card_count; ++iid) {
    total += shandalar_card_pool_random(iid);
    newer += iid > 0x3fff && shandalar_card_pool_random(iid);
    treasures += CampaignCardIsTreasure(iid);
  }
  fprintf(stderr, "Default campaign: %d random rewards, %d above IID 16383, %d treasures\n",
          total, newer, treasures);
  check(newer > 2500 && treasures == 30, "Full implemented pool exceeds the old ID limit");

  int iid = test_iid("Bastion Mastodon");
  for (int i = 0; i < 500; ++i) deck[i] = -1;
  _currentDeck = 1;
  int slot = AddCardToDeckSorted(iid);
  check(iid == 16384 && slot >= 0 && adventure_deck_card_id(deck[slot]) == iid,
        "First formerly excluded IID survives collection insertion");
  check(CardIDFromType(deck[slot]) == global_cards_data[iid].id &&
        !(deck[slot] & ADVENTURE_DECK_AVAILABLE) && (deck[slot] & 0x20000),
        "IID bit 14 must not become a sideboard flag");
  deck[slot] = adventure_deck_set_available(deck[slot], 1, _currentDeck) | ADVENTURE_DECK_SELECTED;
  RebuildDeckEntriesByCardGroup();
  check(adventure_deck_card_id(deck[0]) == iid && (deck[0] & ADVENTURE_DECK_AVAILABLE) &&
        (deck[0] & ADVENTURE_DECK_SELECTED) && !(deck[0] & 0x20000),
        "Collection regrouping preserves high IDs and sideboard/selection state");
  _currentDeck = 0;

  fixture = fopen(fixture_ini, "w");
  if (!fixture) { check(0, "Create explicit card policy"); return; }
  fputs("[Sets]\n--=disable\n[Enable]\nBastion Mastodon=\n", fixture);
  fclose(fixture);
  shandalar_card_pool_initialize(fixture_directory);
  check(shandalar_card_pool_sale(iid) && !shandalar_card_pool_sale(test_iid("Blooming Marsh")),
        "Explicit Shandalar.ini policy still controls the expanded pool");
}

int main(int argc, char **argv)
{
  const int expected_counts[4] = {36, 40, 46, 46};
  int color, difficulty, slot, iid, count, basic_lands, high_ids, total_high_ids = 0;
  int configured_pool;
  char fixture_directory[1024], fixture_ini[1100];
  FILE *fixture;
  if (argc == 2 && strcmp(argv[1], "--coded-mask-only") == 0) {
    global_available_slots = 30000;
    memset(card_coded, 0xff, sizeof(card_coded));
    shandalar_initialize_modded_card_database();
    check((card_coded[239 / 8] & (1 << (239 % 8))) != 0,
          "Database initialization must expose implemented cards to Wish and booster searches");
    check((card_coded[10440 / 8] & (1 << (10440 % 8))) == 0,
          "A null registry binding must remain unavailable to outside-game searches");
    check(card_coded[sizeof(card_coded) - 1] == 0,
          "Database reloads must clear stale availability bits");
    global_available_slots = 100;
    shandalar_initialize_modded_card_database();
    check((card_coded[239 / 8] & (1 << (239 % 8))) == 0,
          "Availability must respect the loaded database boundary on reload");
    return failures ? 1 : 0;
  }
  int availability_only = argc == 3 && strcmp(argv[1], "--availability-only") == 0;
  if (availability_only) { argv[1] = argv[2]; argc = 2; }
  if (!_getcwd(fixture_directory, sizeof(fixture_directory) - 64)) return 2;
  strcat(fixture_directory, "/campaign_card_pool_test_assets");
  _mkdir(fixture_directory);
  snprintf(fixture_ini, sizeof(fixture_ini), "%s/Shandalar.ini", fixture_directory);
  if (argc != 2 || campaign_chdir(argv[1]) != 0) return 2;
  g_card_count = SHANDALAR_CARD_DATA_ACTIVE_COUNT;
  if (availability_only) {
    if (!read_db_guts("Cards.dat")) return 2;
    CopyRawCardNamesAndRarities();
    check_full_campaign_availability(fixture_directory, fixture_ini);
    free(global_base_txt);
    return failures ? 1 : 0;
  }

  for (iid = 0; iid < g_card_count; ++iid)
    check(CardIDFromInternalIndex(iid) == global_cards_data[iid].id,
          "Raw duel indices must preserve every card index bit");
  check(CardIDFromInternalIndex((unsigned int)-1) == -1,
        "Invalid duel index must not resolve to a card");
  check(CardIDFromInternalIndex(SHANDALAR_CARD_DATA_COUNT) == -1,
        "Out-of-range duel index must not read beyond the card table");
  memset(g_initial_library, 0, sizeof(g_initial_library));
  g_initial_library[1][0].csvid = 6426; /* Quick Sliver: null registry binding. */
  g_initial_library[1][0].numcards = 1;
  iid = DrawRandomCardFromInitialLibrary(1);
  check(iid == -1, "An unsupported deck card must never become an arbitrary internal card");
  fixture = fopen(fixture_ini, "w");
  if (!fixture) return 2;
  fputs("Druid regression\n\n.6426 1 Quick Sliver\n.91 5 Forest\n"
        ".4431 1 Clear the Land\n.4766 2 Skyshroud Claim\n"
        ".4100 1 Braidwood Sextant\n.3723 1 Citanul Centaurs\n"
        ".1909 1 Elder Druid\n", fixture);
  fclose(fixture);
  memset(g_initial_library, 0, sizeof(g_initial_library));
  ParseDeckFileIntoInitialLibrary(fixture_ini, g_initial_library[1], 0, -1);
  count = 0;
  for (slot = 0; slot < 200; ++slot) {
    count += g_initial_library[1][slot].numcards;
    if (g_initial_library[1][slot].numcards)
      check(FindCardIndexByCsvid(g_initial_library[1][slot].csvid) >= 0,
            "Unsupported Druid entries must be excluded from the draw pool");
  }
  check(count == 11 && g_initial_library[1][1].numcards == 5,
        "Skipping unsupported entries must preserve every supported card and basic land");
  for (slot = 0; slot < 11; ++slot) {
    iid = DrawRandomCardFromInitialLibrary(1);
    check(iid >= 0 && (global_cards_data[iid].id == CARD_ID_FOREST ||
                      global_cards_data[iid].id == CARD_ID_SKYSHROUD_CLAIM ||
                      global_cards_data[iid].id == CARD_ID_ELDER_DRUID ||
                      global_cards_data[iid].id == CARD_ID_CLEAR_THE_LAND ||
                      global_cards_data[iid].id == CARD_ID_BRAIDWOOD_SEXTANT ||
                      global_cards_data[iid].id == CARD_ID_CITANUL_CENTAURS),
          "Every initialized library card must belong to the supported deck definition");
  }
  check(DrawRandomCardFromInitialLibrary(1) == -1, "The initialized library ends after its last real card");
  {
    const int csvids[] = {CARD_ID_FOREST, CARD_ID_SKYSHROUD_CLAIM,
                         CARD_ID_WHIRLWIND, CARD_ID_VITALIZE,
                         CARD_ID_ELDER_DRUID, CARD_ID_LIVING_LANDS,
                         CARD_ID_SILVERGLADE_ELEMENTAL};
    int cards[8];
    for (slot = 0; slot < 7; ++slot) cards[slot] = FindCardIndexByCsvid(csvids[slot]);
    cards[7] = -1;
    check(show_cardlist(cards, NULL, NULL, 8, "Opponent library", 1, "") == 7,
          "Library viewer must preserve the selected index");
    check(campaign_test_cardlist_count == 7, "Library viewer stops at its sentinel");
    for (slot = 0; slot < 7; ++slot)
      check(campaign_test_cardlist_csvids[slot] == csvids[slot],
            "Library viewer must show the actual card, including internal IDs above 4095");
    cards[1] |= 0x10000;
    show_cardlist(cards, NULL, NULL, 8, "Campaign cards", 0, "");
    check(campaign_test_cardlist_csvids[1] == CARD_ID_SKYSHROUD_CLAIM,
          "Library viewer must still strip campaign new-card flags");
    cards[0] = SHANDALAR_CARD_DATA_ACTIVE_COUNT - 1;
    cards[1] = -1;
    show_cardlist(cards, NULL, NULL, 2, "High duel index", 1, "");
    check(campaign_test_cardlist_csvids[0] == global_cards_data[cards[0]].id,
          "Library viewer must preserve duel indices above the packed deck limit");
  }
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
        iid = adventure_deck_card_id(deck[slot]);
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
        iid = adventure_deck_card_id(deck[slot]);
        check(original_counts[iid]-- > 0, "Deck rebuild must preserve every card's identity");
      }
    }
  }
  check(total_high_ids > 0, "Exercise imported campaign IDs above the old 12-bit limit");

  /* Mesa Pegasus has an imported ID above 4095. Its highlight/selection flags
     must not change copy limits, and market copies must remain excluded. */
  iid = FindCardIndexByCsvid(CARD_ID_MESA_PEGASUS);
  check(iid > 4095 && iid <= ADVENTURE_DECK_MAX_CARD_ID, "Expected a large Mesa Pegasus ID");
  for (slot = 0; slot < 500; ++slot) deck[slot] = -1;
  deck[0] = iid | 0x10000 | ADVENTURE_DECK_SELECTED;
  deck[1] = iid | ADVENTURE_DECK_AVAILABLE;
  g_world_magic_bitmap = 0;
  check(GetRemainingAllowedCardCopies(iid) == 0,
        "A flagged active copy must count; a market copy must not count");
  check(AddCardToDeckSorted(ADVENTURE_DECK_MAX_CARD_ID + 1) == -1,
        "IDs outside the signed engine range must be rejected");

  check(read_db_guts("cards.dat"), "Load real card names, types, flags, and rarity");
  CopyRawCardNamesAndRarities();
  check_card_prices();
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
          iid = adventure_deck_card_id(deck[slot]);
          check(CampaignCardForSale(iid), "Configured starters must respect sale eligibility");
        }
        check(count == expected_counts[difficulty], "Configured starters must keep the original deck size even with only basic lands");
      }
    }
  }
  free(global_base_txt);
  return failures ? 1 : 0;
}

/* This isolated campaign test uses the ordinary host RNG and its srand seeds. */
int duel_host_random(void) { return rand(); }
