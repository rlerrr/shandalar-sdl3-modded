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

int main(void)
{
  char title[52];
  char modifiers[400];
  instance.internal_card_id = LEGACY_EFFECT_CUSTOM;
  instance.info_slot = (int)callback;
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
  return failures ? 1 : 0;
}
