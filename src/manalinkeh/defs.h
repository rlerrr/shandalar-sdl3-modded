#ifndef MANALINKEH_DEFS_H
#define MANALINKEH_DEFS_H

#include "../defs.h"

#ifdef MANALINKEH_STATIC
extern int shandalar_get_poison_counter(int player);
extern void shandalar_set_poison_counter(int player, uint8_t value);
#undef POISON_COUNTERS
#define POISON_COUNTERS(p) shandalar_get_poison_counter(p)
#define SET_POISON_COUNTERS(p, value) shandalar_set_poison_counter((p), (value))
#endif

#endif
