/* Exercise surviving engine functions with the injected Manalink calls mocked.
 * No assets or window system are required. */
#define GLOBAL_STATE_IMPL
#define GLOBALS_OTHERS_IMPL
#define GLOBAL_STRINGS_IMPL
#include "magic/src/game_support.h"
#include "magic/src/global_strings.h"
#include <stdio.h>
#include <string.h>

card_data_t global_cards_data[16];
int ai_blocker_count;
int ai_blocker_cards[16];
int g_duel_ai_mode_state;
int g_duel_network_state;
int g_stack_proxy_internal_card_id;
int cleanup_combat_state(int player);
int is_combat_damage_ability_in_step(int step, int abilities);
int untap_phase_exe(unsigned int player);
void rebuild_battlefield_summary(void);

CRITICAL_SECTION g_duel_render_lock;
int g_ai_response_state;
int g_duel_action_pending;
int g_saved_stop_phase_player, g_response_prompt_enabled, g_duel_result;
void reset_duel_globals(void);
void save_ai_search_state(void);
void restore_ai_search_state(void);
void save_combat_simulation_state(void);
void restore_combat_simulation_state(void);
unsigned int refresh_duel_display_cache(void);
int get_displayed_player_counters(int player);
unsigned int save_or_load_ver1(void);
unsigned int save_or_load_ver2(void);
extern int g_duel_cached_graveyard_player_0[500], g_duel_cached_graveyard_player_1[500];
extern int g_duel_cached_exile_player_0[500], g_duel_cached_exile_player_1[500];
extern int g_duel_cached_library_player_0[500], g_duel_cached_library_player_1[500];

static int counter_io_calls;
static int counter_io_loading;
static unsigned char saved_counter_bytes[8];
void FUN_00464bb0(void) {}
void refresh_ai_random_table(void) {}
int CardIDFromType(unsigned int iid) { return iid; }
int CardTypeFromID(int csv) { return csv; }
int save_or_load_data(void *data, unsigned int size)
{
  if (data == player_counters)
  {
    ++counter_io_calls;
    if (size != sizeof(saved_counter_bytes)) return 0;
    if (counter_io_loading) memcpy(data, saved_counter_bytes, size);
    else memcpy(saved_counter_bytes, data, size);
  }
  return 1;
}

static int failures;
static int created_player, created_card, created_iid;
static int untapped_player, untapped_card;
static int graveyard_events, leave_events, graveyard_moves;
static int refreshes;

