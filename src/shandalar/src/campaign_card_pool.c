#include "card_pool.h"
#include "defs.h"
#include "magic/src/carddata_generated.h"
#include <stdio.h>
#include <stdlib.h>

extern card_data_t global_cards_data[];
extern card_ptr_t global_raw_cards_storage[];
extern int global_available_slots;

static CardPool campaign_pool;
static int initialized;

void shandalar_card_pool_on_database_loaded(const char *directory)
{
  extern void LoadCardRaritiesAndCsvOffsets(void);
  if (global_available_slots <= 0) return;
  /* Raw rarity is copied during shared startup, after the opening menu. Join
   * campaign rarity again before building the configured acquisition pool. */
  LoadCardRaritiesAndCsvOffsets();
  shandalar_card_pool_initialize(directory);
}

void shandalar_card_pool_initialize(const char *directory)
{
  CardPoolCard *cards;
  int iid, enabled = 0;
  /* An early opening-menu call must not cache an empty pool as initialized. */
  if (global_available_slots <= 0) return;
  cards = calloc(SHANDALAR_CARD_DATA_ACTIVE_COUNT, sizeof(*cards));
  if (!cards) return;
  for (iid = 0; iid < SHANDALAR_CARD_DATA_ACTIVE_COUNT; ++iid) {
    int csvid = global_cards_data[iid].id;
    cards[iid].csvid = csvid;
    if (csvid >= 0 && csvid < global_available_slots) {
      card_ptr_t *raw = &global_raw_cards_storage[csvid];
      cards[iid].name = raw->full_name;
      /* Savegame card words reserve the upper bits for ownership/UI flags.
       * Never truncate a large internal ID into a different card. */
      cards[iid].eligible = iid <= ADVENTURE_DECK_CARD_ID_MASK &&
          /* card_dummy is also the legitimate handler for vanilla creatures;
           * type metadata distinguishes those from nonplayable placeholder rows. */
          global_cards_data[iid].code_pointer && (global_cards_data[iid].type & 0x7f) &&
          raw->card_type > CP_TYPE_NONE && raw->card_type != CP_TYPE_TOKEN &&
          raw->card_type <= CP_TYPE_PLANESWALKER && !(raw->flags & CPF_IS_TRANSFORMED_DFC);
      cards[iid].basic_land = iid < 5;
    }
  }
  initialized = card_pool_load(&campaign_pool, directory, cards, SHANDALAR_CARD_DATA_ACTIVE_COUNT);
  free(cards);
  if (!initialized) {
    fprintf(stderr, "Shandalar.ini: card pool not initialized; retaining original campaign pool\n");
    return;
  }
  for (iid = 0; iid < SHANDALAR_CARD_DATA_ACTIVE_COUNT; ++iid)
    enabled += card_pool_allows(&campaign_pool, iid, CARD_POOL_NOCHOOSE);
  fprintf(stderr, "Shandalar.ini: %d cards available for random rewards, %d dungeon treasures\n",
          enabled, campaign_pool.treasure_count);
}

int shandalar_card_pool_ready(void) { return initialized; }
int CampaignCardForSale(int iid)
{
  if (iid < 0 || iid >= SHANDALAR_CARD_DATA_ACTIVE_COUNT) return 0;
  return initialized ? card_pool_allows(&campaign_pool, iid, CARD_POOL_ENABLE)
                     : (global_cards_data[iid].expansion & 0xc1) != 0;
}
int CampaignCardIsTreasure(int iid)
{
  if (iid < 0 || iid >= SHANDALAR_CARD_DATA_ACTIVE_COUNT) return 0;
  return initialized ? shandalar_card_pool_treasure(iid)
                     : (global_cards_data[iid].extra_ability & 0x100) != 0;
}
int shandalar_card_pool_sale(int iid)
{ return initialized ? card_pool_allows(&campaign_pool, iid, CARD_POOL_ENABLE) : -1; }
int shandalar_card_pool_choose(int iid)
{ return initialized ? card_pool_allows(&campaign_pool, iid, CARD_POOL_NOSALE) : -1; }
int shandalar_card_pool_random(int iid)
{ return initialized ? card_pool_allows(&campaign_pool, iid, CARD_POOL_NOCHOOSE) : -1; }
int shandalar_card_pool_treasure(int iid)
{
  int i;
  if (!initialized) return -1;
  for (i = 0; i < campaign_pool.treasure_count; ++i) if (campaign_pool.treasures[i] == iid) return 1;
  return 0;
}
int shandalar_card_pool_rarity(int iid)
{
  if (!initialized || iid < 0 || iid >= SHANDALAR_CARD_DATA_ACTIVE_COUNT) return -1;
  if (shandalar_card_pool_treasure(iid)) return 4;
  /* Imported records already have CARDS.DAT rarity, with concise.csv restoring
   * the original campaign's rarity. Expansion/extra_ability no longer decide
   * eligibility or rarity for the full Manalink database. */
  return global_cards_data[iid].rarity;
}
