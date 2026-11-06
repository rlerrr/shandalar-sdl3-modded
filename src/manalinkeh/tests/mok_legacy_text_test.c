#include "manalink.h"
void effect_card_text_replace_pipe_n(char *text, int power, int toughness);

int LEGACY_EFFECT_CUSTOM = 2;
int LEGACY_EFFECT_ACTIVATED = 3;
char *set_legacy_effect_name_addr;
static card_instance_t instance;
static char text_buffer[400];
static int last_event;
static int failures;

void backtrace_and_abort(const char *message)
{
  fprintf(stderr, "%s\n", message);
  abort();
}

static void check(int condition, const char *message)
{
  if (!condition)
  {
    fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

card_instance_t *get_displayed_card_instance(int player, int card)
{
  check(player == 1 && card == 4, "Naming must use the supplied displayed instance");
  return &instance;
}

int shandalar_is_legacy_effect_text_buffer(const char *text)
{
  return text == text_buffer;
}

static int callback(int player, int card, event_t event)
{
  check(player == 1 && card == 4, "Naming callback must receive the displayed card");
  last_event = event;
  memset(set_legacy_effect_name_addr, 'X', event == EVENT_SET_LEGACY_EFFECT_TEXT ? 400 : 52);
  return 0;
}

static int dispatch_callback(int player, int card, event_t event)
{
  check(player == 1 && card == 4, "Legacy dispatch must preserve player and card");
  last_event = event;
  return 23;
}

card_instance_t *get_card_instance(int player, int card)
{
  check(player == 1 && card == 4, "Legacy dispatch must use the requested instance");
  return &instance;
}

int call_card_fn(CardFunction handler, card_instance_t *actual_instance,
                 int player, int card, event_t event)
{
  check(actual_instance == &instance, "Legacy dispatch must preserve its instance");
  check(handler == dispatch_callback, "Legacy dispatch truncated the callback address");
  return handler == dispatch_callback ? handler(player, card, event) : -1;
}

int main(void)
{
  char title[52];
  char modifiers[400];
  instance.internal_card_id = LEGACY_EFFECT_CUSTOM;
  instance.info_slot = (intptr_t)callback;
  instance.eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME | EVENT_SET_LEGACY_EFFECT_TEXT;
  legacy_name(title, 1, 4);
  check(last_event == EVENT_SET_LEGACY_EFFECT_NAME && strlen(title) == 51,
        "A title callback must receive the name event and terminate at 51 characters");
  legacy_name(text_buffer, 1, 4);
  check(last_event == EVENT_SET_LEGACY_EFFECT_TEXT && strlen(text_buffer) == 399,
        "The relocated rules buffer must receive the text event and preserve text past 51 characters");
  instance.eot_toughness = 2;
  strcpy(title, "Default|#1First|#2Second|#3Third");
  legacy_name(title, 1, 4);
  check(strcmp(title, "Second") == 0, "Numbered legacy alternatives must still work");
  instance.eot_toughness = EVENT_SET_LEGACY_EFFECT_NAME;
  instance.internal_card_id = 7;
  strcpy(title, "Ordinary effect");
  last_event = 0;
  legacy_name(title, 1, 4);
  check(last_event == 0 && strcmp(title, "Ordinary effect") == 0,
        "A noncustom effect must not dispatch a naming callback");

  strcpy(modifiers, "+|n/+|n, then +|n/+|n; amount |n.");
  effect_card_text_replace_pipe_n(modifiers, 2, 3);
  check(strcmp(modifiers, "+2/+3, then +2/+3; amount 3.") == 0,
        "All modifier and standalone placeholders must be replaced");
  strcpy(modifiers, "Modifier -|n/-|n");
  effect_card_text_replace_pipe_n(modifiers, -2, 0);
  check(strcmp(modifiers, "Modifier -2/-0") == 0,
        "Negative modifiers must preserve the signed zero for toughness");
  strcpy(modifiers, "Modifier -|n/-|n");
  effect_card_text_replace_pipe_n(modifiers, 0, -3);
  check(strcmp(modifiers, "Modifier -0/-3") == 0,
        "Negative modifiers must preserve the signed zero for power");
  instance.info_slot = (intptr_t)dispatch_callback;
  if (sizeof(void *) > 4)
    check((uintptr_t)dispatch_callback > UINT32_MAX,
          "The x64 regression must exercise a callback above the 32-bit address range");
  last_event = 0;
  check(legacy_effect_custom(1, 4, EVENT_STATIC_EFFECTS) == 23 &&
        last_event == EVENT_STATIC_EFFECTS,
        "Custom legacy effects must call the complete stored address and return its result");
  instance.targets[1].player = 91;
  instance.targets[3].card = 73;
  target_store_pointer(&instance.targets[2], (intptr_t)dispatch_callback);
  check(target_load_pointer(&instance.targets[2]) == (intptr_t)dispatch_callback &&
        instance.targets[1].player == 91 && instance.targets[3].card == 73,
        "Native metadata pointers must preserve every address bit and neighboring targets");
  target_store_pointer(&instance.targets[2], -1);
  check(target_load_pointer(&instance.targets[2]) == -1,
        "Native metadata pointers must retain the empty sentinel");
  instance.info_slot = 0;
  last_event = 0;
  check(legacy_effect_custom(1, 4, EVENT_STATIC_EFFECTS) == 0 && last_event == 0,
        "An empty legacy effect callback must remain a no-op");
  int excluded[] = {11, 22, 33, 44};
  test_definition_t test = {0};
  test.special_selection_function = test_not_in_array;
  test.value_for_special_selection_function = (intptr_t)excluded;
  check(!test.special_selection_function(33, test.value_for_special_selection_function, 0, -1) &&
        test.special_selection_function(55, test.value_for_special_selection_function, 0, -1),
        "Search predicates must preserve a native stack pointer through their context field");
  return failures ? 1 : 0;
}