static void check(int condition, const char *message)
{
  if (!condition)
  {
    fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

void manalinkeh_create_card_instance(int player, unsigned int iid, int card)
{
  created_player = player;
  created_card = card;
  created_iid = (int)iid;
  PLAYER_CARD_INSTANCE(player, card).internal_card_id = (int)iid;
  PLAYER_CARD_INSTANCE(player, card).original_internal_card_id = (int)iid;
  PLAYER_CARD_INSTANCE(player, card).backup_internal_card_id = (unsigned short)iid;
  PLAYER_CARD_INSTANCE(player, card).card_color = COLOR_TEST_BLUE;
}

void manalinkeh_untap_card(int player, int card)
{
  untapped_player = player;
  untapped_card = card;
  PLAYER_CARD_INSTANCE(player, card).state &= ~STATE_TAPPED;
}

int manalinkeh_play_sound_effect(wav_t sound) { (void)sound; return 0; }
void manalinkeh_recalculate_all_cards_in_play(void) {}
int manalinkeh_get_abilities(int player, int card, event_t event, int unused)
{
  (void)unused;
  return event == EVENT_POWER ? PLAYER_CARD_INSTANCE(player, card).power
                              : PLAYER_CARD_INSTANCE(player, card).toughness;
}
void request_duel_display_refresh_if_human(int player, int card, int reason, int flags)
{ (void)player; (void)card; (void)reason; (void)flags; ++refreshes; }
int manalinkeh_destroy_attached_auras_and_obliterate_card(int player, int card)
{ (void)player; (void)card; return 0; }
void manalinkeh_raw_put_card_in_graveyard(int player, int card)
{ (void)player; (void)card; ++graveyard_moves; }
void manalinkeh_dispatch_event_raw(event_t event)
{ if (event == EVENT_GRAVEYARD_FROM_PLAY) ++graveyard_events; }
int dispatch_trigger_twice_once_with_each_player_as_reason(int reason, trigger_t trigger,
                                                         const char *prompt, int arg)
{
  (void)reason; (void)prompt; (void)arg;
  if (trigger == TRIGGER_LEAVE_PLAY) ++leave_events;
  return 0;
}
int manalinkeh_dispatch_event_to_single_card(int player, int card, event_t event,
                                           int attacking_player, int attacking_card)
{ (void)player; (void)card; (void)event; (void)attacking_player; (void)attacking_card; return 0; }
int manalinkeh_hostabi_dispatch_card_trigger_event(int player, int card, event_t event, int extra)
{ (void)player; (void)card; (void)event; (void)extra; return 0; }
void manalinkeh_kill_card_exe(int player, int card, kill_t mode)
{ (void)player; (void)card; (void)mode; }
void process_damage_prevention(int player) { (void)player; }
int regenerate_or_graveyard_triggers(void) { return 0; }
void show_duel_system_error(const char *message) { check(0, message); }
int dispatch_event(int player, int card, event_t event)
{ (void)player; (void)card; (void)event; return 0; }
void manalinkeh_phase_changed(int player, phase_t phase) { (void)player; (void)phase; }
int player_can_stop_at_phase(int player, phase_t phase) { (void)player; (void)phase; return 0; }
void reset_trigger_dispatch_state(void) {}
int load_text(const char *file, const char *section) { (void)file; (void)section; return 0; }
int select_card_for_action(int player, int arg2, int to_check, unsigned int type,
                           unsigned int color, char *prompt, int mode)
{ (void)player; (void)arg2; (void)to_check; (void)type; (void)color; (void)prompt; (void)mode; return -1; }
int contains_player_card_pair(int *pairs, int count, int player, int card)
{ (void)pairs; (void)count; (void)player; (void)card; return 0; }
int manalinkeh_is_a_tappable_mana_source(int player, int card) { (void)player; (void)card; return 0; }
void manalinkeh_tap_card_for_mana(int player, int card) { (void)player; (void)card; }

static void reset(void)
{
  int p, c;
  memset(global_card_instances, 0, sizeof(global_card_instances));
  memset(global_cards_data, 0, sizeof(global_cards_data));
  memset(&g_duel_summary, 0, sizeof(g_duel_summary));
  for (p = 0; p < 2; ++p)
  {
    for (c = 0; c < 151; ++c)
    {
      PLAYER_CARD_INSTANCE(p, c).internal_card_id = -1;
      PLAYER_CARD_INSTANCE(p, c).original_internal_card_id = -1;
    }
    for (c = 0; c < 500; ++c) global_exile[p][c] = -1;
    g_active_cards_count[p] = 0;
  }
  graveyard_events = leave_events = graveyard_moves = refreshes = 0;
  created_player = created_card = created_iid = -1;
  untapped_player = untapped_card = -1;
  g_duel_ai_mode_state = 1;
  g_nested_trigger_depth = 2;
  g_stack_proxy_internal_card_id = 15;
  global_cards_data[2].type = TYPE_CREATURE;
  global_cards_data[3].type = TYPE_CREATURE;
}

static void test_combat_steps(void)
{
  int step;
  for (step = 0; step < 2; ++step)
  {
    check(is_combat_damage_ability_in_step(step, KEYWORD_DOUBLE_STRIKE),
          "Double strike must deal damage in both steps");
    check(is_combat_damage_ability_in_step(step, KEYWORD_DOUBLE_STRIKE | KEYWORD_FIRST_STRIKE),
          "Double strike must still deal regular damage with first strike set");
    check(is_combat_damage_ability_in_step(step, KEYWORD_FIRST_STRIKE) == (step == 0),
          "First strike alone must only deal first-strike damage");
    check(is_combat_damage_ability_in_step(step, 0) == (step == 1),
          "Ordinary creatures must only deal regular damage");
  }
}

static void test_multicolor_summary(void)
{
  reset();
  g_active_cards_count[0] = 3;
  PLAYER_CARD_INSTANCE(0, 0).internal_card_id = 2;
  PLAYER_CARD_INSTANCE(0, 0).state = STATE_IN_PLAY;
  PLAYER_CARD_INSTANCE(0, 0).power = 4;
  PLAYER_CARD_INSTANCE(0, 0).toughness = 6;
  global_cards_data[2].color = COLOR_TEST_BLUE | COLOR_TEST_RED;
  PLAYER_CARD_INSTANCE(0, 1).internal_card_id = 3;
  PLAYER_CARD_INSTANCE(0, 1).state = STATE_IN_PLAY;
  PLAYER_CARD_INSTANCE(0, 1).power = 2;
  PLAYER_CARD_INSTANCE(0, 1).toughness = 3;
  global_cards_data[3].color = COLOR_TEST_COLORLESS;
  g_active_cards_count[1] = 1;
  PLAYER_CARD_INSTANCE(1, 0).internal_card_id = 2;
  PLAYER_CARD_INSTANCE(1, 0).state = STATE_IN_PLAY;
  PLAYER_CARD_INSTANCE(1, 0).power = 7;
  PLAYER_CARD_INSTANCE(1, 0).toughness = 8;
  PLAYER_CARD_INSTANCE(0, 2).internal_card_id = 2;
  PLAYER_CARD_INSTANCE(0, 2).state = STATE_IN_PLAY | STATE_OUBLIETTED;
  rebuild_battlefield_summary();
  check(g_creature_power_by_color[0][COLOR_BLUE] == 4 &&
        g_creature_power_by_color[0][COLOR_RED] == 4 &&
        g_creature_toughness_by_color[0][COLOR_BLUE] == 6 &&
        g_creature_toughness_by_color[0][COLOR_RED] == 6,
        "Multicolored creatures must contribute to each matching color summary");
  check(g_creature_power_by_color[0][COLOR_COLORLESS] == 2 &&
        g_creature_toughness_by_color[0][COLOR_COLORLESS] == 3 &&
        g_creature_power_by_color[0][7] == 6 && g_creature_toughness_by_color[0][7] == 9,
        "Colorless creatures must contribute and combined totals must count each creature once");
  check(g_duel_summary.creature_counts[0] == 2 && g_duel_summary.hand_counts[0] == 0,
        "Phased-out creatures must not count as creatures in play or cards in hand");
  check(g_creature_power_by_color[1][COLOR_BLUE] == 7 &&
        g_creature_power_by_color[1][COLOR_RED] == 7 &&
        g_creature_toughness_by_color[1][7] == 8,
        "Both players must have independent multicolor summary rows");
}

static void test_combat_cleanup(void)
{
  reset();
  ai_blocker_count = 2;
  ai_blocker_cards[0] = 4;
  ai_blocker_cards[1] = 5;
  PLAYER_CARD_INSTANCE(1, 4).state = STATE_BLOCKING | STATE_TAPPED;
  PLAYER_CARD_INSTANCE(1, 5).state = STATE_IN_PLAY;
  g_active_cards_count[0] = 1;
  PLAYER_CARD_INSTANCE(0, 0).state = STATE_ATTACKING | STATE_IN_PLAY;
  cleanup_combat_state(0);
  check(PLAYER_CARD_INSTANCE(1, 4).state == (STATE_BLOCKED | STATE_TAPPED),
        "Combat cleanup must remember blocking and preserve unrelated flags");
  check(PLAYER_CARD_INSTANCE(1, 5).state == STATE_IN_PLAY,
        "Cleanup must not mark a creature that stopped blocking before damage");
  check(PLAYER_CARD_INSTANCE(0, 0).state == (STATE_ATTACKED | STATE_IN_PLAY),
        "Combat cleanup must retain the attacked flag");
}

static void test_creation_and_exile(void)
{
  int card;
  reset();
  PLAYER_CARD_INSTANCE(1, 0).original_internal_card_id = 2;
  card = add_card_to_hand(1, 3);
  check(card == 1 && created_player == 1 && created_card == 1 && created_iid == 3,
        "Hand allocation must use Manalink's initializer for the free slot");
  check(PLAYER_CARD_INSTANCE(1, 1).backup_internal_card_id == 3 &&
        PLAYER_CARD_INSTANCE(1, 1).card_color == COLOR_TEST_BLUE && g_active_cards_count[1] == 2,
        "Extended initialization must reach the engine's instance and active count");
  PLAYER_CARD_INSTANCE(1, 1).internal_card_id = 2; /* A type-changing effect. */
  move_card_to_exile(0, 1, 1);
  check(global_exile[0][0] == 3 && global_exile[1][0] == -1,
        "Exile must use the original identity and owner, independently of controller");
  PLAYER_CARD_INSTANCE(1, 1).token_status = STATUS_TOKEN;
  move_card_to_exile(0, 1, 1);
  check(global_exile[0][1] == -1, "Exiling a token must not add a card to the exile zone");
}

static void test_death_events(void)
{
  reset();
  PLAYER_CARD_INSTANCE(0, 0).internal_card_id = 2;
  PLAYER_CARD_INSTANCE(0, 0).original_internal_card_id = 3;
  PLAYER_CARD_INSTANCE(0, 0).kill_code = KILL_BURY;
  PLAYER_CARD_INSTANCE(0, 0).token_status = STATUS_CANNOT_LEAVE_PLAY;
  process_killed_card(0, 0);
  check(graveyard_events == 0 && leave_events == 0 && graveyard_moves == 0,
        "Persistent detached effects must bypass death and leave-play events");
  check(PLAYER_CARD_INSTANCE(0, 0).internal_card_id == -1,
        "The departed instance must still be reaped");
  reset();
  PLAYER_CARD_INSTANCE(1, 0).internal_card_id = 2;
  PLAYER_CARD_INSTANCE(1, 0).original_internal_card_id = 3;
  PLAYER_CARD_INSTANCE(1, 0).kill_code = KILL_REMOVE;
  process_killed_card(1, 0);
  check(global_exile[0][0] == 3 && graveyard_events == 1 && leave_events == 1,
        "Ordinary exiled creatures must still dispatch events and use the owner");
}

static void test_stack_untap(void)
{
  reset();
  g_stack_size = 1;
  global_stack_cards[0].player = 0;
  global_stack_cards[0].card = 1;
  g_stack_data[0] = 0;
  PLAYER_CARD_INSTANCE(0, 1).internal_card_id = 2;
  PLAYER_CARD_INSTANCE(0, 1).upkeep_flags = 0xc0;
  PLAYER_CARD_INSTANCE(0, 1).parent_controller = 1;
  PLAYER_CARD_INSTANCE(0, 1).parent_card = 4;
  PLAYER_CARD_INSTANCE(1, 4).state = STATE_TAPPED;
  PLAYER_CARD_INSTANCE(1, 4).upkeep_flags = 0x80;
  resolve_top_card_on_stack();
  check(untapped_player == 1 && untapped_card == 4 &&
        (PLAYER_CARD_INSTANCE(1, 4).state & STATE_TAPPED) == 0,
        "Resolving an untap-cost proxy must call Manalink's untap hook on its parent");
  check((PLAYER_CARD_INSTANCE(1, 4).upkeep_flags & 0x80) == 0 && g_stack_size == 0,
        "Untap resolution must clear the parent's proxy flag and pop the stack");
}

static void test_untap_preparation(void)
{
  reset();
  g_active_cards_count[0] = 1;
  PLAYER_CARD_INSTANCE(0, 0).internal_card_id = 2;
  PLAYER_CARD_INSTANCE(0, 0).state = STATE_IN_PLAY | STATE_TAPPED;
  check(untap_phase_exe(0) == 0, "The legacy untap preparation must finish normally");
  check((PLAYER_CARD_INSTANCE(0, 0).state & STATE_TAPPED) != 0 &&
        PLAYER_CARD_INSTANCE(0, 0).untap_status == 3,
        "Untap preparation must leave the final untap and status reset to Manalink's wrapper");
}

static void test_player_counters(void)
{
  static const unsigned char expected[8] = {3, 4, 5, 0, 7, 8, 9, 0};
  int legacy_poison[2] = {2, 6};
  int format;
  reset();
  memcpy(player_counters, expected, sizeof(expected));
  save_ai_search_state();
  memset(player_counters, 255, sizeof(player_counters));
  restore_ai_search_state();
  check(memcmp(player_counters, expected, sizeof(expected)) == 0,
        "AI search must restore poison, experience, and energy for both players");
  save_combat_simulation_state();
  memset(player_counters, 0, sizeof(player_counters));
  restore_combat_simulation_state();
  check(memcmp(player_counters, expected, sizeof(expected)) == 0,
        "Combat speculation must restore every player counter byte");

  memset(global_graveyard_slots, -1, sizeof(global_graveyard_slots));
  memset(global_exile, -1, sizeof(global_exile));
  memset(global_library, -1, sizeof(global_library));
  memset(global_ante_cards, -1, sizeof(global_ante_cards));
  memset(global_stack_cards, -1, sizeof(global_stack_cards));
  memset(g_duel_cached_graveyard_player_0, -1, sizeof(g_duel_cached_graveyard_player_0));
  memset(g_duel_cached_graveyard_player_1, -1, sizeof(g_duel_cached_graveyard_player_1));
  memset(g_duel_cached_exile_player_0, -1, sizeof(g_duel_cached_exile_player_0));
  memset(g_duel_cached_exile_player_1, -1, sizeof(g_duel_cached_exile_player_1));
  memset(g_duel_cached_library_player_0, -1, sizeof(g_duel_cached_library_player_0));
  memset(g_duel_cached_library_player_1, -1, sizeof(g_duel_cached_library_player_1));
  InitializeCriticalSection(&g_duel_render_lock);
  refresh_duel_display_cache();
  check(get_displayed_player_counters(0) == 0x050403 &&
        get_displayed_player_counters(1) == 0x090807,
        "The life panel must receive the packed counters for both players");
  check(refresh_duel_display_cache() == 0, "Unchanged state must not request a display refresh");
  player_counters[1].energy_ = 10;
  check(refresh_duel_display_cache() != 0 && get_displayed_player_counters(1) == 0x0a0807,
        "Changing energy alone must invalidate and update the display cache");
  player_counters[0].experience_ = 11;
  check(refresh_duel_display_cache() != 0 && get_displayed_player_counters(0) == 0x050b03,
        "Changing experience alone must invalidate and update the display cache");
  check(get_displayed_player_counters(-1) == 0 && get_displayed_player_counters(2) == 0,
        "Invalid players must have no displayed counters");
  DeleteCriticalSection(&g_duel_render_lock);

  for (format = 1; format <= 2; ++format)
  {
    memcpy(player_counters, expected, sizeof(expected));
    counter_io_loading = global_saveload_loading = counter_io_calls = 0;
    check((format == 1 ? save_or_load_ver1() : save_or_load_ver2()) != 0 &&
          counter_io_calls == 1 && memcmp(saved_counter_bytes, expected, 8) == 0,
          "Both save formats must serialize the complete eight-byte counter slot");
    memset(player_counters, 0, sizeof(player_counters));
    counter_io_loading = global_saveload_loading = 1;
    counter_io_calls = 0;
    check((format == 1 ? save_or_load_ver1() : save_or_load_ver2()) != 0 &&
          counter_io_calls == 1 && memcmp(player_counters, expected, 8) == 0,
          "Both load formats must restore poison, experience, and energy");
    memcpy(saved_counter_bytes, legacy_poison, sizeof(legacy_poison));
    check((format == 1 ? save_or_load_ver1() : save_or_load_ver2()) != 0 &&
          POISON_COUNTERS(0) == 2 && POISON_COUNTERS(1) == 6 &&
          EXPERIENCE_COUNTERS(0) == 0 && ENERGY_COUNTERS(1) == 0,
          "Old poison-only save slots must load with zero energy and experience");
  }
  memcpy(player_counters, expected, sizeof(expected));
  reset_duel_globals();
  check(POISON_COUNTERS(0) == 0 && EXPERIENCE_COUNTERS(0) == 0 && ENERGY_COUNTERS(0) == 0 &&
        POISON_COUNTERS(1) == 0 && EXPERIENCE_COUNTERS(1) == 0 && ENERGY_COUNTERS(1) == 0,
        "Starting a new duel must reset every player counter type");
}

int main(void)
{
  test_player_counters();
  test_combat_steps();
  test_multicolor_summary();
  test_combat_cleanup();
  test_creation_and_exile();
  test_death_events();
  test_stack_untap();
  test_untap_preparation();
  return failures ? 1 : 0;
}
