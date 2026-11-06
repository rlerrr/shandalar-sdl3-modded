#include "duel_host.h"
#include "duel_engine.h"
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
  state.observe_zone_legality = 0;
  state.present_ai_dialogs = 0;
  state.target_probe_player = state.target_probe_card = -1;
  state.target_probe_seen = 0; state.target_probe_result = -1;
  state.shuffle_count[0] = state.shuffle_count[1] = 0;
}
void duel_host_uninstall(void) { memset(&host, 0, sizeof(host)); }
int duel_host_is_headless(void) { return host.choose != NULL; }
void duel_host_record_shuffle(int player) {
  if (duel_host_is_headless() && player >= 0 && player < 2)
    ++state.shuffle_count[player];
}
int duel_host_scripted_input(void) {
  /* Nested damage-prevention searches replay engine snapshots. Their trial
     choices belong to the AI, not the caller's one-shot script. */
  return duel_host_is_headless() && !state.ai_choices && g_duel_ai_mode_state != 1;
}
int duel_host_choice_ready(DuelChoiceKind kind, int player) {
  return duel_host_scripted_input() && host.choice_ready && host.choice_ready(host.context,kind,player);
}
int duel_host_choose_x(int player, int maximum) {
  int value=duel_host_choose(DUEL_CHOICE_X,player,"Choose X",maximum,NULL,NULL);
  if (value<0 || value>200000 || (maximum>=0 && value>maximum)) {
    char message[160];snprintf(message,sizeof(message),"illegal scripted X choice: player=%d value=%d maximum=%d",player,value,maximum);duel_host_fail(message);
  }
  return value;
}
void duel_host_set_ai_choices(int enabled) { state.ai_choices = !!enabled; }
void duel_host_present_ai_dialogs(int enabled) { state.present_ai_dialogs = !!enabled; }
int duel_host_presenting_ai_dialogs(void) { return duel_host_scripted_input() && state.present_ai_dialogs; }
void duel_host_probe_target(int player, int card) {
  state.target_probe_player=player; state.target_probe_card=card;
  state.target_probe_seen=0; state.target_probe_result=-1;
}
int duel_host_target_probe_candidate(int *player, int *card) {
  if (!duel_host_scripted_input() || state.target_probe_player<0) return 0;
  *player=state.target_probe_player; *card=state.target_probe_card; return 1;
}
void duel_host_observe_target_legal(int legal) {
  int player,card;
  if (!duel_host_target_probe_candidate(&player,&card)) return;
  if (!state.target_probe_seen) state.target_probe_result=!!legal;
  else if (state.target_probe_result!=!!legal) state.target_probe_result=-2;
  ++state.target_probe_seen;
}
int duel_host_target_probe_result(void) { return state.target_probe_result; }
void duel_host_observe_zone_legality(int enabled) { state.observe_zone_legality = !!enabled; }
int duel_host_observing_zone_legality(void) { return duel_host_scripted_input() && state.observe_zone_legality; }
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
static int choose_with_policy(DuelChoiceKind kind, int player, const char *prompt,
                     int count, const int *cards, int *card, int must_select, const int *legal)
{
  DuelChoiceRequest request = {kind, player, prompt, count, cards, must_select, legal};
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
int duel_host_choose(DuelChoiceKind kind, int player, const char *prompt,
                     int count, const int *cards, int *card)
{
  return choose_with_policy(kind,player,prompt,count,cards,card,-1,NULL);
}
int duel_host_choose_zone(int player, const char *prompt, int count,
                          const int *cards, int must_select, const int *legal)
{
  return choose_with_policy(DUEL_CHOICE_ZONE,player,prompt,count,cards,NULL,!!must_select,legal);
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
