#include "duel_test_api.h"
#include "magic/src/game_support.h"
#include "magic/src/global_state.h"
#include "magic/src/global_other.h"
#include "magic/src/global_strings.h"
#include "magic/src/global_duel_ui_ids.h"
#include "magic/src/carddata_generated.h"
#include "deckdll/src/card_db.h"
#include "platform/asset_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "duel_handler_bindings.inc"
#include "manalinkeh/subtypes.h"
extern int manalinkeh_get_color(int, int);
extern unsigned int manalinkeh_get_type(int, int);
extern int manalinkeh_has_subtype(int, int, subtype_t);
extern int manalinkeh_count_player_counters(int player, int type);
extern int manalinkeh_get_unlock_code(int csvid);
extern int unlocked[50];
extern int manalinkeh_count_counters(int player, int card, int type);
int duel_test_poison(int player) { return manalinkeh_count_player_counters(player, 0); /* PCOUNTER_POISON */ }
int duel_test_energy(int player) { return manalinkeh_count_player_counters(player, 2); /* PCOUNTER_ENERGY */ }

extern size_t duel_engine_ai_snapshot_size(void), duel_manalink_snapshot_size(void);
extern void duel_engine_capture_ai_snapshot(void *), duel_engine_restore_ai_snapshot(const void *);
extern void duel_manalink_snapshot_capture(void *), duel_manalink_snapshot_restore(const void *);
extern int manalinkeh_get_power(int, int), manalinkeh_get_toughness(int, int);
extern int manalinkeh_put_into_play(int, int), manalinkeh_in_hand(int, int);
extern void manalinkeh_count_colors_of_lands_in_play(void);
extern card_instance_t *manalinkeh_in_play(int, int);
extern int manalinkeh_dispatch_event_to_single_card(int, int, event_t, int, int);
extern void manalinkeh_dispatch_event_raw(event_t);
extern int manalinkeh_put_card_on_stack3(int, int);
extern int manalinkeh_finalize_activation(int, int);
extern int activate(int, int, int), put_card_on_stack(int, int, int), resolve_top_card_on_stack(void);
extern void reset_duel_globals(void), cleanup_phase(unsigned int);
extern int reset_timestamp_players(void);
extern int end_turn_phase(unsigned int);
extern int TENTATIVE_start_turn(int);
extern void shandalar_initialize_modded_card_database(void);
extern void InitializeDuelUiGlobalIds(void);
extern void save_ai_search_state(void), restore_ai_search_state(void);
extern void manalinkeh_backup_data_for_ai_0(void), manalinkeh_restore_data_for_ai_0(void);
extern int choose_ai_main_phase_action(int), can_attack(int, int);
extern void run_duel_turn(unsigned int);
extern int assign_blocker_to_attacker(int, int, int, int);
extern void resolve_combat_damage(int);
extern int cleanup_combat_state(int);
extern void mark_blocked_attackers(int);
extern int manalinkeh_count_creatures_this_is_blocking(int, int), manalinkeh_is_unblocked(int, int);
extern int manalinkeh_card_multiblocker_hook(int, int, event_t);
extern void manalinkeh_finish_blocker_selection(void);
extern int manalinkeh_dispatch_trigger2(int, trigger_t, const char *, int, int, int);
extern int turn_count, stack_phase[32];
extern int unknown62BCEC, tapped_for_mana_color;
extern void duel_rules_initialize_headless(void), manalinkeh_recalculate_rules_engine_and_deadbox(void);
extern void manalinkeh_recalculate_all_cards_in_play(void);
extern void manalinkeh_remove_from_combat(int player, int card);
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

