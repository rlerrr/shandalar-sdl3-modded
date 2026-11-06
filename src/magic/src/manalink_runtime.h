#ifndef SHANDALAR_MANALINK_RUNTIME_H
#define SHANDALAR_MANALINK_RUNTIME_H

#include "defs.h"

/* Additional source-owned state saved alongside the host's engine globals. */
typedef struct ManalinkRuntimeSnapshot {
  card_ext_t extensions[2][151];
  card_ext_t display_extensions[2][151];
  uint32_t per_player_flags[2];
  uint8_t uncounterable_colors[2];
  int mana_added[2];
  int mana_colors[2][8];
  int mana_quantities[2][8];
  int mana_transmutation[2][2];
  int dice_adjustments[2];
  int extra_dice[2];
  uint8_t type_counts[2][7];
  int graveyard_sources[2][500];
  int next_graveyard_source;
  int turns;
  char prompt_for_color;
  int mulligans[2];
  int challenge_round;
  int challenge_modes[4];
  int added_counters;
  unsigned char rules[1024];
} ManalinkRuntimeSnapshot;

#ifdef __cplusplus
extern "C" {
#endif
void manalinkeh_capture_runtime_snapshot(ManalinkRuntimeSnapshot *);
void manalinkeh_restore_runtime_snapshot(const ManalinkRuntimeSnapshot *);
void manalinkeh_initialize_card_extensions(void);
void manalinkeh_memcpy_inst_and_ext(card_instance_t *, const card_instance_t *);
#ifdef __cplusplus
}
#endif
#endif
