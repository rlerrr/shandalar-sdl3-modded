#ifndef SHANDALAR_DUEL_TEST_API_H
#define SHANDALAR_DUEL_TEST_API_H
#include <stdint.h>
#include "magic/src/duel_host.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { DUEL_ZONE_HAND, DUEL_ZONE_BATTLEFIELD, DUEL_ZONE_LIBRARY, DUEL_ZONE_GRAVEYARD, DUEL_ZONE_EXILE };
enum { DUEL_STATE_TAPPED = 1, DUEL_STATE_SICK = 2, DUEL_STATE_ATTACKING = 4, DUEL_STATE_UNTAPPED = 8 };
typedef struct { int player, card; } DuelTestRef;
typedef struct {
  int csvid, power, toughness, tapped, sick, attacking, in_play, in_hand, damage;
} DuelTestCard;
typedef struct DuelTestSnapshot DuelTestSnapshot;
int duel_test_initialize(const char *assets);
void duel_test_probe_presentation(void);
void duel_test_reset(uint32_t seed);
DuelTestRef duel_test_add(int player, int zone, int csvid, int state);
void duel_test_prepare_fixture(void);
void duel_test_set_mana(int player, int color, int amount);
void duel_test_set_life(int player, int amount);
void duel_test_set_turn(int player, int combat);
void duel_test_decision(DuelChoiceKind kind, int player, int value, int card);
void duel_test_finish_decisions(void);
int duel_test_can_activate(DuelTestRef source);
int duel_test_activate(DuelTestRef source);
/* Query the card's EVENT_CAN_CAST handler without paying costs or choosing targets. */
int duel_test_can_cast(DuelTestRef source);
int duel_test_cast(DuelTestRef source);
void duel_test_resolve(void);
void duel_test_cleanup(void);
void duel_test_remove_from_combat(DuelTestRef ref);
void duel_test_destroy(DuelTestRef ref);
int duel_test_attack(DuelTestRef ref);
int duel_test_block(DuelTestRef blocker, DuelTestRef attacker);
/* Complete blocker declaration and enter the after-blocking response window. */
void duel_test_advance_combat(void);
void duel_test_combat_damage(void);
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
