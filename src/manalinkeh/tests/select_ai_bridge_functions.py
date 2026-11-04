"""Compile imported blocker, target replay, and trigger dispatch routines."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
definitions = []
for filename, names in [('engine.c', ['ai_assign_blockers']),
                        ('targets.c', ['replay_ai_target_choice', 'choose_ai_target']),
                        ('events.c', ['dispatch_trigger_impl', 'dispatch_trigger']),
                        ('deck.c', ['initialize_card_extensions', 'memcpy_inst_and_ext']),
                        ('windows.c', ['copy_to_display_start', 'copy_to_display_end'])]:
    source = (root / filename).read_text(encoding="utf-8")
    for name in names:
        start = re.search(rf'(?m)^(?:static )?(?:int|void) {name}\([^;]*?\)\s*\{{', source)
        if not start:
            raise ValueError(name)
        end = re.search(r'(?m)^}', source[start.end():])
        definitions.append(source[start.start():start.end() + end.end()])
deck = (root / 'deck.c').read_text(encoding="utf-8")
extensions = re.search(r'(?ms)^card_ext_t card_ext_buffer1;.*?^    \.special_flags3 = 0};', deck).group()
engine = (root / 'engine.c').read_text(encoding="utf-8")
state_names = ['card_flags', 'your_spells_cant_be_countered_by_color', 'mana_colors_added',
               'mana_color_override', 'mana_quantity_override', 'multi_mana_transmutation',
               'add_to_die_rolls', 'roll_extra_dice', 'card_counts']
state = '\n'.join(re.search(rf'(?m)^(?:int|uint32_t|uint8_t) {name}[^;]*;', engine).group()
                  for name in state_names)
definitions.insert(0, extensions + '\n' + state)
definitions.append((root / 'static_host.c').read_text(encoding="utf-8"))
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#undef SHANDALAR\n#include "manalink.h"\n'
                  '#define initialize_card_extensions manalinkeh_initialize_card_extensions\n'
                  '#define memcpy_inst_and_ext manalinkeh_memcpy_inst_and_ext\n'
                  '#define copy_to_display_start manalinkeh_copy_to_display_start\n'
                  '#define copy_to_display_end manalinkeh_copy_to_display_end\n'
                  'void ai_assign_one_blocker(int, int);\nvoid enforce_minimum_blockers(void);\n'
                  '#define critical_section_for_display g_duel_render_lock\nextern CRITICAL_SECTION g_duel_render_lock;\n'
                  'int sub_498F20(void);\nint sub_499050(void);\n'
                  'void human_assign_blockers(int player);\n'
                  'extern int g_shandalar_difficulty;\n'
                  'xtrigger_t xtrigger_impl_value_dont_use_directly;\n'
                  'char gs_prompt_turnsequence_0[1], gs_prompt_blockerselection_0[1];\n'
                  + '\n'.join(definitions) + '\n'
                  'int mok_test_choose_ai_target(const target_t* c, int n) '
                  '{ return choose_ai_target(c, n); }\n')
