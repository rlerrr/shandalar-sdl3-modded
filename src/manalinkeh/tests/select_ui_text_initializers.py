"""Compile the real Manalink UI text initializers in the small loader harness."""
import re
import sys
from pathlib import Path

windows, subtypes, output = map(Path, sys.argv[1:])
ui_source = windows.read_text(encoding="utf-8")
type_source = subtypes.read_text(encoding="utf-8")

def function(source, name):
    starts = list(re.finditer(rf"(?m)^void {name}\([^\n]+\)\n\{{\n", source))
    if len(starts) != 1:
        raise ValueError(f"Expected one definition of {name}, got {len(starts)}")
    match = starts[0]
    end = re.search(r"(?m)^}\n", source[match.end():])
    if end is None:
        raise ValueError(f"Missing closing brace of {name}")
    return source[match.start():match.end() + end.end()]

names = re.search(r"(?m)^static char\* ability_tooltip_names.*\nchar\* counter_names.*\n", ui_source)
arrays = re.findall(r"(?m)^static char\* strs_\w+\[[^\n]+\] = \{0\};", type_source)
assert names and len(arrays) == 9
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include "manalink.h"\n' + names.group() + "\n".join(arrays) + "\n" +
                  function(type_source, "init_subtype_text") + "\n" +
                  function(ui_source, "initialize_ability_tooltip_names").replace(
                      'void initialize_ability_tooltip_names(', 'void manalinkeh_initialize_ability_tooltip_names(') +
                  '\nconst char *mok_test_ability_name(int icon) { return ability_tooltip_names[icon]; }\n'
                  'const char *mok_test_creature_type_name(int index) { return strs_creature_type[index]; }\n',
                  encoding="utf-8")
