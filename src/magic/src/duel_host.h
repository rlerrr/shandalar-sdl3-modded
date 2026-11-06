#ifndef SHANDALAR_DUEL_HOST_H
#define SHANDALAR_DUEL_HOST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Input and presentation boundaries only; legality and game rules stay in the engine. */
typedef enum {
  DUEL_CHOICE_TARGET, DUEL_CHOICE_ZONE, DUEL_CHOICE_DIALOG,
  DUEL_CHOICE_NUMBER, DUEL_CHOICE_COLOR, DUEL_CHOICE_PRIORITY, DUEL_CHOICE_MANA,
  DUEL_CHOICE_X
} DuelChoiceKind;
typedef struct {
  DuelChoiceKind kind;
  int player;
  const char *prompt;
  int count;
  const int *cards;
  int zone_must_select; /* -1 when unspecified; input policy, not a rules override. */
  const int *zone_legal; /* Read-only engine predicate results; NULL if unavailable. */
} DuelChoiceRequest;
typedef struct { int value, card; } DuelChoiceAnswer;
typedef struct {
  void *context;
  int (*choose)(void *, const DuelChoiceRequest *, DuelChoiceAnswer *);
  int (*choice_ready)(void *, DuelChoiceKind, int player);
} DuelHost;
typedef struct {
  uint32_t random_state, ticks, remaining_steps;
  int ai_choices;
  int observe_zone_legality;
  int present_ai_dialogs;
  int target_probe_player, target_probe_card, target_probe_seen, target_probe_result;
  uint32_t shuffle_count[2];
} DuelHostState;

void duel_host_install(const DuelHost *host, uint32_t seed, uint32_t step_budget);
void duel_host_uninstall(void);
int duel_host_is_headless(void);
/* Passive receipts; simulation snapshots roll these back with the libraries. */
void duel_host_record_shuffle(int player);
int duel_host_scripted_input(void);
/* Opt-in inspection of the next planned input, without consuming it. */
int duel_host_choice_ready(DuelChoiceKind kind, int player);
int duel_host_choose_x(int player, int maximum);
void duel_host_set_ai_choices(int enabled);
void duel_host_present_ai_dialogs(int enabled);
int duel_host_presenting_ai_dialogs(void);
/* A negative player disables this isolated target-definition observation. */
void duel_host_probe_target(int player, int card);
int duel_host_target_probe_candidate(int *player, int *card);
void duel_host_observe_target_legal(int legal);
int duel_host_target_probe_result(void);
void duel_host_observe_zone_legality(int enabled);
int duel_host_observing_zone_legality(void);
int duel_host_choose(DuelChoiceKind kind, int player, const char *prompt,
                     int count, const int *cards, int *card);
int duel_host_choose_zone(int player, const char *prompt, int count,
                          const int *cards, int must_select, const int *legal);
int duel_host_random(void);
uint32_t duel_host_clock(void);
void duel_host_checkpoint(const char *operation);
void duel_host_fail(const char *message);
DuelHostState duel_host_capture(void);
void duel_host_restore(DuelHostState state);
#ifdef __cplusplus
}
#endif
#endif
