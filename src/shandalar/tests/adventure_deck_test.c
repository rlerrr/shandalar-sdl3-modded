#include "defs.h"

int main(void)
{
  if (adventure_deck_card_id(-1) != -1 || adventure_deck_entry(-1) != -1)
    return 1;
  for (int iid = 0; iid <= ADVENTURE_DECK_MAX_CARD_ID; ++iid) {
    for (int flags = 0; flags < 32; ++flags) {
      int entry = iid | (flags << 15);
      if (adventure_deck_card_id(entry) != iid)
        return 2;
      for (int slot = 0; slot < 3; ++slot) {
        int outside = adventure_deck_set_available(entry, 1, slot);
        int inside = adventure_deck_set_available(outside, 0, slot);
        int member = 1 << (16 + slot);
        if (adventure_deck_card_id(outside) != iid || adventure_deck_card_id(inside) != iid ||
            !(outside & ADVENTURE_DECK_AVAILABLE) || (outside & member) ||
            (inside & ADVENTURE_DECK_AVAILABLE) || !(inside & member) ||
            ((inside ^ entry) & ~(ADVENTURE_DECK_AVAILABLE | member)))
          return 3;
      }
    }
  }
  return 0;
}
