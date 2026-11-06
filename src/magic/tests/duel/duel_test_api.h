#ifndef SHANDALAR_DUEL_TEST_API_H
#define SHANDALAR_DUEL_TEST_API_H
#include <stdint.h>
#include "magic/src/duel_host.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { DUEL_ZONE_HAND, DUEL_ZONE_BATTLEFIELD, DUEL_ZONE_LIBRARY, DUEL_ZONE_GRAVEYARD, DUEL_ZONE_EXILE };
enum { DUEL_STATE_TAPPED = 1, DUEL_STATE_SICK = 2, DUEL_STATE_ATTACKING = 4, DUEL_STATE_UNTAPPED = 8,
       DUEL_STATE_OPPONENT_OWNED = 16, DUEL_STATE_TOKEN = 32 };
typedef struct { int player, card; } DuelTestRef;
typedef struct {
  int csvid, power, toughness, tapped, sick, attacking, in_play, in_hand, damage, blocking_count, blocked, plus1_counters, minus1_counters;
} DuelTestCard;
typedef struct DuelTestSnapshot DuelTestSnapshot;
/* Read-only token observations; ordinary card inspection stays unchanged. */
int duel_test_token_characteristic(DuelTestRef ref, const char *property);
/* Actual shuffle entries since reset, restored by test and AI snapshots. */
int duel_test_shuffle_count(int player);
int duel_test_initialize(const char *assets);
int duel_test_handler_matches_override(int csvid);
int duel_test_energy(int player);
int duel_test_poison(int player);
const char *duel_test_card_name(DuelTestRef ref);
const char *duel_test_payment_prompt(void);
void duel_test_probe_presentation(void);
void duel_test_reset(uint32_t seed);
/* Adjust only the remaining deterministic work budget, preserving RNG/time. */
void duel_test_set_budget(uint32_t steps);
DuelTestRef duel_test_add(int player, int zone, int csvid, int state);
/* Challenge availability is test environment state, never a rules override. */
int duel_test_card_available(int csvid);
void duel_test_set_card_available(int csvid, int available);
void duel_test_prepare_fixture(void);
/* Refresh the cached characteristics normal gameplay updates between actions. */
void duel_test_refresh(void);
/* Exercise suspend's continuous haste duration without simulating its countdown. */
DuelTestRef duel_test_suspend_haste(DuelTestRef ref);
void duel_test_set_mana(int player, int color, int amount);
void duel_test_set_life(int player, int amount);
void duel_test_set_turn(int player, int combat);
void duel_test_decision(DuelChoiceKind kind, int player, int value, int card);
/* Select an engine-created pending damage packet by its actual recipient. */
void duel_test_damage_decision(int player, DuelTestRef recipient);
/* Activate only in the legacy destruction window; pass earlier responses. */
void duel_test_regeneration_decision(DuelTestRef source);
int duel_test_offered_count(void);
int duel_test_offered_selection_required(void);
int duel_test_offered_legal(int index);
int duel_test_forced_zone_choices(void);
/* 0 disables; 1/2 choose the lowest/highest CSVID actually offered for discard.
   Starts a fresh counted policy; both fields are caller-snapshot state. */
void duel_test_set_discard_policy(int policy);
int duel_test_discard_choice_count(void);
int duel_test_offered_csvid(int index);
void duel_test_finish_decisions(void);
int duel_test_finish_target_plan(void);
/* Count and retire unused choices so a rules failure can retain state assertions. */
int duel_test_finish_choice_plan(void);
DuelTestRef duel_test_permanent_at(int player, int index);
DuelTestRef duel_test_hand_at(int player, int index);
int duel_test_can_activate(DuelTestRef source);
int duel_test_activate(DuelTestRef source);
/* Use the real rules-engine hand-ability menu; scripts choose its option. */
int duel_test_can_activate_hand(DuelTestRef source);
int duel_test_activate_hand(DuelTestRef source);
/* GA_BASIC abilities and creature Unearth use a graveyard-index ref, not an
   instance. Scripts select the real RulesEngine menu option; other graveyard
   casting/return modes are unsupported. Costs/resolution stay in that engine. */
