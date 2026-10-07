#include "duel_test_api.h"
#include "magic/src/game_support.h"
#include "magic/src/global_state.h"
#include "magic/src/global_other.h"
#include "magic/src/global_strings.h"
#include "magic/src/carddata_generated.h"
#include "deckdll/src/card_db.h"
#include "platform/asset_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern size_t duel_engine_ai_snapshot_size(void), duel_manalink_snapshot_size(void);
extern void duel_engine_capture_ai_snapshot(void *), duel_engine_restore_ai_snapshot(const void *);
extern void duel_manalink_snapshot_capture(void *), duel_manalink_snapshot_restore(const void *);
extern int manalinkeh_get_power(int, int), manalinkeh_get_toughness(int, int);
extern int manalinkeh_put_into_play(int, int), manalinkeh_in_hand(int, int);
extern card_instance_t *manalinkeh_in_play(int, int);
extern int manalinkeh_dispatch_event_to_single_card(int, int, event_t, int, int);
extern void manalinkeh_dispatch_event_raw(event_t);
extern int manalinkeh_put_card_on_stack3(int, int);
extern int manalinkeh_finalize_activation(int, int);
extern int activate(int, int, int), put_card_on_stack(int, int, int), resolve_top_card_on_stack(void);
extern void reset_duel_globals(void), cleanup_phase(unsigned int);
extern int reset_timestamp_players(void);
extern int end_turn_phase(unsigned int);
extern void shandalar_initialize_modded_card_database(void);
extern void InitializeDuelUiGlobalIds(void);
extern void save_ai_search_state(void), restore_ai_search_state(void);
extern void manalinkeh_backup_data_for_ai_0(void), manalinkeh_restore_data_for_ai_0(void);
extern int choose_ai_main_phase_action(int), can_attack(int, int);
extern void run_duel_turn(unsigned int);
extern int assign_blocker_to_attacker(int, int, int, int);
extern void resolve_combat_damage(int);
extern int turn_count, stack_phase[32];
extern int unknown62BCEC, tapped_for_mana_color;
extern void duel_rules_initialize_headless(void), manalinkeh_recalculate_rules_engine_and_deadbox(void);
extern void manalinkeh_recalculate_all_cards_in_play(void);
extern int manalinkeh_raw_put_iid_on_top_of_graveyard(int, int);
extern int manalinkeh_add_card_to_hand(int, int);
extern int get_card_response_action_type(int, int);
extern char global_base_directory[0x105];

typedef struct { void *address; size_t size; } Global;
#define DUEL_GLOBAL(name) { &name, sizeof(name) },
static Global globals[] = {
#include "duel_snapshot_globals.inc"
};
#undef DUEL_GLOBAL

