/* Run this under Node/WASM as well as native: x86 tolerates a wrong arity. */
#include "defs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ASSERT(condition) assert(condition)

static card_instance_t instances[2][4];
static int damage_card = 1, activation_card = 2;
static int dmg_source_color;
static int source_color, source_in_play, source_is_creature, source_attacks_planeswalker;

static card_instance_t* get_card_instance(int player, int card)
{ return &instances[player][card]; }
static card_instance_t* in_play(int player, int card)
{ return player >= 0 && card >= 0 && source_in_play ? get_card_instance(player, card) : NULL; }
static int get_color(int player, int card)
{ (void)player; (void)card; return source_color; }
static int is_what(int player, int card, int type)
{ (void)player; (void)card; (void)type; return source_is_creature; }
static int check_special_flags(int player, int card, int flags)
{ (void)player; (void)card; (void)flags; return source_attacks_planeswalker; }

#include "target_predicate_functions.inc"

static int seen[5];
static const char* source_sensitive(int chooser, int player, int card, int source_player, int source_card)
{
  seen[0] = chooser; seen[1] = player; seen[2] = card;
  seen[3] = source_player; seen[4] = source_card;
  return source_player == 1 && source_card == 3 ? NULL : "wrong source";
}

static int failures;
static void check(int condition, const char* message)
{
  if (!condition) { fprintf(stderr, "%s\n", message); ++failures; }
}

int main(void)
{
  card_instance_t* damage = get_card_instance(1, 1);
  /* Keep a real indirect call even in optimized WASM builds. */
  volatile int extra = target_predicate_extra(colored_damage_source);
  memset(instances, 0, sizeof(instances));
  damage->internal_card_id = damage_card;
  damage->damage_target_player = 0;
  damage->damage_target_card = -1;
  damage->damage_source_player = 1;
  damage->damage_source_card = 0;
  damage->initial_color = COLOR_TEST_BLUE;
  dmg_source_color = COLOR_TEST_BLUE;
  check(!invoke_target_predicate(extra, 0, 1, 1, 0, 2),
        "CoP Blue must accept blue damage to its controller through the five-argument indirect call");
  damage->initial_color = COLOR_TEST_RED;
  check(invoke_target_predicate(extra, 0, 1, 1, 0, 2) != NULL,
        "CoP Blue must reject red damage");
  source_in_play = 1;
  source_color = COLOR_TEST_BLUE;
  check(!invoke_target_predicate(extra, 0, 1, 1, 0, 2),
        "CoP must use a live source's color");
  damage->damage_target_player = 1;
  check(invoke_target_predicate(extra, 0, 1, 1, 0, 2) != NULL,
        "CoP must reject damage to the other player");
  damage->damage_target_player = 0;
  damage->damage_target_card = 3;
  check(invoke_target_predicate(extra, 0, 1, 1, 0, 2) != NULL,
        "CoP must reject damage to a creature");
  damage->damage_target_card = -1;
  source_is_creature = source_attacks_planeswalker = 1;
  check(invoke_target_predicate(extra, 0, 1, 1, 0, 2) != NULL,
        "CoP must reject damage assigned to a planeswalker");
  damage->internal_card_id = 3;
  check(invoke_target_predicate(extra, 0, 1, 1, 0, 2) != NULL,
        "CoP must reject a non-damage card");

  extra = target_predicate_extra(source_sensitive);
  instances[0][2].internal_card_id = activation_card;
  instances[0][2].parent_controller = 1;
  instances[0][2].parent_card = 3;
  check(!invoke_target_predicate(extra, 0, 1, 1, 0, 2) && seen[0] == 0 &&
        seen[1] == 1 && seen[2] == 1 && seen[3] == 1 && seen[4] == 3,
        "Activation predicates must receive the chooser, target, and original source in order");
  check(!invoke_target_predicate(extra, 0, 1, 1, 1, 3),
        "Non-activation predicates must retain their source");
  check(invoke_target_predicate(extra, 0, 1, 1, -1, -1) != NULL &&
        seen[3] == -1 && seen[4] == -1,
        "Source-less validation must retain its sentinel");
  return failures ? 1 : 0;
}
