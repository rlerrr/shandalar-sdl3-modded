#pragma once

// Declarative test data shared by the runner and bounded generated data shards.
struct Setup { const char *ref; int player, zone, csvid, state; };
struct Fixture { const char *name; const Setup *cards; int count, turn, combat; int life[2]; int mana[2][6]; };
struct Step { const char *op, *ref, *other, *property; int player, value, zone, index, color; };
struct Case { const char *name, *card, *oracle_id, *oracle_text, *fixture; unsigned seed; const Setup *cards; int count; const Step *steps; int step_count; };
struct Template { const char *name; int csvid, power, toughness; const char *oracle_id; };
struct Smoke { const char *name; int csvid; };
