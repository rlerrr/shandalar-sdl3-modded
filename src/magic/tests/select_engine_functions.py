"""Isolate production engine definitions for the asset-free C test harness.

The legacy translation units include unrelated UI/card callbacks that prevent
linking a small harness, even with function sections. Their FUNCTION annotations
provide boundaries so we can compile the selected definitions without copying
their implementations into the tests. Fail if a definition is missing/ambiguous.
"""

import re
import sys
from pathlib import Path

FUNCTIONS = {
    "duel.c": ["untap_phase_exe", "rebuild_battlefield_summary", "reset_duel_globals",
               "end_turn_phase"],
    "duel_card_state.c": ["is_in_play", "add_card_to_hand", "initialize_card_instance",
                          "move_card_to_exile", "process_killed_card",
                          "regenerate_or_graveyard_triggers"],
    "duel_stack.c": ["resolve_top_card_on_stack", "push_affected_card_stack",
                     "pop_affected_card_stack", "choose_response_card",
                     "get_card_response_action_type"],
    "duel_ai.c": ["cleanup_combat_state", "save_ai_search_state", "restore_ai_search_state",
                  "save_combat_simulation_state", "restore_combat_simulation_state",
                  "enumerate_ai_block_assignments", "record_ai_action_selection",
                  "replay_ai_action_selection"],
    "dialog.c": ["refresh_duel_display_cache", "displayed_card_indices_invalid",
                 "get_displayed_card_blocking"],
    "duel_engine_lifeclass_wndproc.c": ["is_invalid_duel_player", "get_displayed_player_counters"],
    "global_state.c": ["save_or_load_ver1", "save_or_load_ver2"],
    "duel_main_phase.c": ["mark_blocked_attackers"],
    "duel_main_phase_combat_damage.c": ["is_combat_damage_ability_in_step",
                                       "assign_blocker_combat_damage"],
}


def main():
    source_root, output = map(Path, sys.argv[1:])
    definitions = []
    for filename, names in FUNCTIONS.items():
        text = (source_root / filename).read_text(encoding="utf-8")
        blocks = re.split(r"(?m)^// FUNCTION:.*\n", text)
        for name in names:
            pattern = rf"(?m)^(?:unsigned int|int|void) {name}\([^;{{}}]*\)\s*(?:#(?:else|endif)[^\n]*\n)?\s*{{"
            matches = [block for block in blocks if re.search(pattern, block)]
            if len(matches) != 1:
                raise ValueError(f"Expected one definition for {name} in {filename}, got {len(matches)}")
            definitions.append(matches[0])
    ai_source = (source_root / "duel_ai.c").read_text(encoding="utf-8")
    backup_type = re.search(r"(?ms)^typedef struct\n\{.*?^} AiSearchBackup;", ai_source).group()
    backup_vars = re.findall(r"(?m)^static [^\n;]*\b(?:g_ai_search_backup|combat_sim_saved_\w+)[^\n;]*;", ai_source)
    dialog_source = (source_root / "dialog.c").read_text(encoding="utf-8")
    cache_vars = re.findall(r"(?m)^(?:int|dialog_stack_entry_t) g_duel_cached_\w+(?:\[[^\n]+\])?;", dialog_source)
    cache_type = re.search(r"(?ms)^typedef struct\n\{[^}]*^} dialog_stack_entry_t;", dialog_source).group()
    definitions.insert(0, cache_type + "\n" + backup_type + "\n" + "\n".join(backup_vars + cache_vars))
    combat_source = (source_root / "duel_main_phase_combat_damage.c").read_text(encoding="utf-8")
    combat_vars = re.findall(r"(?m)^int combat_damage_\w+(?:\[[^\n]+\])?;", combat_source)
    combat_flags = re.findall(r"(?m)^#define COMBAT_DAMAGE_FLAG_\w+[^\n]*", combat_source)
    definitions.insert(0, "\n".join(combat_vars + combat_flags))
    ai_globals = ["ai_block_damage_orders", "g_ai_combat_blocking_card_indices",
                  "ai_blocker_player", "g_ai_attacker_count", "g_ai_current_blocker",
                  "g_ai_block_assignment_count", "g_ai_combat_best_score",
                  "g_ai_attacker_power", "ai_candidate_block_assignments",
                  "ai_attacker_blocker_counts"]
    declarations = []
    for name in ai_globals:
        declarations.append(re.search(rf"(?m)^int {name}(?:\[[^\n]+\])?;", ai_source).group())
    definitions.insert(0, "\n".join(declarations))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('#include <stdio.h>\n#include "magic/src/game_support.h"\n'
                      '#include "magic/src/duel_host.h"\n'
                      '#include "magic/src/global_strings.h"\n'
                      'extern int ai_blocker_count;\nextern int ai_blocker_cards[16];\n'
                      'extern int g_ai_response_state;\nextern int g_duel_action_pending;\n'
                      'extern int g_saved_stop_phase_player, g_response_prompt_enabled, g_duel_result;\n'
                      'int has_damage_card_targeting(int player, int card);\n'
                      'int score_current_ai_block_assignment(void);\n'
                      'void FUN_00464bb0(void);\nvoid refresh_ai_random_table(void);\n'
                      'int save_or_load_data(void *, unsigned int);\n'
                      'void __cdecl _assert(void *, void *, unsigned);\n'
                      'void update_phase_display(int player, phase_t phase);\n'
                      + "\n".join(definitions), encoding="utf-8")


if __name__ == "__main__":
    main()
