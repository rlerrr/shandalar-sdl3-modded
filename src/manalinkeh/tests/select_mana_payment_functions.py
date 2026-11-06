"""Exercise the actual imported payment loop and human autotap policy."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
engine = (root / 'manalinkeh/functions/engine.c').read_text(encoding='utf-8')
start = engine.index('#define payment_amt_to_apply_to_cost')
end = engine.index('#endif', start)
parts = [engine[start:end]]
for filename, names in [('magic/src/duel_mana.c', ['mana_source_is_on_stack']),
                        ('manalinkeh/functions/ai.c', ['human_autotap_for_mana']),
                        ('manalinkeh/functions/engine.c', ['charge_mana'])]:
    text = (root / filename).read_text(encoding='utf-8')
    for name in names:
        start = re.search(rf'(?m)^(?:static )?(?:int|void) {name}\([^;]*?\)\s*\{{', text)
        if start is None:
            raise ValueError(name)
        end = re.search(r'(?m)^}', text[start.end():])
        parts.append(text[start.start():start.end() + end.end()])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include "manalink.h"\n#include "magic/src/duel_host.h"\n' +
                  '#define PLAYER_CARD_INSTANCE(p,c) global_card_instances[p][c]\n' +
                  'int pay_mana_maximally_satisfied(int*, int, int);\n' +
                  'int try_to_pay_for_mana_by_autotapping(int, int*, int*, int, int);\n' +
                  'void event_activate_then_duplicate_into_stack(int,int,int,int,int);\n' +
                  '\n'.join(parts) + '\n', encoding='utf-8')
