"""Compile the imported legacy text entry points without their large source TU."""
import re
import sys
from pathlib import Path

source, output = map(Path, sys.argv[1:])
text = source.read_text(encoding="utf-8")
definitions = []
for name in ("legacy_name_or_text", "legacy_name", "effect_card_text_replace_pipe_n"):
    starts = list(re.finditer(rf"(?m)^void {name}\([^\n]+\)[^\n]*\n\{{\n", text))
    if len(starts) != 1:
        raise ValueError(f"Expected one definition of {name}, got {len(starts)}")
    start = starts[0].start()
    # These functions indent all inner closing braces; their final brace is
    # at column zero. Keep the definition, including conditional host code.
    end = re.search(r"(?m)^}\n", text[starts[0].end():])
    if end is None:
        raise ValueError(f"Missing closing brace of {name}")
    definitions.append(text[start:starts[0].end() + end.end()])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include "manalink.h"\n' + "\n".join(definitions), encoding="utf-8")
