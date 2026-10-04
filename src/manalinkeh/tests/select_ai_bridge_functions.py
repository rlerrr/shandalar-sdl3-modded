"""Compile imported blocker, target replay, and trigger dispatch routines."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
definitions = []
for filename, names in [('ai.c', ['ai_assign_blockers']),
                        ('targets.c', ['replay_ai_target_choice', 'choose_ai_target']),
                        ('events.c', ['dispatch_trigger_impl', 'dispatch_trigger'])]:
    source = (root / filename).read_text()
    for name in names:
        start = re.search(rf'(?m)^(?:static )?(?:int|void) {name}\([^;]*?\)\s*\{{', source)
        if not start:
            raise ValueError(name)
        end = re.search(r'(?m)^}', source[start.end():])
        definitions.append(source[start.start():start.end() + end.end()])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#undef SHANDALAR\n#include "manalink.h"\n'
                  'int sub_498F20(void);\nint sub_499050(void);\n'
                  'void human_assign_blockers(int player);\n'
                  'extern int g_shandalar_difficulty;\n'
                  'xtrigger_t xtrigger_impl_value_dont_use_directly;\n'
                  'char gs_prompt_turnsequence_0[1], gs_prompt_blockerselection_0[1];\n'
                  + '\n'.join(definitions) + '\n'
                  'int mok_test_choose_ai_target(const target_t* c, int n) '
                  '{ return choose_ai_target(c, n); }\n')
