#include "duel_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DuelHost host;
static DuelHostState state;
void duel_host_install(const DuelHost *value, uint32_t seed, uint32_t steps)
{
  host = *value;
  state.random_state = seed ? seed : 1;
  state.ticks = 0;
  state.remaining_steps = steps;
  state.ai_choices = 0;
}
void duel_host_uninstall(void) { memset(&host, 0, sizeof(host)); }
int duel_host_is_headless(void) { return host.choose != NULL; }
int duel_host_scripted_input(void) { return duel_host_is_headless() && !state.ai_choices; }
void duel_host_set_ai_choices(int enabled) { state.ai_choices = !!enabled; }
void duel_host_fail(const char *message)
{
  fprintf(stderr, "DUEL_HARNESS_ERROR: %s\n", message);
  fflush(stderr);
  abort();
}
void duel_host_checkpoint(const char *operation)
{
  if (duel_host_is_headless() && state.remaining_steps-- == 0)
    duel_host_fail(operation);
}
int duel_host_choose(DuelChoiceKind kind, int player, const char *prompt,
                     int count, const int *cards, int *card)
{
  DuelChoiceRequest request = {kind, player, prompt, count, cards};
  DuelChoiceAnswer answer = {-1, -1};
  duel_host_checkpoint("execution budget exhausted during input");
  if (!host.choose || !host.choose(host.context, &request, &answer))
  {
    char message[512];
    snprintf(message, sizeof(message), "unscripted choice kind=%d player=%d prompt=%s",
             kind, player, prompt ? prompt : "");
    duel_host_fail(message);
  }
  if (card) *card = answer.card;
  return answer.value;
}
int duel_host_random(void)
{
  uint32_t x;
  if (!duel_host_is_headless()) return rand();
  duel_host_checkpoint("execution budget exhausted during RNG");
  x = state.random_state;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  if (getenv("DUEL_TEST_TRACE")) fprintf(stderr, "RNG before=%u after=%u\n", state.random_state, x);
  state.random_state = x;
  return (int)(x & 0x7fffffff);
}
uint32_t duel_host_clock(void) { duel_host_checkpoint("execution budget exhausted during clock polling"); return ++state.ticks; }
DuelHostState duel_host_capture(void) { return state; }
void duel_host_restore(DuelHostState value) { state = value; }