typedef struct { DuelChoiceKind kind; int player, value, card, damage_recipient, regeneration_window; } Decision;
static Decision decisions[128];
static int decision_count, decision_cursor;
static int offered_count, offered_csvids[512], offered_legal[512], offered_selection_required;
static int forced_zone_choices;
static int discard_policy, discard_choice_count;
static int untap_prepared_player = -1, untap_prepared_turn = -1;
/* Transient input binding for the one real RulesEngine casting menu. */
static const char *flashback_menu_label;
static int flashback_menu_player, flashback_menu_occurrence, flashback_menu_seen;
struct DuelTestSnapshot {
  DuelHostState host;
  int decision_count, decision_cursor;
  Decision decisions[128];
  int offered_count, offered_csvids[512], offered_legal[512], offered_selection_required;
  int forced_zone_choices;
  int discard_policy, discard_choice_count;
  int untap_prepared_player, untap_prepared_turn;
  int unlocked[50];
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
  s->offered_count = offered_count;
  s->offered_selection_required = offered_selection_required;
  s->forced_zone_choices = forced_zone_choices;
  s->discard_policy = discard_policy; s->discard_choice_count = discard_choice_count;
  s->untap_prepared_player = untap_prepared_player; s->untap_prepared_turn = untap_prepared_turn;
  memcpy(s->unlocked, unlocked, sizeof(s->unlocked));
  memcpy(s->offered_csvids, offered_csvids, sizeof(offered_csvids));
  memcpy(s->offered_legal, offered_legal, sizeof(offered_legal));
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
  offered_count = s->offered_count;
  offered_selection_required = s->offered_selection_required;
  forced_zone_choices = s->forced_zone_choices;
  discard_policy = s->discard_policy; discard_choice_count = s->discard_choice_count;
  untap_prepared_player = s->untap_prepared_player; untap_prepared_turn = s->untap_prepared_turn;
  memcpy(unlocked, s->unlocked, sizeof(s->unlocked));
  memcpy(offered_csvids, s->offered_csvids, sizeof(offered_csvids));
  memcpy(offered_legal, s->offered_legal, sizeof(offered_legal));
}
void duel_test_free_snapshot(DuelTestSnapshot *s) { free(s); }
static int choice_ready(void *context, DuelChoiceKind kind, int player)
{
  (void)context;
  if (flashback_menu_label && kind==DUEL_CHOICE_DIALOG && player==flashback_menu_player) return 1;
  return decision_cursor<decision_count && decisions[decision_cursor].kind==kind &&
    (decisions[decision_cursor].player==-1 || decisions[decision_cursor].player==player);
}
static int choose(void *context, const DuelChoiceRequest *request, DuelChoiceAnswer *answer)
{
  (void)context;
  if (flashback_menu_label && request->kind == DUEL_CHOICE_DIALOG) {
    if (request->player != flashback_menu_player || !request->prompt)
      duel_host_fail("unexpected dialog before Flashback menu");
    const char *line=request->prompt;
    int index=0, occurrence=0, selected=-1;
    size_t wanted=strlen(flashback_menu_label);
    while (*line) {
      const char *end=strchr(line,'\n');
      if (!end) end=line+strlen(line);
      const char *text=line;
      while (text<end && (*text==' ' || *text=='\t')) ++text;
      size_t length=(size_t)(end-text);
      if (length && text[length-1]=='\r') --length;
      if (length==wanted && !memcmp(text,flashback_menu_label,wanted) &&
          occurrence++==flashback_menu_occurrence) selected=index;
      ++index;
      line=*end ? end+1 : end;
    }
    if (selected<0) duel_host_fail("requested Flashback source is not offered");
    flashback_menu_label=NULL; flashback_menu_seen=1;
    answer->value=selected; answer->card=-1;
    return 1;
  }
  /* Mana output and a public recovery reveal have OK-only notices. These exact
     notices carry no gameplay choice; other dialogs remain scripted. */
  if (request->kind == DUEL_CHOICE_DIALOG && request->player == 1 && request->prompt) {
    static const char *notices[] = {"to produce black mana.", "to produce blue mana.",
      "to produce green mana.", "to produce red mana.", "to produce white mana.", "to produce colorless mana.", "reveals:",
      "at random to discard.", "to discard."};
    for (unsigned i=0; i<sizeof(notices)/sizeof(*notices); ++i) {
      if (!strcmp(request->prompt,notices[i]) &&
          (decision_cursor>=decision_count || decisions[decision_cursor].kind!=DUEL_CHOICE_DIALOG ||
           (decisions[decision_cursor].player!=-1 && decisions[decision_cursor].player!=request->player))) {
        answer->value=0; answer->card=-1; return 1;
      }
    }
  }
  /* A one-option color prompt has no branching decision. Select its sole
     legal value unless the test supplied an explicit color choice. */
  if (request->kind == DUEL_CHOICE_COLOR && request->count>0 &&
      !(request->count & (request->count-1)) &&
      (decision_cursor>=decision_count || decisions[decision_cursor].kind!=DUEL_CHOICE_COLOR ||
       (decisions[decision_cursor].player!=-1 && decisions[decision_cursor].player!=request->player))) {
    int color=0;
    while (color<7 && !(request->count & (1u<<color))) ++color;
    if (color<7) { answer->value=color; answer->card=-1; return 1; }
  }
  if (request->kind == DUEL_CHOICE_ZONE) {
    if (request->count < 0 || request->count > 512 || (request->count && !request->cards))
      duel_host_fail("invalid offered zone cards");
    offered_count = 0;
    offered_selection_required = request->zone_must_select;
    for (int c = 0; c < request->count && request->cards[c] != -1; ++c) {
      int id = request->cards[c];
      if (id < 0 || id >= g_card_count + 16) duel_host_fail("invalid offered card id");
      offered_legal[offered_count] = request->zone_legal ? !!request->zone_legal[c] : -1;
      offered_csvids[offered_count++] = global_cards_data[id].id;
    }
  }
  if (getenv("DUEL_TEST_TRACE")) fprintf(stderr,"CHOICE kind=%d player=%d ai_mode=%d ai_depth=%d trigger=%d affected=%d/%d prompt=%s\n",request->kind,request->player,g_duel_ai_mode_state,g_ai_speculation_depth,g_trigger_condition,g_affected_card_controller,g_affected_card,request->prompt?request->prompt:"");
  if (request->kind == DUEL_CHOICE_ZONE && discard_policy && request->prompt &&
      (!strcmp(request->prompt,"Select a card to discard.") || !strcmp(request->prompt,"Select card to discard."))) {
    int chosen=-1;
    for (int c=0;c<offered_count;++c) if (!request->zone_legal || request->zone_legal[c])
      if (chosen<0 || (discard_policy==1 ? offered_csvids[c]<offered_csvids[chosen] : offered_csvids[c]>offered_csvids[chosen])) chosen=c;
    if (chosen<0) duel_host_fail("discard policy has no legal offered card");
    ++discard_choice_count; answer->value=chosen; answer->card=-1; return 1;
  }
  /* Explicit policy: pass priority unless the script requests this player's
     response. Other players' passes must not consume that planned response. */
  if (request->kind == DUEL_CHOICE_PRIORITY &&
      (decision_cursor == decision_count || decisions[decision_cursor].kind != DUEL_CHOICE_PRIORITY ||
       (decisions[decision_cursor].player != -1 && decisions[decision_cursor].player != request->player) ||
       (decisions[decision_cursor].regeneration_window && !(g_land_can_be_played & LCBP_REGENERATION))))
  { answer->value = -2; answer->card = -1; return 1; }
  if (decision_cursor >= decision_count) return 0;
  Decision *d = &decisions[decision_cursor];
  if (d->kind != request->kind || (d->player != -1 && d->player != request->player)) return 0;
  if (request->kind == DUEL_CHOICE_PRIORITY && d->regeneration_window) {
    /* A destruction window can still exclude this particular creature (for
       example, "can't be regenerated"). Use the actual offered responses,
       retaining the attempted regeneration so the case can assert it unused. */
    int legal = 0;
    if (manalinkeh_in_play(request->player, d->value)) {
      if (request->cards) {
        for (int c=0;c<request->count;++c) if (request->cards[c]==d->value) { legal=1; break; }
      } else legal = !!get_card_response_action_type(request->player, d->value);
    }
    if (!legal) { answer->value=-2; answer->card=-1; return 1; }
  }
  if (request->kind == DUEL_CHOICE_COLOR &&
      (d->value < 0 || d->value > 6 || !(request->count & (1u << d->value))))
    duel_host_fail("illegal scripted mana color");
  if (request->kind == DUEL_CHOICE_PRIORITY && d->value == -3) {
    /* A planned trigger may follow earlier spell-response windows. Pass until
       its effect is offered; an absent trigger remains queued and fails the
       test's consumed-choice check rather than selecting an unrelated action. */
    int found = -1;
    for (int c = 0; (int)g_trigger_condition != -1 && c < g_active_cards_count[request->player]; ++c) {
      card_instance_t *i = &global_card_instances[request->player][c];
      if (i->internal_card_id >= 0 &&
          ((int)global_cards_data[i->internal_card_id].id == d->card ||
           ((global_cards_data[i->internal_card_id].type & TYPE_EFFECT) && i->display_pic_csv_id == d->card)) &&
          get_card_response_action_type(request->player,c)) {
        if (found != -1) duel_host_fail("ambiguous scripted trigger");
        found = c;
      }
    }
    if (found == -1) { answer->value = -2; answer->card = -1; return 1; }
    ++decision_cursor; answer->value = found; answer->card = -1; return 1;
  }
  ++decision_cursor; answer->value = d->value; answer->card = d->card;
  if (request->kind == DUEL_CHOICE_ZONE && d->value == -5) {
    if (request->zone_must_select < 0) duel_host_fail("optional zone decision lacks selection policy");
    answer->value = -1; answer->card = -1;
    if (request->zone_must_select) {
      /* Expose a forced optional action as a rules assertion. Select the
         scripted legal fallback so normal engine validation and resolution
         still run; never bypass the engine's must-select restriction. */
      ++forced_zone_choices;
      int found = -1;
      for (int c=0;c<offered_count;++c) if (offered_csvids[c]==d->card) { found=c; break; }
      if (request->zone_legal && (found == -1 || !request->zone_legal[found])) {
        /* An incorrect mandatory filter can reject the Oracle fallback.
           Preserve that predicate and select another actually legal card,
           or return no selection when the offered zone has no legal cards. */
        found = -1;
        for (int c=0;c<offered_count;++c) if (request->zone_legal[c]) { found=c; break; }
      } else if (found == -1) duel_host_fail("optional zone fallback is not offered");
      answer->value = found;
    }
  }
  if (request->kind == DUEL_CHOICE_DIALOG && d->value == -4) {
    /* Select the explicitly scripted mana vector from the real payment menu,
       without depending on option ordering or which alternatives are affordable. */
    const char *line = request->prompt;
    int index = 0, found = -1;
    while (line && *line) {
      const char *end = strchr(line,'\n');
      if (!end) end = line + strlen(line);
      const char *at = line;
      while (at < end && (*at == ' ' || *at == '\t')) ++at;
      if (end-at >= 4 && !strncmp(at,"Pay ",4)) {
        int counts[6] = {0}, valid = 1;
        at += 4;
        if (at < end && *at >= '0' && *at <= '9') {
          while (at < end && *at >= '0' && *at <= '9') {
            if (counts[0] <= 15) counts[0] = counts[0]*10 + *at - '0';
            else valid = 0;
            ++at;
          }
        }
        const char *colors = "BUGRW";
        for (; at < end; ++at) {
          const char *color = strchr(colors,*at);
          if (color) ++counts[1+color-colors];
          else if (*at != ' ' && *at != '\r' && *at != '\t') valid = 0;
        }
        unsigned packed = 0;
        for (int color=0;color<6;++color) {
          if (counts[color] > 15) valid = 0;
          packed |= (unsigned)counts[color] << (4*color);
        }
        if (valid && packed == (unsigned)d->card) {
          if (found != -1) duel_host_fail("ambiguous scripted payment");
          found = index;
        }
      }
      ++index;
      line = *end ? end+1 : end;
    }
    if (found == -1) duel_host_fail("scripted payment is not offered");
    answer->value = found; answer->card = -1;
  }
  if (request->kind == DUEL_CHOICE_ZONE && d->value == -3) {
    /* Choose a named card after a shuffle without prescribing RNG results.
       Duplicate copies deliberately select the first offered occurrence.
       The engine still validates the selected card against its tutor filter. */
    int found = -1;
    for (int c = 0; c < offered_count; ++c) {
      if (offered_csvids[c] == d->card) { found = c; break; }
    }
    /* In observation mode, an Oracle-legal planned choice may expose an
       incorrect engine predicate. Decline only when the engine permits it,
       so the case can assert the rejected eligibility/effect rather than abort.
       No candidate is admitted past the ordinary legality validator. */
    if (request->zone_legal && !request->zone_must_select &&
        (found == -1 || !request->zone_legal[found])) answer->value = -1;
    else {
      if (found == -1) duel_host_fail("scripted zone card is not offered");
      answer->value = found;
    }
    answer->card = -1;
  }
  if (d->damage_recipient) {
    int found_player = -1, found_card = -1;
    for (int p = 0; p < 2; ++p) {
      for (int c = 0; c < g_active_cards_count[p]; ++c) {
        card_instance_t *i = &global_card_instances[p][c];
        if (i->internal_card_id == g_damage_card_internal_card_id && i->info_slot > 0 &&
            i->damage_target_player == d->value && i->damage_target_card == d->card) {
          if (found_card != -1) duel_host_fail("ambiguous scripted damage recipient");
          found_player = p; found_card = c;
        }
      }
    }
    if (found_card == -1) duel_host_fail("scripted damage packet is not available");
    answer->value = found_player; answer->card = found_card;
  }
  return 1;
}
int duel_test_initialize(const char *assets)
{
  char path[1024]; int p, c;
  if (initialized) return 1;
  DuelHost host = {NULL, choose, choice_ready};
  duel_host_install(&host, 1, 200000);
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
  /* Both fixture libraries are explicit. The legacy -1 opponent mode draws
     random encounter cards instead of consuming global_library[1]. */
  g_selected_wizard_color = -1;
  g_opponent_initial_library_index = 1;
  baseline = duel_test_capture(); initialized = 1;
  return 1;
}
void duel_test_reset(uint32_t seed)
{
  DuelHost host = {NULL, choose, choice_ready};
  if (!initialized) duel_host_fail("engine not initialized");
  duel_test_restore(baseline); decision_count = decision_cursor = forced_zone_choices = 0;
  duel_host_install(&host, seed, 200000);
}
void duel_test_set_budget(uint32_t steps)
{
  DuelHostState state = duel_host_capture();
  if (steps < 200000 || steps > 2000000) duel_host_fail("invalid scripted work budget");
  state.remaining_steps = steps;
  duel_host_restore(state);
}
static int iid(int csvid) { int i; for (i = 0; i < g_card_count; ++i) if ((int)global_cards_data[i].id == csvid) return i; duel_host_fail("fixture card not registered"); return -1; }
int duel_test_card_available(int csvid)
{
  (void)iid(csvid);
  int level = manalinkeh_get_unlock_code(csvid);
  if (level < 0 || level >= 50) duel_host_fail("invalid challenge availability level");
  return level == 0 || unlocked[level] == 1;
}
void duel_test_set_card_available(int csvid, int available)
{
  (void)iid(csvid);
  int level = manalinkeh_get_unlock_code(csvid);
  if (level <= 0 || level >= 50 || (available != 0 && available != 1))
    duel_host_fail("invalid fixture card availability override");
  unlocked[level] = available;
}
DuelTestRef duel_test_add(int player, int zone, int csvid, int state)
{
  DuelTestRef ref = {player, -1}; int id = iid(csvid);
  /* Explicit fixture cards belong to the available test collection. Grant only
     their mapped challenge unlocks, without files or tournament-rule overrides.
     Caller-owned snapshots preserve this input environment across AI searches. */
  int level = manalinkeh_get_unlock_code(csvid);
  if (level < 0 || level >= 50) duel_host_fail("invalid fixture challenge level");
  if (level > 0) unlocked[level] = 1;
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
  inst->ext->host_summoning_sickness = 0;
  inst->state &= ~STATE_SUMMONSICK_BOTH;
  if (state & DUEL_STATE_TAPPED) inst->state |= STATE_TAPPED;
  if (state & DUEL_STATE_UNTAPPED) inst->state &= ~STATE_TAPPED;
  if (state & DUEL_STATE_SICK) { inst->ext->host_summoning_sickness = 1; inst->state |= STATE_SUMMONSICK_BOTH; }
  if (state & DUEL_STATE_ATTACKING) inst->state |= STATE_ATTACKING;
  /* STATE_OWNED_BY_OPPONENT records player one's ownership, independently
   * of controller.  Toggle the default ownership to construct stolen cards. */
  if (state & DUEL_STATE_OPPONENT_OWNED) inst->state ^= STATE_OWNED_BY_OPPONENT;
  if (state & DUEL_STATE_TOKEN) inst->token_status |= STATUS_TOKEN;
  /* Normal gameplay refreshes battlefield counts between actions. Ordered
     fixture placement must do the same before the next permanent's entry
     replacement effect queries the land-count cache. */
  if (zone == DUEL_ZONE_BATTLEFIELD) manalinkeh_count_colors_of_lands_in_play();
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
  /* No UI phase shortcut is pending in a freshly loaded fixture. Zero would
     mean "run to untap" and cause normal mana cancellation to be ignored. */
  g_stop_phase = g_stop_phase_player = -1;
}
void duel_test_refresh(void) { manalinkeh_recalculate_all_cards_in_play(); }
extern void manalinkeh_default_pump_ability_definition(int, int, pump_ability_t *, int, int, int, int);
extern int manalinkeh_pump_ability(int, int, int, int, pump_ability_t *);
DuelTestRef duel_test_suspend_haste(DuelTestRef ref) {
  pump_ability_t pump;
  manalinkeh_default_pump_ability_definition(ref.player, ref.card, &pump, 0, 0, 0, SP_KEYWORD_HASTE);
  pump.paue_flags = PAUE_END_IF_TARGET_CHANGES_CONTROL;
  DuelTestRef effect = {ref.player, manalinkeh_pump_ability(ref.player, ref.card, ref.player, ref.card, &pump)};
  return effect;
}
void duel_test_set_mana(int p, int color, int amount) {
  g_raw_mana_available[p][7] += amount - g_raw_mana_available[p][color];
  g_raw_mana_available[p][color] = amount;
}
void duel_test_set_life(int p, int amount) { g_life[p] = amount; }
void duel_test_set_turn(int p, int combat) {
  /* Selecting an arbitrary fixture phase is not a new untap step. */
  untap_prepared_player = -1;
  g_current_turn = g_current_player = p;
  g_current_phase = combat ? PHASE_DECLARE_BLOCKERS : PHASE_MAIN1;
  manalinkeh_recalculate_all_cards_in_play();
}
void duel_test_decision(DuelChoiceKind kind, int p, int value, int card)
{ if (decision_count == 128) duel_host_fail("too many scripted choices"); decisions[decision_count++] = (Decision){kind,p,value,card,0,0}; }
void duel_test_regeneration_decision(DuelTestRef source) {
  duel_test_decision(DUEL_CHOICE_PRIORITY, source.player, source.card, -1);
  /* Spell responses and damage prevention precede the legacy destruction
     window. Retain this planned activation through those earlier passes. */
  decisions[decision_count - 1].regeneration_window = 1;
}
void duel_test_damage_decision(int player, DuelTestRef recipient) {
  duel_test_decision(DUEL_CHOICE_TARGET, player, recipient.player, recipient.card);
  decisions[decision_count - 1].damage_recipient = 1;
}
int duel_test_offered_count(void) { return offered_count; }
int duel_test_offered_selection_required(void) { return offered_selection_required; }
int duel_test_offered_legal(int index) {
  if (index < 0 || index >= offered_count) duel_host_fail("offered legality index out of range");
  return offered_legal[index];
}
int duel_test_forced_zone_choices(void) { return forced_zone_choices; }
void duel_test_set_discard_policy(int policy) {
  if (policy<0 || policy>2) duel_host_fail("invalid scripted discard policy");
  discard_policy=policy; discard_choice_count=0;
}
int duel_test_discard_choice_count(void) { return discard_choice_count; }
int duel_test_offered_csvid(int index) {
  if (index < 0 || index >= offered_count) duel_host_fail("offered card index out of range");
  return offered_csvids[index];
}
void duel_test_finish_decisions(void) { if (decision_cursor != decision_count) duel_host_fail("unused scripted choices"); }
int duel_test_finish_target_plan(void) {
  int unused = decision_count - decision_cursor;
  for (int i = decision_cursor; i < decision_count; ++i)
    if (decisions[i].kind != DUEL_CHOICE_TARGET)
      duel_host_fail("non-target choice remaining in target plan");
  decision_cursor = decision_count;
  return unused;
}
int duel_test_finish_choice_plan(void) {
  int unused = 0;
  for (int i=decision_cursor;i<decision_count;++i) {
    /* An explicit optional decline also permits a skipped optional trigger.
       The test still asserts unchanged zones and no forced recovery. */
    if (!(decisions[i].kind==DUEL_CHOICE_ZONE && decisions[i].value==-5)) ++unused;
  }
  decision_cursor = decision_count;
  return unused;
}
int duel_test_can_activate(DuelTestRef s) { return manalinkeh_dispatch_event_to_single_card(s.player,s.card,EVENT_CAN_ACTIVATE,1-s.player,-1); }
int duel_test_activate(DuelTestRef s) {
  if (!duel_test_can_activate(s)) return 0;
  int mana_before[6];
  for (int color = 0; color < 6; ++color)
    mana_before[color] = g_raw_mana_available[s.player][color];
  int result = activate(s.player,s.player,s.card);
  card_instance_t *instance = &global_card_instances[s.player][s.card];
  int iid = instance->internal_card_id >= 0 ? instance->internal_card_id : instance->original_internal_card_id;
  /* Gameplay finishes mana activations immediately. Keep ordinary activations
     pending so scripts can inspect costs and then resolve them explicitly. */
  int produced_mana = 0;
  for (int color = 0; color < 6; ++color)
    produced_mana |= g_raw_mana_available[s.player][color] > mana_before[color];
  /* Some legacy non-tapping mana handlers update the pool directly without
     setting tapped_for_mana_color. A pool increase distinguishes these from
     a mana source's nonmana mode (for example, sacrificing Mind Stone to draw). */
  if (result && iid >= 0 && (global_cards_data[iid].extra_ability & EA_MANA_SOURCE) &&
      (tapped_for_mana_color != -1 || produced_mana))
    manalinkeh_finalize_activation(s.player,s.card);
  return result;
}
int duel_test_can_cast(DuelTestRef s) { return manalinkeh_dispatch_event_to_single_card(s.player,s.card,EVENT_CAN_CAST,1-s.player,-1); }
extern int manalinkeh_get_base_power_iid(int, int), manalinkeh_get_base_toughness_iid(int, int);
int duel_test_base_power(int player, int csvid) { return manalinkeh_get_base_power_iid(player, iid(csvid)); }
int duel_test_base_toughness(int player, int csvid) { return manalinkeh_get_base_toughness_iid(player, iid(csvid)); }
int duel_test_can_target_creature(DuelTestRef source, DuelTestRef target) {
  extern void manalinkeh_default_target_definition(int, int, target_definition_t *, int);
  extern int manalinkeh_validate_arbitrary_target(target_definition_t *, int, int);
  target_definition_t td;
  manalinkeh_default_target_definition(source.player, source.card, &td, TYPE_CREATURE);
  return manalinkeh_validate_arbitrary_target(&td, target.player, target.card);
}
int duel_test_probe_activation_target(DuelTestRef source, DuelTestRef target, int mode_choice) {
  DuelTestSnapshot *saved=duel_test_capture();
  decision_count=decision_cursor=0;
  duel_host_set_ai_choices(0); duel_host_present_ai_dialogs(1);
  duel_host_probe_target(target.player,target.card);
  duel_test_set_discard_policy(1);
  if (mode_choice>=0) duel_test_decision(DUEL_CHOICE_DIALOG,source.player,mode_choice,-1);
  int available=duel_test_can_activate(source);
  if (duel_host_target_probe_result()==-1 && available)
    duel_test_activate(source);
  int result=duel_host_target_probe_result();
  duel_test_restore(saved); duel_test_free_snapshot(saved);
  return result;
}
static DuelTestRef hand_ability_proxy(int player) {
  for (int card=0; card<g_active_cards_count[player]; ++card) {
    int iid=global_card_instances[player][card].internal_card_id;
    if (iid>=0 && global_cards_data[iid].id==CARD_ID_RULES_ENGINE && manalinkeh_in_play(player,card))
      return (DuelTestRef){player,card};
  }
  return (DuelTestRef){player,-1};
}
int duel_test_can_activate_hand(DuelTestRef s) {
  if (s.player<0 || s.player>1 || s.card<0 || s.card>=g_active_cards_count[s.player]) return 0;
  if (!manalinkeh_in_hand(s.player,s.card)) return 0;
  DuelTestRef proxy=hand_ability_proxy(s.player);
  return proxy.card>=0 &&
    manalinkeh_dispatch_event_to_single_card(s.player,s.card,EVENT_CAN_ACTIVATE_FROM_HAND,1-s.player,-1) &&
    duel_test_can_activate(proxy);
}
int duel_test_activate_hand(DuelTestRef s) {
  if (!duel_test_can_activate_hand(s)) return 0;
  return duel_test_activate(hand_ability_proxy(s.player));
}
extern int manalinkeh_call_card_function(int, int, event_t);
extern void manalinkeh_obliterate_card_and_recycle(int, int);
extern int manalinkeh_check_card_for_rules_engine(int);
extern void manalinkeh_update_rules_engine(int);
extern int grafdiggers_cages(int, type_t, tutor_from_t);
extern player_bits_t player_bits[2];
void duel_test_prepare_graveyard_abilities(void) {
  if (g_stack_size) duel_host_fail("prepare_graveyard_abilities requires an empty stack");
  /* Normal startup sees the source in a deck/hand. Direct graveyard fixtures
     never did, and the normal rescan intentionally only visits those zones. */
  int flags = 0;
  for (int player = 0; player < 2; ++player)
    for (int index = 0; index < duel_test_zone_count(player, DUEL_ZONE_GRAVEYARD); ++index)
      flags |= manalinkeh_check_card_for_rules_engine(global_graveyard_slots[player][index]);
  manalinkeh_update_rules_engine(flags);
}
static int graveyard_ability_iid(DuelTestRef source) {
  if (source.player < 0 || source.player > 1 || source.card < 0 ||
      source.card >= duel_test_zone_count(source.player, DUEL_ZONE_GRAVEYARD)) return -1;
  int id = global_graveyard_slots[source.player][source.card];
  if (id < 0 || id >= g_card_count) return -1;
  /* These categories enter the real RulesEngine graveyard menu. */
  int category = global_cards_data[id].cc[2];
  if (!(category == 9 || category == 10 || category == 17 || category == 21 ||
        category == 23 || category == 24 || category == 25 || category == 27 ||
        global_cards_data[id].id == CARD_ID_VISCERA_DRAGGER)) return -1;
  return id;
}
static int graveyard_ability_modes(DuelTestRef source) {
  int id = graveyard_ability_iid(source);
  if (getenv("DUEL_TEST_TRACE")) fprintf(stderr,"GY_PROBE player=%d pos=%d iid=%d flags=%x phase=%d turn=%d\n",source.player,source.card,id,source.player>=0&&source.player<2?player_bits[source.player]:0,g_current_phase,g_current_turn);
  if (id < 0 || (player_bits[source.player] & PB_GY_ABILITIES_DISABLED)) return 0;
  DuelTestRef proxy = hand_ability_proxy(source.player);
  if (getenv("DUEL_TEST_TRACE")) fprintf(stderr,"GY_PROBE proxy=%d,%d info=%x\n",proxy.player,proxy.card,proxy.card>=0?global_card_instances[proxy.player][proxy.card].info_slot:0);
  if (proxy.card < 0) return 0;
  DuelTestSnapshot* saved = duel_test_capture();
  int fake = manalinkeh_add_card_to_hand(source.player, id);
  if (fake < 0) duel_host_fail("graveyard ability probe allocation failed");
  global_card_instances[source.player][fake].state |= STATE_INVISIBLE;
  --g_duel_summary.hand_counts[source.player];
  g_land_can_be_played |= LCBP_ACTIVATING_FROM_HAND;
  int available = manalinkeh_call_card_function(source.player, fake, EVENT_GRAVEYARD_ABILITY);
  manalinkeh_obliterate_card_and_recycle(source.player, fake);
  int proxy_available = duel_test_can_activate(proxy);
  int modes = available & (GA_BASIC | GA_UNEARTH | GA_PLAYABLE_FROM_GRAVE);
  /* Match the real menu's creature-return restriction, even if another card
     makes the same shared RulesEngine proxy available. */
  if ((modes & GA_UNEARTH) &&
      (!(global_cards_data[id].type & TYPE_CREATURE) ||
       grafdiggers_cages(source.player, TYPE_CREATURE, TUTOR_FROM_GRAVE)))
    modes &= ~GA_UNEARTH;
  if ((modes & GA_PLAYABLE_FROM_GRAVE) &&
      grafdiggers_cages(source.player, global_cards_data[id].type, TUTOR_FROM_GRAVE))
    modes &= ~GA_PLAYABLE_FROM_GRAVE;
  int result = proxy_available ? modes : 0;
  if (getenv("DUEL_TEST_TRACE")) fprintf(stderr,"GY_PROBE available=%x proxy_available=%x\n",available,proxy_available);
  duel_test_restore(saved);
  duel_test_free_snapshot(saved);
  return result;
}
int duel_test_can_activate_graveyard(DuelTestRef source) {
  return (graveyard_ability_modes(source) & (GA_BASIC | GA_UNEARTH)) != 0;
}
int duel_test_activate_graveyard(DuelTestRef source) {
  int id = graveyard_ability_iid(source);
  int modes = graveyard_ability_modes(source) & (GA_BASIC | GA_UNEARTH);
  if (id < 0 || !modes) return 0;
  DuelTestRef proxy = hand_ability_proxy(source.player);
  int before = g_stack_size;
  int result = duel_test_activate(proxy);
  if (!result) return 0;
  if (g_stack_size != before + 1) duel_host_fail("graveyard ability did not leave one pending activation");
  DuelTestRef top = {global_stack_cards[g_stack_size - 1].player, global_stack_cards[g_stack_size - 1].card};
  card_instance_t* activation = &global_card_instances[top.player][top.card];
  if (activation->parent_controller != proxy.player || activation->parent_card != proxy.card ||
      !(activation->targets[2].player & modes) || activation->targets[2].card != id)
    duel_host_fail("graveyard ability menu selected a different source or mode");
  return result;
}
int duel_test_can_cast_graveyard(DuelTestRef source) {
  int id=graveyard_ability_iid(source);
  if (g_stack_size || id<0 || global_cards_data[id].cc[2]!=9 ||
      !(global_cards_data[id].type & (TYPE_SORCERY | TYPE_INSTANT | TYPE_INTERRUPT))) return 0;
  return !!(graveyard_ability_modes(source) & GA_PLAYABLE_FROM_GRAVE);
}
int duel_test_cast_graveyard(DuelTestRef source) {
  if (!duel_test_can_cast_graveyard(source)) return 0;
  int id=graveyard_ability_iid(source), occurrence=0;
  for (int index=0; index<source.card; ++index)
    occurrence += global_graveyard_slots[source.player][index]==id;
  char label[256];
  int size=snprintf(label,sizeof(label),"Flashback %s",global_raw_cards_storage[global_cards_data[id].id].name);
  if (size<0 || size>=(int)sizeof(label)) duel_host_fail("Flashback menu label is too long");
  flashback_menu_label=label; flashback_menu_player=source.player;
  flashback_menu_occurrence=occurrence; flashback_menu_seen=0;
  int result=duel_test_activate(hand_ability_proxy(source.player));
  flashback_menu_label=NULL;
  if (result && !flashback_menu_seen) duel_host_fail("Flashback casting bypassed its real menu");
  /* The engine casts and resolves inside its menu callback. Finish the real
     RulesEngine proxy afterwards; never manufacture a pending spell. */
  if (result) duel_test_resolve();
  return result;
}
int duel_test_cast(DuelTestRef s) { if (!put_card_on_stack(s.player,s.card,0)) return 0; return put_card_on_stack(s.player,s.card,1); }
int duel_test_cast_response(DuelTestRef source) {
  if (g_stack_size <= 0 || g_current_spell_player >= 0)
    duel_host_fail("cast_response requires a pending spell outside a casting callback");
  int top = g_stack_size - 1;
  if ((((unsigned)g_stack_data[top] >> 16) & 255) != EVENT_RESOLVE_SPELL)
    duel_host_fail("cast_response requires a pending spell, not an activation");
  int previous_player = g_current_spell_player, previous_card = g_current_spell_card;
  /* The response UI normally supplies this context while casting is nested.
     Preserve the same real stack target for an explicit scripted response.
     The ordinary handler, targeting, legality and mana payment still run. */
  g_current_spell_player = global_stack_cards[top].player;
  g_current_spell_card = global_stack_cards[top].card;
  card_instance_t* pending = &global_card_instances[g_current_spell_player][g_current_spell_card];
  int pending_iid = pending->internal_card_id;
  int just_cast = pending->state & STATE_JUST_CAST;
  pending->state &= ~STATE_JUST_CAST;
  int result = duel_test_cast(source);
  if (pending->internal_card_id == pending_iid) pending->state |= just_cast;
  g_current_spell_player = previous_player;
  g_current_spell_card = previous_card;
  return result;
}
extern void manalinkeh_reset_dead_count(void);
void duel_test_reset_death_history(void) {
  if (g_stack_size) duel_host_fail("reset_death_history requires an empty stack");
  /* Reserved Deadbox token metadata overlaps a history slot until the normal
     start-turn reset. Opt in before authored deaths, never during refresh. */
  manalinkeh_reset_dead_count();
}
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
void duel_test_resolve_response(void) {
  if (g_stack_size < 2 || g_current_spell_player >= 0)
    duel_host_fail("resolve_response requires a response and its pending parent spell");
  int parent = g_stack_size - 2;
  if ((((unsigned)g_stack_data[parent] >> 16) & 255) != EVENT_RESOLVE_SPELL)
    duel_host_fail("resolve_response parent must be a spell");
  int previous_player = g_current_spell_player, previous_card = g_current_spell_card;
  g_current_spell_player = global_stack_cards[parent].player;
  g_current_spell_card = global_stack_cards[parent].card;
  card_instance_t* pending = &global_card_instances[g_current_spell_player][g_current_spell_card];
  int pending_iid = pending->internal_card_id;
  int just_cast = pending->state & STATE_JUST_CAST;
  pending->state &= ~STATE_JUST_CAST;
  /* Ordinary UI responses resolve inside the parent's casting callback. Supply
     that real target context without changing ordinary scripted resolution. */
  duel_test_resolve();
  if (pending->internal_card_id == pending_iid) pending->state |= just_cast;
  g_current_spell_player = previous_player;
  g_current_spell_card = previous_card;
}
void duel_test_cleanup(void) { g_current_phase=PHASE_CLEANUP; cleanup_phase(g_current_turn); end_turn_phase(g_current_turn); }
void duel_test_remove_from_combat(DuelTestRef r) {
  manalinkeh_remove_from_combat(r.player, r.card);
  manalinkeh_recalculate_all_cards_in_play();
}
void duel_test_destroy(DuelTestRef r) {
  kill_card(r.player,r.card,KILL_DESTROY);
  /* Destruction may queue regeneration and graveyard processing. Finish the real
     death pipeline before the script starts its next action. */
  regenerate_or_graveyard_triggers();
}
int duel_test_attack(DuelTestRef r) {
  if (!can_attack(r.player,r.card)) return 0;
  global_card_instances[r.player][r.card].state |= STATE_ATTACKING;
  /* The combat fixture can preload STATE_ATTACKING without completing a
     declaration. Preserve its explicit attack step and apply the tap obligation.
     A scripted declaration has the same tap obligation as a game declaration.
     Use the engine's vigilance query and tap operation, including tap events. */
  if (!has_vigilance(r.player,r.card)) tap_card_and_dispatch_event(r.player,r.card);
  /* Group declarations dispatch once after every attacker is selected. The
     older blocker-phase shortcut does not exercise attack triggers. */
  if (g_current_phase != PHASE_DECLARE_ATTACKERS)
    manalinkeh_dispatch_event_to_single_card(r.player,r.card,EVENT_DECLARE_ATTACKERS,1-r.player,-1);
  return 1;
}
void duel_test_begin_attackers(void) {
  if (g_current_phase != PHASE_MAIN1)
    duel_host_fail("begin_attackers requires first main phase");
  g_current_phase = PHASE_DECLARE_ATTACKERS;
}
void duel_test_finish_attackers(void) {
  if (g_current_phase != PHASE_DECLARE_ATTACKERS)
    duel_host_fail("finish_attackers requires attacker declaration");
  /* This engine entry point counts all attackers, notifies every permanent,
     resolves XTRIGGER_ATTACKING, and refreshes derived characteristics. */
  C_dispatch_event_raw(EVENT_DECLARE_ATTACKERS);
  g_current_phase = PHASE_BEFORE_BLOCKING;
}
void duel_test_begin_blockers(void) {
  if (g_current_phase != PHASE_BEFORE_BLOCKING)
    duel_host_fail("begin_blockers requires the before-blocking response window");
  g_current_phase = PHASE_DECLARE_BLOCKERS;
  dispatch_trigger_twice_once_with_each_player_as_reason(g_current_turn, TRIGGER_MUST_BLOCK, gs_choose_defenders_00789a50, 0);
}
int duel_test_block(DuelTestRef b,DuelTestRef a) {
  /* The game UI restricts declarations to the defending player's creatures. */
  if (b.player == a.player || a.player != g_current_turn) return 0;
  /* Gameplay selects the available shadow for an additional block, rather than
     overwriting the original creature's assignment. Scripts keep its name. */
  if (global_card_instances[b.player][b.card].state & STATE_BLOCKING) {
    int shadow = -1;
    for (int card = 0; card < g_active_cards_count[b.player]; ++card) {
      card_instance_t *copy = &global_card_instances[b.player][card];
      if (card != b.card && copy->internal_card_id >= 0 &&
          (global_cards_data[copy->internal_card_id].code_pointer == manalinkeh_card_multiblocker_hook ||
           global_cards_data[copy->internal_card_id].code_pointer == card_two_headed_giant_of_foriys_legacy) &&
          copy->damage_source_player == b.player && copy->damage_source_card == b.card &&
          (copy->state & STATE_IN_PLAY) && !(copy->state & STATE_BLOCKING)) {
        shadow = card;
        break;
      }
    }
    if (shadow < 0) return 0;
    b.card = shadow;
  }
  if (!assign_blocker_to_attacker(b.player,b.card,a.player,a.card)) return 0;
  /* Match the declaration performed after this legality check by the game UI. */
  unsigned char parent = global_card_instances[a.player][a.card].blocking;
  global_card_instances[b.player][b.card].state |= STATE_BLOCKING;
  global_card_instances[b.player][b.card].blocking = (unsigned char)(parent == 0xff ? a.card : parent);
  manalinkeh_recalculate_all_cards_in_play();
  /* Match choose_defenders' post-selection trigger. It creates Manalink's
     additional-blocker shadows and runs other blocker-selection handlers. */
  if (g_battlefield_extra_ability_flags & EA_SELECT_BLOCK)
    manalinkeh_dispatch_trigger2(g_current_turn, TRIGGER_BLOCKER_CHOSEN, gs_choose_defenders_00789a50, 0, b.player, b.card);
  return 1;
}
void duel_test_advance_combat(void) {
  if (g_current_phase != PHASE_DECLARE_BLOCKERS)
    duel_host_fail("advance_combat requires blocker declaration");
  /* Match resolve_combat_if_needed: a single event sees the complete board,
     including an empty declaration, before opening the response window. */
  manalinkeh_finish_blocker_selection();
  C_dispatch_event_raw(EVENT_DECLARE_BLOCKERS);
  mark_blocked_attackers(g_current_turn);
  g_current_phase = PHASE_AFTER_BLOCKING;
  manalinkeh_recalculate_all_cards_in_play();
}
void duel_test_combat_damage(void) {
  if (g_current_phase != PHASE_AFTER_BLOCKING)
    duel_host_fail("advance_combat before combat damage");
  resolve_combat_damage(g_current_turn);
}
void duel_test_end_combat(void) {
  if (g_current_phase != PHASE_NORMAL_COMBAT_DAMAGE)
    duel_host_fail("end_combat requires completed combat damage");
  /* The game's combat loop resolves this trigger before clearing assignments.
     Leave the fixture's mana pools intact, as other phase helpers do. */
  g_response_processing_flags = 1;
  dispatch_trigger_twice_once_with_each_player_as_reason(g_current_turn, TRIGGER_END_COMBAT, gs_end_of_combat_008b43a0, 0);
  cleanup_combat_state(g_current_turn);
  g_current_phase = PHASE_MAIN2;
  manalinkeh_recalculate_all_cards_in_play();
}
void duel_test_advance_turn(void) {
  if (g_stack_size != 0) duel_host_fail("advance_turn requires an empty stack");
  duel_test_cleanup();
  g_current_turn=1-g_current_turn;
  g_current_player=g_current_turn;
  ++turn_count;
  /* Run the game's turn gate (skip-turn effects, per-turn traps and sickness).
     The network flag suppresses the host's autosave, not rules dispatch.
     As with fixture phase selection, upkeep/draw/untap are deliberately omitted. */
  int previous_network_flags=g_duel_network_flags;
  g_duel_network_flags |= 2;
  int skipped=TENTATIVE_start_turn(g_current_turn);
  g_duel_network_flags=previous_network_flags;
  if (skipped) duel_host_fail("advance_turn encountered a skipped turn");
  g_current_phase=PHASE_MAIN1;
  manalinkeh_recalculate_all_cards_in_play();
  untap_prepared_player=g_current_turn;
  untap_prepared_turn=turn_count;
}
extern int manalinkeh_untap_phase(int);
int duel_test_untap_step(int player) {
  if (player < 0 || player > 1 || player != g_current_turn || player != g_current_player ||
      g_stack_size || g_current_phase != PHASE_MAIN1 ||
      untap_prepared_player != player || untap_prepared_turn != turn_count) return 0;
  /* The real wrapper processes mana burn. These bounded fixtures enter before
     any mana production; do not silently erase caller pools or burn life. */
  for (int p=0; p<2; ++p)
    for (int color=0; color<6; ++color)
      if (duel_test_mana(p,color)) return 0;
  untap_prepared_player=-1;
  char flags[2]={g_duel_phase_stop_settings[0].phase_flags[PHASE_UNTAP],
                 g_duel_phase_stop_settings[1].phase_flags[PHASE_UNTAP]};
  int stop=g_stop_phase, stop_player=g_stop_phase_player;
  int previous=g_previous_stop_phase, previous_player=g_previous_stop_phase_player;
  for (int p=0; p<2; ++p) g_duel_phase_stop_settings[p].phase_flags[PHASE_UNTAP]=0;
  if (g_stop_phase==PHASE_UNTAP) g_stop_phase=g_stop_phase_player=-1;
  if (g_previous_stop_phase==PHASE_UNTAP) g_previous_stop_phase=g_previous_stop_phase_player=-1;
  /* This complete gameplay entry point queries permissions, commits untaps,
     emits untap notifications and expires next-untap restrictions. */
  manalinkeh_untap_phase(player);
  for (int p=0; p<2; ++p) g_duel_phase_stop_settings[p].phase_flags[PHASE_UNTAP]=flags[p];
  g_stop_phase=stop; g_stop_phase_player=stop_player;
  g_previous_stop_phase=previous; g_previous_stop_phase_player=previous_player;
  return 1;
}
int duel_test_shuffle_count(int player) {
  return player >= 0 && player < 2 ? (int)duel_host_capture().shuffle_count[player] : -1;
}
int duel_test_token_characteristic(DuelTestRef r, const char *property)
{
  if (!strcmp(property,"token")) return !!(global_card_instances[r.player][r.card].token_status & 0x10); /* STATUS_TOKEN */
  if (!strcmp(property,"color")) return manalinkeh_get_color(r.player,r.card) & 0x3e; /* colored bits; colorless is absence */
  if (!strcmp(property,"artifact")) return !!(manalinkeh_get_type(r.player,r.card) & TYPE_ARTIFACT);
  static const struct { const char *name; subtype_t code; } subtypes[] = {
#define TOKEN_SUBTYPE(name, code) {"subtype_" name, SUBTYPE_##code}
    TOKEN_SUBTYPE("Angel",ANGEL), TOKEN_SUBTYPE("Bear",BEAR), TOKEN_SUBTYPE("Bird",BIRD),
    TOKEN_SUBTYPE("Centaur",CENTAUR), TOKEN_SUBTYPE("Citizen",CITIZEN), TOKEN_SUBTYPE("Dragon",DRAGON),
    TOKEN_SUBTYPE("Drake",DRAKE), TOKEN_SUBTYPE("Eldrazi",ELDRAZI), TOKEN_SUBTYPE("Elf",ELF),
    TOKEN_SUBTYPE("Goblin",GOBLIN), TOKEN_SUBTYPE("Horror",HORROR), TOKEN_SUBTYPE("Human",HUMAN),
    TOKEN_SUBTYPE("Insect",INSECT), TOKEN_SUBTYPE("Knight",KNIGHT), TOKEN_SUBTYPE("Ally",ALLY),
    TOKEN_SUBTYPE("Minotaur",MINOTAUR), TOKEN_SUBTYPE("Myr",MYR), TOKEN_SUBTYPE("Satyr",SATYR),
    /* The core header predates Servo; Manalink's subtypes.h assigns 0x00ee. */
    {"subtype_Servo", (subtype_t)0x00ee}, TOKEN_SUBTYPE("Sliver",SLIVER), TOKEN_SUBTYPE("Soldier",SOLDIER),
    TOKEN_SUBTYPE("Spirit",SPIRIT), TOKEN_SUBTYPE("Thopter",THOPTER), TOKEN_SUBTYPE("Warrior",WARRIOR),
    TOKEN_SUBTYPE("Wurm",WURM), TOKEN_SUBTYPE("Boar",BOAR), TOKEN_SUBTYPE("Cat",CAT),
    TOKEN_SUBTYPE("Spider",SPIDER), TOKEN_SUBTYPE("Saproling",SAPROLING), TOKEN_SUBTYPE("Phyrexian",PHYREXIAN),
    TOKEN_SUBTYPE("Zombie",ZOMBIE), TOKEN_SUBTYPE("Construct",CONSTRUCT), TOKEN_SUBTYPE("Beast",BEAST),
    TOKEN_SUBTYPE("Illusion",ILLUSION), TOKEN_SUBTYPE("Ooze",OOZE), TOKEN_SUBTYPE("Squirrel",SQUIRREL),
    TOKEN_SUBTYPE("Plant",PLANT), TOKEN_SUBTYPE("Bat",BAT), TOKEN_SUBTYPE("Elemental",ELEMENTAL),
    TOKEN_SUBTYPE("Clue",CLUE), TOKEN_SUBTYPE("Elephant",ELEPHANT), TOKEN_SUBTYPE("Kithkin",KITHKIN),
    TOKEN_SUBTYPE("Treefolk",TREEFOLK), TOKEN_SUBTYPE("Shaman",SHAMAN), TOKEN_SUBTYPE("Snake",SNAKE),
    TOKEN_SUBTYPE("Merfolk",MERFOLK), TOKEN_SUBTYPE("Wizard",WIZARD), TOKEN_SUBTYPE("Golem",GOLEM),
    TOKEN_SUBTYPE("Rogue",ROGUE), TOKEN_SUBTYPE("Scion",SCION), TOKEN_SUBTYPE("Spawn",SPAWN),
    TOKEN_SUBTYPE("Wolf",WOLF), TOKEN_SUBTYPE("Ape",APE),
    TOKEN_SUBTYPE("Frog",FROG), TOKEN_SUBTYPE("Lizard",LIZARD)
#undef TOKEN_SUBTYPE
  };
  for (unsigned n=0;n<sizeof(subtypes)/sizeof(subtypes[0]);++n)
    if (!strcmp(property,subtypes[n].name)) return !!manalinkeh_has_subtype(r.player,r.card,subtypes[n].code);
  duel_host_fail("unknown token characteristic"); return 0;
}
DuelTestCard duel_test_inspect(DuelTestRef r)
{
  card_instance_t *i=&global_card_instances[r.player][r.card];
  DuelTestCard v={0}; v.csvid=i->internal_card_id<0?-1:global_cards_data[i->internal_card_id].id;
  v.in_play=(i->internal_card_id>=0 && manalinkeh_in_play(r.player,r.card));
  v.in_hand=(i->internal_card_id>=0 && manalinkeh_in_hand(r.player,r.card));
  if (v.in_play) {
    v.minus1_counters=manalinkeh_count_counters(r.player,r.card,1); /* COUNTER_M1_M1 */
    v.plus1_counters=manalinkeh_count_counters(r.player,r.card,0); /* COUNTER_P1_P1 */
    v.blocking_count=manalinkeh_count_creatures_this_is_blocking(r.player,r.card);
    /* The core engine also creates multiblocker copies (Blaze of Glory). The
       Manalink counter recognizes its own callback only, so include core
       copies through the same single-copy/banding query. */
    for (int c=0;c<g_active_cards_count[r.player];++c) {
      card_instance_t *copy=&global_card_instances[r.player][c];
      if (c!=r.card && copy->internal_card_id>=0 &&
          global_cards_data[copy->internal_card_id].code_pointer==card_two_headed_giant_of_foriys_legacy &&
          copy->damage_source_player==r.player && copy->damage_source_card==r.card &&
          manalinkeh_in_play(r.player,c))
        v.blocking_count+=manalinkeh_count_creatures_this_is_blocking(r.player,c);
    }
    /* Becoming blocked persists after the last blocker leaves combat. Counting
       living blockers (is_unblocked) loses that fact, e.g. after flanking kills
       a 1/1. Inspect the same historical flag used by core combat damage. */
    v.blocked=!!(i->state&STATE_ATTACKING) && !!(i->state&STATE_ISBLOCKED);
  }
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
static DuelTestRef active_zone_at(int player, int zone, int index) {
  for (int card = 0, n = 0; card < g_active_cards_count[player]; ++card) {
    card_instance_t *instance = &global_card_instances[player][card];
    if (instance->internal_card_id >= 0 && (!!(instance->state & STATE_IN_PLAY) == (zone == DUEL_ZONE_BATTLEFIELD)) &&
        !(global_cards_data[instance->internal_card_id].type & TYPE_EFFECT) && n++ == index)
      return (DuelTestRef){player, card};
  }
  return (DuelTestRef){player, -1};
}
DuelTestRef duel_test_permanent_at(int player, int index) { return active_zone_at(player, DUEL_ZONE_BATTLEFIELD, index); }
DuelTestRef duel_test_hand_at(int player, int index) { return active_zone_at(player, DUEL_ZONE_HAND, index); }
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

extern char *get_displayed_card_name(int player, int card);
extern void manalinkeh_format_manacost_into_global_allpurpose_buffer(int, int *, int, int);
extern char global_all_purpose_buffer[1024];
const char *duel_test_card_name(DuelTestRef ref) { return get_displayed_card_name(ref.player, ref.card); }
const char *duel_test_payment_prompt(void) {
  int cost[8] = {2, 0, 0, 0, 0, 0, 0, 0};
  manalinkeh_format_manacost_into_global_allpurpose_buffer(0, cost, 0, -1);
  return global_all_purpose_buffer;
}
