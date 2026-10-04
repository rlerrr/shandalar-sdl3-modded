#ifndef SHANDALAR_CARD_POOL_H
#define SHANDALAR_CARD_POOL_H

#include <stddef.h>

typedef enum {
  CARD_POOL_ENABLE, CARD_POOL_NOSALE, CARD_POOL_NOCHOOSE,
  CARD_POOL_DISABLE, CARD_POOL_DUNGEON_TREASURE
} CardPoolDisposition;

typedef struct {
  int csvid;
  const char *name;
  int eligible;
  int basic_land;
} CardPoolCard;

typedef struct {
  unsigned char *dispositions;
  size_t count;
  int treasures[30];
  int treasure_count;
} CardPool;

/* Reads Shandalar.ini, Menus.txt, and Rarity.dat from the asset directory.
 * Missing configuration means defaults. Invalid configuration returns zero. */
int card_pool_load(CardPool *pool, const char *directory,
                   const CardPoolCard *cards, size_t count);
void card_pool_free(CardPool *pool);
int card_pool_allows(const CardPool *pool, int iid, CardPoolDisposition maximum);

/* Adventure integration. Queries return -1 before initialization so extracted
 * engine tests and the standalone duel retain their original behavior. */
void shandalar_card_pool_initialize(const char *directory);
void shandalar_card_pool_on_database_loaded(const char *directory);
int shandalar_card_pool_sale(int iid);
int shandalar_card_pool_choose(int iid);
int shandalar_card_pool_random(int iid);
int shandalar_card_pool_treasure(int iid);
int shandalar_card_pool_rarity(int iid);
int shandalar_card_pool_ready(void);
int CampaignCardForSale(int iid);
int CampaignCardIsTreasure(int iid);

#endif
