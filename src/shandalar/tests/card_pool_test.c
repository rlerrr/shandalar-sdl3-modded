#include "shandalar/src/card_pool.h"
#include "platform/iniparser.h"
#include <direct.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures, visited;
static CardPoolCard cards[40];
static char names[40][80];
static CardPool pool;

static void check(int ok, const char *message)
{ if (!ok) { fprintf(stderr, "%s\n", message); ++failures; } }
static void write_text(const char *name, const char *text)
{
  FILE *file = fopen(name, "wb");
  if (!file) { ++failures; return; }
  fputs(text, file); fclose(file);
}
static void load(const char *text)
{
  write_text("Shandalar.ini", text);
  check(card_pool_load(&pool, ".", cards, 40), "Configuration should load");
}
static int allowed(int iid, CardPoolDisposition maximum)
{ return card_pool_allows(&pool, iid, maximum); }
static void entry(void *context, const char *section, const char *key, const char *value)
{ (void)context; (void)section; (void)key; (void)value; ++visited; }

int main(void)
{
  int i;
  FILE *file;
  dictionary *dict;
  unsigned char header[8] = {40, 0, 0, 0, 14, 0, 0, 0};
  _mkdir("card_pool_test_assets");
  if (_chdir("card_pool_test_assets")) return 2;
  for (i = 0; i < 40; ++i) {
    snprintf(names[i], sizeof(names[i]), "Card %02d with a long untruncated name", i);
    cards[i].csvid = i; cards[i].name = names[i]; cards[i].eligible = i != 5;
  }
  cards[0].basic_land = 1;
  write_text("Menus.txt", "@EXPANSIONNAMES\n12\nFormat0\nFormat1\nFormat2\nFormat3\nFormat4\nFormat5\nFormat6\nFormat7\nPromo\nAlpha\nBeta\nLater\n");
  file = fopen("Rarity.dat", "wb");
  if (!file) return 2;
  fwrite(header, 1, 8, file);
  for (i = 0; i < 40; ++i) {
    uint64_t bits = 0;
    unsigned char bytes[6];
    int j;
    if (i == 0 || i == 2) bits |= UINT64_C(2) << 30; /* Beta straddles a byte boundary. */
    if (i == 1) bits |= UINT64_C(1) << 27;
    if (i >= 1 && i <= 3) bits |= UINT64_C(3) << 33;
    for (j = 0; j < 6; ++j) bytes[j] = (unsigned char)(bits >> (8 * j));
    fwrite(bytes, 1, 6, file);
  }
  fclose(file);

  load("; Default: all eligible cards\n[Sets]\n");
  check(allowed(1, CARD_POOL_ENABLE) && !allowed(5, CARD_POOL_NOCHOOSE), "Default enables playable cards only");
  load("[Sets]\n--Beta=enable\n--=disable\n");
  check(allowed(1, CARD_POOL_ENABLE) && allowed(2, CARD_POOL_ENABLE), "An early printing wins over later reprints");
  check(!allowed(3, CARD_POOL_NOCHOOSE) && !allowed(4, CARD_POOL_NOCHOOSE), "Later and unprinted cards follow the all-card fallback");
  check(allowed(0, CARD_POOL_ENABLE), "Basic lands always enabled");
  load("[Sets]\n--=nosale\n--=disable\n");
  check(!allowed(1, CARD_POOL_ENABLE) && allowed(1, CARD_POOL_NOSALE), "Duplicate rules retain first-match order");
  load("[Sets]\nLater=disable\n--=enable\nLater=ignore\n");
  check(allowed(3, CARD_POOL_ENABLE), "Ignore is preprocessed even when listed last");
  load("[Sets]\nLater=ignore\n--=disable\n");
  check(!allowed(3, CARD_POOL_NOCHOOSE), "All-card rules include ignored printings");
  load("[Disable]\nCard 01 with a long untruncated name\n[Enable]\n1\n5\n[Sets]\n--=disable\n[Nosale]\n2\n[Nochoose]\n3\n");
  check(!allowed(1, CARD_POOL_NOCHOOSE), "Individual restrictions override Enable regardless of section order");
  check(!allowed(2, CARD_POOL_ENABLE) && allowed(2, CARD_POOL_NOSALE), "Nosale still allows choosing");
  check(!allowed(3, CARD_POOL_NOSALE) && allowed(3, CARD_POOL_NOCHOOSE), "Nochoose still allows random rewards");
  check(allowed(0, CARD_POOL_ENABLE) && !allowed(5, CARD_POOL_NOCHOOSE), "Overrides cannot enable invalid cards or disable basics");

  write_text("additional.ini", "[Sets]\n--=disable\n[Disable]\n1\n[Configs]\nShandalar.ini\n");
  load("[Configs]\nadditional.ini\nadditional.ini\n[Sets]\n--=enable\n");
  check(allowed(2, CARD_POOL_ENABLE) && !allowed(1, CARD_POOL_NOCHOOSE), "Parent rules precede includes; overrides apply across files; cycles terminate");
  write_text("Shandalar.ini", "[DungeonTreasure]\n");
  file = fopen("Shandalar.ini", "ab");
  for (i = 6; i < 40; ++i) fprintf(file, "%d\n", i);
  fprintf(file, "6\n0\n"); fclose(file);
  check(card_pool_load(&pool, ".", cards, 40), "Treasure configuration should load");
  check(pool.treasure_count == 30 && pool.treasures[0] == 6 && pool.treasures[29] == 35, "Only the first 30 unique playable treasures are placed");
  check(!allowed(39, CARD_POOL_NOCHOOSE) && allowed(0, CARD_POOL_ENABLE), "Overflow treasures remain excluded and basic lands remain enabled");

  cards[1].name = "Ancestral Recall";
  load("[Sets]\n");
  check(pool.treasure_count == 1 && pool.treasures[0] == 1, "Missing treasure list uses documented defaults");
  write_text("Shandalar.ini", "[Sets]\nUnknown Set=disable\n");
  check(!card_pool_load(&pool, ".", cards, 40), "Unknown set names must not silently widen the pool");
  check(pool.treasure_count == 1, "Failed parsing leaves the prior policy intact");

  write_text("strict.ini", "[Disable]\nA");
  file = fopen("strict.ini", "r"); dict = iniparser_load_file(file, "strict.ini"); fclose(file);
  check(!dict, "Registry parser remains strict about bare keys"); iniparser_freedict(dict);
  file = fopen("strict.ini", "r"); dict = iniparser_load_file_entries(file, "strict.ini", entry, NULL); fclose(file);
  check(dict && visited == 1, "Ordered parser accepts one-character bare keys"); iniparser_freedict(dict);
  check(!allowed(-1, CARD_POOL_NOCHOOSE) && !allowed(40, CARD_POOL_NOCHOOSE), "Invalid indices are safely excluded");
  card_pool_free(&pool);
  return failures ? 1 : 0;
}
