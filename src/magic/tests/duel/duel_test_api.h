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
void duel_test_prepare_fixture(void);
/* Refresh the cached characteristics normal gameplay updates between actions. */
void duel_test_refresh(void);
void duel_test_set_mana(int player, int color, int amount);
void duel_test_set_life(int player, int amount);
void duel_test_set_turn(int player, int combat);
void duel_test_decision(DuelChoiceKind kind, int player, int value, int card);
/* Select an engine-created pending damage packet by its actual recipient. */
void duel_test_damage_decision(int player, DuelTestRef recipient);
/* Activate only in the legacy destruction window; pass earlier responses. */
void duel_test_regeneration_decision(DuelTestRef source);
int duel_test_offered_count(void);
int duel_test_forced_zone_choices(void);
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
/* Query the card's EVENT_CAN_CAST handler without paying costs or choosing targets. */
int duel_test_can_cast(DuelTestRef source);
/* Normal unrestricted creature-spell targeting, independent of AI strategy.
   Suitable for Unsummon; does not model card-specific targeting restrictions. */
int duel_test_can_target_creature(DuelTestRef source, DuelTestRef target);
int duel_test_cast(DuelTestRef source);
void duel_test_resolve(void);
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
DuelTestCard duel_test_inspect(DuelTestRef ref);
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
