#include "drawcardlib/Drawcardlib.h"

int DrawCardModdedBack(HDC dc, const RECT *rect, unsigned int mode,
                       int arg1, int arg2, int arg3, int arg4);
int DrawCardModdedDestroyAllCardBackgrounds(void);
int DrawCardModdedSmallCardTitle(HDC dc, const RECT *rect, const char *title,
                                int color, int controlled_by_owner,
                                const card_ptr_t *card, int player, int slot);
int DrawCardModdedSmallCard(HDC dc, const RECT *rect, const card_ptr_t *card,
                           int version, int mode, int player, int slot);

/* The host retains the original short APIs; the modded renderer extends them. */
void DestroyAllCardBackgrounds(void)
{
  DrawCardModdedDestroyAllCardBackgrounds();
}

void DrawCardBack(HDC dc, RECT *rect)
{
  DrawCardModdedBack(dc, rect, 0, 0, 0, 0, 0);
}

void DrawSmallCardTitle(HDC dc, RECT *rect, char *title, int color, int transparent)
{
  DrawCardModdedSmallCardTitle(dc, rect, title, color, transparent & ~8, NULL, -1, -1);
}

void DrawSmallCard(HDC dc, RECT *rect, card_ptr_t *card, uint version, int reload)
{
  DrawCardModdedSmallCard(dc, rect, card, version, reload, -1, -1);
}