typedef struct { DuelChoiceKind kind; int player, value, card; } Decision;
static Decision decisions[128];
static int decision_count, decision_cursor;
struct DuelTestSnapshot {
  DuelHostState host;
  int decision_count, decision_cursor;
  Decision decisions[128];
  unsigned char data[];
};
static DuelTestSnapshot *baseline;
static int initialized;
static DWORD headless_clock(void) { return duel_host_clock(); }
static void headless_sleep(DWORD milliseconds)
{
  duel_host_checkpoint("execution budget exhausted during delay");
  DuelHostState s = duel_host_capture(); s.ticks += milliseconds; duel_host_restore(s);
}
void duel_test_probe_presentation(void)
{
  /* Exercise the exact entry point used by drawcardlib's missing-file popup. */
  MessageBoxA(NULL, "headless presentation probe", "drawcardlib.dll", MB_ICONERROR);
  duel_host_fail("presentation guard returned unexpectedly");
}
static size_t global_size(void) { size_t s = 0, i; for (i = 0; i < sizeof(globals)/sizeof(*globals); ++i) s += globals[i].size; return s; }
DuelTestSnapshot *duel_test_capture(void)
{
  size_t core = duel_engine_ai_snapshot_size(), supplemental = duel_manalink_snapshot_size(), i;
  DuelTestSnapshot *s = malloc(sizeof(*s) + core + supplemental + global_size());
  unsigned char *cursor;
  if (!s) duel_host_fail("snapshot allocation failed");
  s->host = duel_host_capture(); s->decision_count = decision_count; s->decision_cursor = decision_cursor;
  memcpy(s->decisions, decisions, sizeof(decisions));
  duel_engine_capture_ai_snapshot(s->data);
  duel_manalink_snapshot_capture(s->data + core);
  cursor = s->data + core + supplemental;
  for (i = 0; i < sizeof(globals)/sizeof(*globals); ++i) { memcpy(cursor, globals[i].address, globals[i].size); cursor += globals[i].size; }
  return s;
}
void duel_test_restore(const DuelTestSnapshot *s)
{
  size_t core = duel_engine_ai_snapshot_size(), supplemental = duel_manalink_snapshot_size(), i;
  const unsigned char *cursor = s->data + core + supplemental;
  for (i = 0; i < sizeof(globals)/sizeof(*globals); ++i) { memcpy(globals[i].address, cursor, globals[i].size); cursor += globals[i].size; }
  duel_engine_restore_ai_snapshot(s->data);
  duel_manalink_snapshot_restore(s->data + core);
  duel_host_restore(s->host); decision_count = s->decision_count; decision_cursor = s->decision_cursor;
  memcpy(decisions, s->decisions, sizeof(decisions));
}
void duel_test_free_snapshot(DuelTestSnapshot *s) { free(s); }
static int choose(void *context, const DuelChoiceRequest *request, DuelChoiceAnswer *answer)
{
  (void)context;
  if (getenv("DUEL_TEST_TRACE")) fprintf(stderr,"CHOICE kind=%d player=%d prompt=%s\n",request->kind,request->player,request->prompt?request->prompt:"");
  /* Explicit harness policy: pass priority unless the script requests a response. */
  if (request->kind == DUEL_CHOICE_PRIORITY &&
      (decision_cursor == decision_count || decisions[decision_cursor].kind != DUEL_CHOICE_PRIORITY))
  { answer->value = -2; answer->card = -1; return 1; }
  if (decision_cursor >= decision_count) return 0;
  Decision *d = &decisions[decision_cursor];
  if (d->kind != request->kind || (d->player != -1 && d->player != request->player)) return 0;
  if (request->kind == DUEL_CHOICE_COLOR &&
      (d->value < 0 || d->value > 6 || !(request->count & (1u << d->value))))
    duel_host_fail("illegal scripted mana color");
  ++decision_cursor; answer->value = d->value; answer->card = d->card;
  if (request->kind == DUEL_CHOICE_PRIORITY && d->value == -3) {
    int found = -1;
    for (int c = 0; c < g_active_cards_count[request->player]; ++c) {
      card_instance_t *i = &global_card_instances[request->player][c];
      if (i->internal_card_id >= 0 && i->display_pic_csv_id == d->card &&
          (global_cards_data[i->internal_card_id].type & TYPE_EFFECT) &&
          get_card_response_action_type(request->player,c)) {
        if (found != -1) duel_host_fail("ambiguous scripted trigger");
        found = c;
      }
    }
    if (found == -1) duel_host_fail("scripted trigger is not available");
    answer->value = found;
  }
  return 1;
}
int duel_test_initialize(const char *assets)
{
  char path[1024]; int p, c;
  if (initialized) return 1;
  shandalar_set_presentation_guard(duel_host_fail);
  shandalar_set_time_hooks(headless_clock, headless_sleep);
  shandalar_assets_set_root(assets);
  shandalar_assets_set_fetch_enabled(0);
  snprintf(path, sizeof(path), "%s/cards.dat", assets);
  if (!read_db_guts(path)) return 0;
  InitializeDuelUiGlobalIds();
  duel_rules_initialize_headless();
  shandalar_initialize_modded_card_database();
  snprintf(global_base_directory, sizeof(global_base_directory), "%s", assets);
  strcpy(global_ui_strings_filename, "uistrings.txt");
  load_global_ui_strings(global_ui_strings_filename);
  InitializeCriticalSection(&g_duel_render_lock);
  for (p = 0; p < 2; ++p) for (c = 0; c < 151; ++c) {
    memset(&global_card_instances[p][c], 0, sizeof(card_instance_t));
    global_card_instances[p][c].internal_card_id = -1;
    global_card_instances[p][c].original_internal_card_id = -1;
  }
  memset(global_library, -1, sizeof(global_library));
  memset(global_graveyard_slots, -1, sizeof(global_graveyard_slots));
  memset(global_exile, -1, sizeof(global_exile));
  reset_timestamp_players(); reset_duel_globals();
  g_duel_active = 1;
  g_current_turn = g_current_player = 0; g_current_phase = PHASE_MAIN1;
  g_life[0] = g_life[1] = 20; g_current_spell_player = g_current_spell_card = -1;
  g_trigger_condition = -1; g_affected_card = g_affected_card_controller = -1;
  g_stack_size = 0; g_duel_ai_mode_state = 0; g_ai_speculation_depth = 0;
  baseline = duel_test_capture(); initialized = 1;
  return 1;
}
void duel_test_reset(uint32_t seed)
{
  DuelHost host = {NULL, choose};
  if (!initialized) duel_host_fail("engine not initialized");
  duel_test_restore(baseline); decision_count = decision_cursor = 0;
  duel_host_install(&host, seed, 200000);
}
static int iid(int csvid) { int i; for (i = 0; i < g_card_count; ++i) if ((int)global_cards_data[i].id == csvid) return i; duel_host_fail("fixture card not registered"); return -1; }
DuelTestRef duel_test_add(int player, int zone, int csvid, int state)
{
  DuelTestRef ref = {player, -1}; int id = iid(csvid);
  if (zone >= DUEL_ZONE_LIBRARY) {
    if (zone == DUEL_ZONE_GRAVEYARD) { ref.card = manalinkeh_raw_put_iid_on_top_of_graveyard(player, id); return ref; }
    int *z = zone == DUEL_ZONE_LIBRARY ? global_library[player] : zone == DUEL_ZONE_GRAVEYARD ? global_graveyard_slots[player] : global_exile[player];
    int n = 0; while (n < 499 && z[n] != -1) ++n;
    if (n == 499) duel_host_fail("fixture zone full");
    z[n] = id; z[n+1] = -1; ref.card = n; return ref;
  }
  ref.card = manalinkeh_add_card_to_hand(player, id);
  if (zone == DUEL_ZONE_BATTLEFIELD) manalinkeh_put_into_play(player, ref.card);
  card_instance_t *inst = &global_card_instances[player][ref.card];
  inst->state &= ~STATE_SUMMONSICK_BOTH;
  if (state & DUEL_STATE_TAPPED) inst->state |= STATE_TAPPED;
  if (state & DUEL_STATE_UNTAPPED) inst->state &= ~STATE_TAPPED;
  if (state & DUEL_STATE_SICK) inst->state |= STATE_SUMMONSICK_BOTH;
  if (state & DUEL_STATE_ATTACKING) inst->state |= STATE_ATTACKING;
  return ref;
}
void duel_test_prepare_fixture(void) {
  manalinkeh_recalculate_rules_engine_and_deadbox();
  /* Normal gameplay refreshes cached keywords and mana colors before querying
     actions. Rule-engine/deadbox reconciliation alone does not do that. */
  manalinkeh_recalculate_all_cards_in_play();
  /* The normal mana-payment path restores this to -1 when it finishes. A
     fixture starts outside that path; zero falsely means payment is active. */
  unknown62BCEC = -1;
}
void duel_test_set_mana(int p, int color, int amount) {
  g_raw_mana_available[p][7] += amount - g_raw_mana_available[p][color];
  g_raw_mana_available[p][color] = amount;
}
void duel_test_set_life(int p, int amount) { g_life[p] = amount; }
void duel_test_set_turn(int p, int combat) { g_current_turn = g_current_player = p; g_current_phase = combat ? PHASE_DECLARE_BLOCKERS : PHASE_MAIN1; }
void duel_test_decision(DuelChoiceKind kind, int p, int value, int card)
{ if (decision_count == 128) duel_host_fail("too many scripted choices"); decisions[decision_count++] = (Decision){kind,p,value,card}; }
void duel_test_finish_decisions(void) { if (decision_cursor != decision_count) duel_host_fail("unused scripted choices"); }
int duel_test_can_activate(DuelTestRef s) { return manalinkeh_dispatch_event_to_single_card(s.player,s.card,EVENT_CAN_ACTIVATE,1-s.player,-1); }
int duel_test_activate(DuelTestRef s) {
  if (!duel_test_can_activate(s)) return 0;
  int result = activate(s.player,s.player,s.card);
  card_instance_t *instance = &global_card_instances[s.player][s.card];
  int iid = instance->internal_card_id >= 0 ? instance->internal_card_id : instance->original_internal_card_id;
  /* Gameplay finishes mana activations immediately. Keep ordinary activations
     pending so scripts can inspect costs and then resolve them explicitly. */
  if (result && iid >= 0 && (global_cards_data[iid].extra_ability & EA_MANA_SOURCE) && tapped_for_mana_color != -1)
    manalinkeh_finalize_activation(s.player,s.card);
  return result;
}
int duel_test_can_cast(DuelTestRef s) { return manalinkeh_dispatch_event_to_single_card(s.player,s.card,EVENT_CAN_CAST,1-s.player,-1); }
int duel_test_cast(DuelTestRef s) { if (!put_card_on_stack(s.player,s.card,0)) return 0; return put_card_on_stack(s.player,s.card,1); }
void duel_test_resolve(void) {
  int n=0;
  while(g_stack_size) {
    if (++n>64) duel_host_fail("stack did not drain");
    int index=g_stack_size-1;
    DuelTestRef r={global_stack_cards[index].player,global_stack_cards[index].card};
    if ((((unsigned)g_stack_data[index] >> 16) & 255) == EVENT_RESOLVE_SPELL)
      manalinkeh_put_card_on_stack3(r.player,r.card);
    else manalinkeh_finalize_activation(r.player,r.card);
  }
}
void duel_test_cleanup(void) { g_current_phase=PHASE_CLEANUP; cleanup_phase(g_current_turn); end_turn_phase(g_current_turn); }
void duel_test_remove_from_combat(DuelTestRef r) { global_card_instances[r.player][r.card].state &= ~STATE_ATTACKING; }
void duel_test_destroy(DuelTestRef r) {
  kill_card(r.player,r.card,KILL_DESTROY);
  /* Destruction may queue regeneration and graveyard processing. Finish the real
     death pipeline before the script starts its next action. */
  regenerate_or_graveyard_triggers();
}
int duel_test_attack(DuelTestRef r) {
  /* Match the separate no-attack sickness guard in the main phase and AI action
     scan. Manalink's can_attack alone checks the no-tap bit and can accept
     creatures that the game cannot declare as attackers. This fixture helper
     still does not implement the complete attack timing/cost sequence. */
  if (global_card_instances[r.player][r.card].state & STATE_SUMMONSICK_NOATTACK) return 0;
  if (!can_attack(r.player,r.card)) return 0;
  global_card_instances[r.player][r.card].state |= STATE_ATTACKING;
  /* The combat fixture can preload STATE_ATTACKING without completing a
     declaration. Preserve its explicit attack step and apply the tap obligation.
     A scripted declaration has the same tap obligation as a game declaration.
     Use the engine's vigilance query and tap operation, including tap events. */
  if (!has_vigilance(r.player,r.card)) tap_card_and_dispatch_event(r.player,r.card);
  manalinkeh_dispatch_event_to_single_card(r.player,r.card,EVENT_DECLARE_ATTACKERS,1-r.player,-1);
  return 1;
}
int duel_test_block(DuelTestRef b,DuelTestRef a) {
  if (!assign_blocker_to_attacker(b.player,b.card,a.player,a.card)) return 0;
  /* Match the declaration performed after this legality check by the game UI. */
  unsigned char parent = global_card_instances[a.player][a.card].blocking;
  global_card_instances[b.player][b.card].state |= STATE_BLOCKING;
  global_card_instances[b.player][b.card].blocking = (unsigned char)(parent == 0xff ? a.card : parent);
  return 1;
}
void duel_test_combat_damage(void) { resolve_combat_damage(g_current_turn); }
void duel_test_advance_turn(void) { duel_test_cleanup(); g_current_turn=1-g_current_turn; g_current_player=g_current_turn; g_current_phase=PHASE_MAIN1; ++turn_count; }
DuelTestCard duel_test_inspect(DuelTestRef r)
{
  card_instance_t *i=&global_card_instances[r.player][r.card];
  DuelTestCard v={0}; v.csvid=i->internal_card_id<0?-1:global_cards_data[i->internal_card_id].id;
  v.in_play=(i->internal_card_id>=0 && manalinkeh_in_play(r.player,r.card));
  v.in_hand=(i->internal_card_id>=0 && manalinkeh_in_hand(r.player,r.card));
  v.tapped=!!(i->state&STATE_TAPPED); v.sick=!!(i->state&STATE_SUMMONSICK_BOTH); v.attacking=!!(i->state&STATE_ATTACKING); v.damage=i->damage_on_card;
  if(i->internal_card_id>=0) { v.power=manalinkeh_get_power(r.player,r.card); v.toughness=manalinkeh_get_toughness(r.player,r.card); }
  return v;
}
int duel_test_zone_count(int p,int zone) {
  int n=0,c;
  if(zone<DUEL_ZONE_LIBRARY) { for(c=0;c<g_active_cards_count[p];++c) { card_instance_t *i=&global_card_instances[p][c]; if(i->internal_card_id>=0 && !(global_cards_data[i->internal_card_id].type & TYPE_EFFECT) && (!!(i->state&STATE_IN_PLAY)==(zone==DUEL_ZONE_BATTLEFIELD))) ++n; } }
  else { const int*z=zone==DUEL_ZONE_LIBRARY?global_library[p]:zone==DUEL_ZONE_GRAVEYARD?global_graveyard_slots[p]:global_exile[p]; while(n<500 && z[n]>=0)++n; }
  return n;
}
int duel_test_zone_csvid(int p,int zone,int index) {
  if(zone<DUEL_ZONE_LIBRARY) { int c,n=0; for(c=0;c<g_active_cards_count[p];++c) { card_instance_t*i=&global_card_instances[p][c]; if(i->internal_card_id>=0 && !(global_cards_data[i->internal_card_id].type & TYPE_EFFECT) && (!!(i->state&STATE_IN_PLAY)==(zone==DUEL_ZONE_BATTLEFIELD)) && n++==index) return global_cards_data[i->internal_card_id].id; } return -1; }
  int*z=zone==DUEL_ZONE_LIBRARY?global_library[p]:zone==DUEL_ZONE_GRAVEYARD?global_graveyard_slots[p]:global_exile[p]; return index<0 || index>=500 || z[index]<0 ? -1 : global_cards_data[z[index]].id;
}
int duel_test_life(int p) {return g_life[p];}
int duel_test_mana(int p,int color) {return g_raw_mana_available[p][color];}
int duel_test_stack_size(void) {return g_stack_size;}
void duel_test_dump(void) {
  fprintf(stderr,"STATE: phase=%d stack=%d turn=%d\n",g_current_phase,g_stack_size,g_current_turn);
  for(int p=0;p<2;++p) for(int c=0;c<g_active_cards_count[p];++c) {
    card_instance_t *i=&global_card_instances[p][c];
    if(i->internal_card_id>=0) fprintf(stderr,"  player=%d slot=%d %s csvid=%d state=%x power=%d toughness=%d targets=%d target0=(%d,%d)\n",p,c,global_cards_data[i->internal_card_id].name,global_cards_data[i->internal_card_id].id,i->state,i->power,i->toughness,i->number_of_targets,i->targets[0].player,i->targets[0].card);
  }
}
void duel_test_check_invariants(void) { int p,c; if(g_stack_size<0||g_stack_size>32) duel_host_fail("stack bounds"); for(p=0;p<2;++p) { if(g_active_cards_count[p]<0||g_active_cards_count[p]>151)duel_host_fail("active card bounds"); for(c=0;c<g_active_cards_count[p];++c) if(global_card_instances[p][c].internal_card_id < -1 || global_card_instances[p][c].internal_card_id>=g_card_count+16)duel_host_fail("invalid internal card id"); } }
void duel_test_ai_roundtrip(void) { manalinkeh_backup_data_for_ai_0(); manalinkeh_restore_data_for_ai_0(); }
int duel_test_ai_action(int p) { int old=g_duel_ai_mode_state; g_duel_ai_mode_state=1; int action=choose_ai_main_phase_action(p); g_duel_ai_mode_state=old; return action; }
void duel_test_ai_turns(int count) {
  int previous = g_duel_ai_mode_state;
  duel_host_set_ai_choices(1);
  for (int n = 0; n < count; ++n) {
    int player = n % 2;
    g_duel_ai_mode_state = 1;
    run_duel_turn(player);
    duel_test_check_invariants();
    if (g_life[0] <= 0 || g_life[1] <= 0) break;
  }
  g_duel_ai_mode_state = previous;
  duel_host_set_ai_choices(0);
}
