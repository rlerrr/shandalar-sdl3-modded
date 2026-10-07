#ifndef SHANDALAR_DUEL_HOST_H
#define SHANDALAR_DUEL_HOST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Input and presentation boundaries only; legality and game rules stay in the engine. */
typedef enum {
  DUEL_CHOICE_TARGET, DUEL_CHOICE_ZONE, DUEL_CHOICE_DIALOG,
  DUEL_CHOICE_NUMBER, DUEL_CHOICE_COLOR, DUEL_CHOICE_PRIORITY
} DuelChoiceKind;
typedef struct {
  DuelChoiceKind kind;
  int player;
  const char *prompt;
  int count;
  const int *cards;
} DuelChoiceRequest;
typedef struct { int value, card; } DuelChoiceAnswer;
typedef struct {
  void *context;
  int (*choose)(void *, const DuelChoiceRequest *, DuelChoiceAnswer *);
} DuelHost;
typedef struct {
  uint32_t random_state, ticks, remaining_steps;
  int ai_choices;
} DuelHostState;

void duel_host_install(const DuelHost *host, uint32_t seed, uint32_t step_budget);
void duel_host_uninstall(void);
int duel_host_is_headless(void);
int duel_host_scripted_input(void);
void duel_host_set_ai_choices(int enabled);
int duel_host_choose(DuelChoiceKind kind, int player, const char *prompt,
                     int count, const int *cards, int *card);
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
