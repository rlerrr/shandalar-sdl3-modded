"""Exercise real pump creation, dispatch, and cleanup without the game's UI."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
source = (root / 'manalinkeh/functions/functions.c').read_text(encoding='utf-8')
names = ('effect_pump_until_eot', 'pump_until_eot', 'pump_until_eot_merge_previous',
         'legacy_effect_custom', 'legacy_effect_activated',
         'create_targetted_legacy_effect', 'create_targetted_legacy_activate',
         'alternate_legacy_text', 'generic_shade_merge_pt')
definitions = []
for name in names:
    start = re.search(rf'(?m)^(?:static )?int {name}\([^;]*?\)\s*{{', source)
    if not start:
        raise ValueError(name)
    end = re.search(r'(?m)^}', source[start.end():])
    definitions.append(source[start.start():start.end() + end.end()])
card = (root / 'manalinkeh/cards/fifth_dawn.c').read_text(encoding='utf-8')
start = re.search(r'(?m)^int card_furnace_whelp\([^;]*?\)\s*{', card)
end = re.search(r'(?m)^}', card[start.end():])
definitions.append(card[start.start():start.end() + end.end()])
bridge = (root / 'shandalar/src/manalinkeh_host_bridge.c').read_text(encoding='utf-8')
for name in ('LEGACY_EFFECT_CUSTOM', 'LEGACY_EFFECT_ACTIVATED'):
    definitions.insert(0, re.search(rf'(?m)^int {name}(?: = [^;]+)?;', bridge).group())
# Use the production record identities and handlers, not hand-written test IDs.
table = (root / 'magic/src/carddata.c').read_text(encoding='utf-8')
records = []
for name in ('Custom Legacy', 'Activated Legacy'):
    line = re.search(rf'(?m)^  {{ 0, "{name}".*$', table).group()
    iid = re.search(r'/\* (\d+):', line)[1]
    csvid = re.search(r'\{0\}, (\d+),', line)[1]
    handler = re.search(r'(manalinkeh_legacy_effect_\w+),', line)[1].removeprefix('manalinkeh_')
    records.append(f'[{iid}] = {{.id = {csvid}, .type = TYPE_EFFECT, .code_pointer = {handler}}}')
definitions.append('card_data_t global_cards_data[SHANDALAR_CARD_DATA_COUNT] = {\n'
                   + ',\n'.join(records) + '\n};')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include "manalink.h"\n#include "magic/src/carddata_generated.h"\n'
                  + '\n\n'.join(definitions), encoding='utf-8')