int duel_test_can_activate_graveyard(DuelTestRef source);
int duel_test_activate_graveyard(DuelTestRef source);
/* Register seeded grave cards as normal deck/hand startup would have done. */
void duel_test_prepare_graveyard_abilities(void);
/* Bounded noncreature Flashback via the real RulesEngine menu. Source is a
   graveyard-index ref; entry requires an empty stack. The native menu casts
   and resolves internally, so success returns after complete resolution.
   Only the requested outer menu entry is selected automatically; scripts
   still supply payment/target/other decisions. Query preserves snapshots. */
int duel_test_can_cast_graveyard(DuelTestRef source);
int duel_test_cast_graveyard(DuelTestRef source);
/* Query the card's EVENT_CAN_CAST handler without paying costs or choosing targets. */
int duel_test_can_cast(DuelTestRef source);
/* Normal unrestricted creature-spell targeting, independent of AI strategy.
   Suitable for Unsummon; does not model card-specific targeting restrictions. */
int duel_test_can_target_creature(DuelTestRef source, DuelTestRef target);
/* Inspect an actual ability definition without keeping costs or decisions.
 * Returns -1 if none was exposed, -2 for conflicting definitions, else 0/1. */
int duel_test_probe_activation_target(DuelTestRef source, DuelTestRef target, int mode_choice);
int duel_test_cast(DuelTestRef source);
/* Cast against the actual pending spell after a scripted cast has returned. */
int duel_test_cast_response(DuelTestRef source);
/* Start an explicitly authored death-history fixture as normal turn setup does. */
void duel_test_reset_death_history(void);
void duel_test_resolve(void);
/* Drain an explicit response while preserving the pending parent's context. */
void duel_test_resolve_response(void);
void duel_test_cleanup(void);
void duel_test_remove_from_combat(DuelTestRef ref);
void duel_test_destroy(DuelTestRef ref);
int duel_test_attack(DuelTestRef ref);
/* Declare a complete group, then run the real attack event and trigger pipeline. */
void duel_test_begin_attackers(void);
void duel_test_finish_attackers(void);
void duel_test_begin_blockers(void);
int duel_test_block(DuelTestRef blocker, DuelTestRef attacker);
/* Complete blocker declaration and enter the after-blocking response window. */
void duel_test_advance_combat(void);
void duel_test_combat_damage(void);
/* Resolve end-of-combat triggers, clear assignments, enter second main phase. */
void duel_test_end_combat(void);
/* Cleanup and the next player's real start-turn gate, then select main phase.
   Does not simulate untap, upkeep or draw; fixture zones and mana stay intact. */
void duel_test_advance_turn(void);
/* Run the real complete untap wrapper once after advance_turn. Requires the
   current player, empty stack and zero floating mana; returns 0 on rejection.
   Snapshot restore also restores the one-step preparation. No upkeep/draw. */
int duel_test_untap_step(int player);
DuelTestCard duel_test_inspect(DuelTestRef ref);
int duel_test_base_power(int player, int csvid);
int duel_test_base_toughness(int player, int csvid);
int duel_test_zone_count(int player, int zone);
int duel_test_zone_csvid(int player, int zone, int index);
int duel_test_life(int player);
int duel_test_mana(int player, int color);
int duel_test_stack_size(void);
void duel_test_check_invariants(void);
void duel_test_dump(void);
DuelTestSnapshot *duel_test_capture(void);
void duel_test_restore(const DuelTestSnapshot *snapshot);
void duel_test_free_snapshot(DuelTestSnapshot *snapshot);
void duel_test_ai_roundtrip(void);
int duel_test_ai_action(int player);
void duel_test_ai_turns(int count);
#ifdef __cplusplus
}
#endif
#endif
