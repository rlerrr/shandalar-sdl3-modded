"""Exercise the production indirect call and CoP predicate without game assets."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
definitions = []
for filename, name in [('functions/targets.c', 'invoke_target_predicate'),
                       ('cards/unlimited.c', 'colored_damage_source')]:
    source = (root / filename).read_text(encoding='utf-8')
    start = re.search(rf'(?m)^static const char\*\s*{name}\([^;]*?\)\s*\{{', source)
    if not start:
        raise ValueError(name)
    end = re.search(r'(?m)^}', source[start.end():])
    definitions.append(source[start.start():start.end() + end.end()])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('\n'.join(definitions) + '\n', encoding='utf-8')
