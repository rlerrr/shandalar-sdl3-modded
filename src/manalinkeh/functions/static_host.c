#include "manalink.h"
#include "magic/src/manalink_runtime.h"

extern int mulligans_complete[2];
extern int challenge_round, challenge1, challenge2, challenge3, challenge4;

extern size_t duel_rules_snapshot_size(void);
extern void duel_rules_snapshot_capture(void *);
extern void duel_rules_snapshot_restore(const void *);

void manalinkeh_capture_runtime_snapshot(ManalinkRuntimeSnapshot *snapshot)
{
  memcpy(snapshot->extensions, card_ext, sizeof(snapshot->extensions));
  memcpy(snapshot->display_extensions, disp_card_ext, sizeof(snapshot->display_extensions));
  memcpy(snapshot->per_player_flags, card_flags, sizeof(snapshot->per_player_flags));
  memcpy(snapshot->uncounterable_colors, your_spells_cant_be_countered_by_color,
         sizeof(snapshot->uncounterable_colors));
  memcpy(snapshot->mana_added, mana_colors_added, sizeof(snapshot->mana_added));
  memcpy(snapshot->mana_colors, mana_color_override, sizeof(snapshot->mana_colors));
  memcpy(snapshot->mana_quantities, mana_quantity_override, sizeof(snapshot->mana_quantities));
  memcpy(snapshot->mana_transmutation, multi_mana_transmutation, sizeof(snapshot->mana_transmutation));
  memcpy(snapshot->dice_adjustments, add_to_die_rolls, sizeof(snapshot->dice_adjustments));
  memcpy(snapshot->extra_dice, roll_extra_dice, sizeof(snapshot->extra_dice));
  memcpy(snapshot->type_counts, card_counts, sizeof(snapshot->type_counts));
  memcpy(snapshot->graveyard_sources, graveyard_source, sizeof(snapshot->graveyard_sources));
  snapshot->next_graveyard_source = next_graveyard_source_id;
  snapshot->turns = turn_count;
  snapshot->prompt_for_color = always_prompt_for_color;
  memcpy(snapshot->mulligans, mulligans_complete, sizeof(snapshot->mulligans));
  snapshot->challenge_round = challenge_round;
  snapshot->challenge_modes[0] = challenge1;
  snapshot->challenge_modes[1] = challenge2;
  snapshot->challenge_modes[2] = challenge3;
  snapshot->challenge_modes[3] = challenge4;
  snapshot->added_counters = counters_added;
  if (duel_rules_snapshot_size() > sizeof(snapshot->rules))
    backtrace_and_abort("Static rules snapshot exceeds its storage");
  memset(snapshot->rules, 0, sizeof(snapshot->rules));
  duel_rules_snapshot_capture(snapshot->rules);
}

void manalinkeh_restore_runtime_snapshot(const ManalinkRuntimeSnapshot *snapshot)
{
  memcpy(card_ext, snapshot->extensions, sizeof(snapshot->extensions));
  memcpy(disp_card_ext, snapshot->display_extensions, sizeof(snapshot->display_extensions));
  memcpy(card_flags, snapshot->per_player_flags, sizeof(snapshot->per_player_flags));
  memcpy(your_spells_cant_be_countered_by_color, snapshot->uncounterable_colors,
         sizeof(snapshot->uncounterable_colors));
  memcpy(mana_colors_added, snapshot->mana_added, sizeof(snapshot->mana_added));
  memcpy(mana_color_override, snapshot->mana_colors, sizeof(snapshot->mana_colors));
  memcpy(mana_quantity_override, snapshot->mana_quantities, sizeof(snapshot->mana_quantities));
  memcpy(multi_mana_transmutation, snapshot->mana_transmutation, sizeof(snapshot->mana_transmutation));
  memcpy(add_to_die_rolls, snapshot->dice_adjustments, sizeof(snapshot->dice_adjustments));
  memcpy(roll_extra_dice, snapshot->extra_dice, sizeof(snapshot->extra_dice));
  memcpy(card_counts, snapshot->type_counts, sizeof(snapshot->type_counts));
  memcpy(graveyard_source, snapshot->graveyard_sources, sizeof(snapshot->graveyard_sources));
  next_graveyard_source_id = snapshot->next_graveyard_source;
  turn_count = snapshot->turns;
  always_prompt_for_color = snapshot->prompt_for_color;
  memcpy(mulligans_complete, snapshot->mulligans, sizeof(snapshot->mulligans));
  challenge_round = snapshot->challenge_round;
  challenge1 = snapshot->challenge_modes[0];
  challenge2 = snapshot->challenge_modes[1];
  challenge3 = snapshot->challenge_modes[2];
  challenge4 = snapshot->challenge_modes[3];
  counters_added = snapshot->added_counters;
  duel_rules_snapshot_restore(snapshot->rules);
}
