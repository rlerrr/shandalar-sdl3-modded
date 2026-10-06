#include "manalink.h"
#include "magic/src/carddata_generated.h"

int card_furnace_whelp(int player, int card, event_t event);
int g_active_cards_count[2], g_affected_card, g_affected_card_controller;
int g_event_result, g_trigger_condition, g_current_player, g_current_turn;
int g_duel_ai_mode_state, g_life[2];
int g_stack_proxy_internal_card_id = SHANDALAR_DAMAGE_CARD_IID + 19;
int LEGACY_EFFECT_PUMP = SHANDALAR_DAMAGE_CARD_IID + 1;
char g_text_lines[300][300];
static card_instance_t instances[2][16];
static int failures, creations, callback_calls;

static void check(int condition, const char *message)
{
  if (!condition) { fprintf(stderr, "%s\n", message); ++failures; }
}
void backtrace_and_abort(const char *message)
{
  fprintf(stderr, "Unexpected assertion: %s\n", message);
  abort();
}
card_instance_t *get_card_instance(int player, int card) { return &instances[player][card]; }
card_instance_t *in_play(int player, int card)
{
  card_instance_t *instance = get_card_instance(player, card);
  return instance->internal_card_id >= 0 && (instance->state & STATE_IN_PLAY) ? instance : NULL;
}
int call_card_fn(CardFunction fn, card_instance_t *instance, int player, int card, event_t event)
{
  return fn(player, card, event);
}
// The host allocation boundary supplies a slot and preserves the requested IID.
int create_legacy_effect_exe(int player, int card, int iid, int target_player, int target_card)
{
  int slot;
  for (slot = 0; slot < 16 && instances[player][slot].internal_card_id >= 0; ++slot) {}
  if (slot == 16) return -1;
  card_instance_t *effect = get_card_instance(player, slot);
  memset(effect, 0, sizeof(*effect));
  effect->internal_card_id = iid;
  effect->state = STATE_IN_PLAY;
  effect->damage_source_player = player;
  effect->damage_source_card = card;
  effect->damage_target_player = target_player;
  effect->damage_target_card = target_card;
  if (slot >= active_cards_count[player]) active_cards_count[player] = slot + 1;
  ++creations;
  return slot;
}
void kill_card(int player, int card, kill_t mode)
{
  check(mode == KILL_EXILE, "Temporary pump effects must exile themselves at cleanup");
  instances[player][card].internal_card_id = -1;
  instances[player][card].state = 0;
}
int can_use_activated_abilities(int p, int c) { return 1; }
int has_mana_for_activated_ability(int p, int c, int x, int b, int u, int g, int r, int w) { return 1; }
int charge_mana_for_activated_ability(int p, int c, int x, int b, int u, int g, int r, int w) { return 1; }
int can_pay_life(int p, int amount) { return 1; }
int lose_life(int p, int amount) { return amount; }
int generic_shade_amt_can_pump(int p, int c, int pump, int life, int x, int b, int u, int g, int r, int w, int limit) { return pump; }
int has_mana(int p, color_t color, int amount) { return 1; }
int charge_mana(int p, color_t color, int amount) { return 1; }
int load_text(const char *file, const char *section) { return 0; }
int do_dialog(int p, int bp, int bc, int sp, int sc, const char *options, int choice) { return choice; }

static int callback(int player, int card, event_t event)
{
  ++callback_calls;
  check(player == 0 && instances[player][card].internal_card_id == LEGACY_EFFECT_ACTIVATED,
        "An activated legacy must call its own stored callback");
  return 37;
}
int main(void)
{
  int p, c, legacy;
  for (p = 0; p < 2; ++p)
    for (c = 0; c < 16; ++c) instances[p][c].internal_card_id = -1;
  check(LEGACY_EFFECT_CUSTOM > 0 && LEGACY_EFFECT_ACTIVATED > 0 &&
        LEGACY_EFFECT_CUSTOM != LEGACY_EFFECT_ACTIVATED,
        "Host effect IDs must be initialized and distinct from ordinary cards");
  check(cards_data[LEGACY_EFFECT_CUSTOM].id == 903 &&
        cards_data[LEGACY_EFFECT_CUSTOM].code_pointer == legacy_effect_custom &&
        cards_data[LEGACY_EFFECT_ACTIVATED].code_pointer == legacy_effect_activated,
        "The generated records must match the host's callback effect types");
  check(SHANDALAR_CARD_DATA_COUNT - SHANDALAR_CARD_DATA_ACTIVE_COUNT == 16,
        "Custom effects must leave all sixteen dynamic copy slots available");
  instances[0][0].internal_card_id = 1;
  instances[0][0].state = STATE_IN_PLAY;
  instances[0][4].internal_card_id = activation_card;
  instances[0][4].parent_controller = 0;
  instances[0][4].parent_card = 0;
  active_cards_count[0] = 5;
  card_furnace_whelp(0, 4, EVENT_RESOLVE_ACTIVATION);
  check(creations == 1 && instances[0][1].internal_card_id == LEGACY_EFFECT_CUSTOM &&
        instances[0][1].counter_power == 1 && instances[0][1].eot_toughness == 1,
        "Furnace Whelp must create a +1/+0 effect with its alternate text without asserting");
  card_furnace_whelp(0, 4, EVENT_RESOLVE_ACTIVATION);
  check(creations == 1 && instances[0][1].counter_power == 2,
        "Repeated activations must merge into a single +2/+0 effect");
  affected_card = 0; affected_card_controller = 0; event_result = 0;
  cards_data[LEGACY_EFFECT_CUSTOM].code_pointer(0, 1, EVENT_POWER);
  check(event_result == 2, "The effect's stored callback must actually increase power");
  event_result = 0;
  cards_data[LEGACY_EFFECT_CUSTOM].code_pointer(0, 1, EVENT_TOUGHNESS);
  check(event_result == 0, "Furnace Whelp must not change toughness");
  cards_data[LEGACY_EFFECT_CUSTOM].code_pointer(0, 1, EVENT_CLEANUP);
  check(instances[0][1].internal_card_id == -1, "The pump must end at cleanup");
  legacy = create_targetted_legacy_activate(0, 0, callback, 0, 0);
#ifdef __EMSCRIPTEN__
  check((uintptr_t)callback < 4096, "Exercise a small Wasm function-table index");
#endif
  check(cards_data[LEGACY_EFFECT_ACTIVATED].code_pointer(0, legacy, EVENT_CAN_ACTIVATE) == 37 &&
        callback_calls == 1, "Small callback indices must not take the Nafs Asp path");
  instances[0][legacy].info_slot = 0;
  check(legacy_effect_activated(0, legacy, EVENT_CAN_ACTIVATE) == 0,
        "An unset callback must not be called indirectly");
  instances[0][legacy].internal_card_id = SHANDALAR_DAMAGE_CARD_IID + 16; // Nafs Asp's original effect
  instances[0][legacy].info_slot = 10000;
  check(legacy_effect_activated(0, legacy, EVENT_CAN_ACTIVATE) == 1 && callback_calls == 1,
        "The original Asp effect must retain its mana ability without treating data as a callback");
  return failures ? 1 : 0;
}
