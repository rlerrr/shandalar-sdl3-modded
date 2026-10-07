"""Inventory declared engine globals, not extracted gameplay implementations."""
import re
import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
names = []
for header, marker in [("global_state.h", "GLOBAL_STATE_EXTERN"), ("global_other.h", "GLOBAL_OTHERS_EXTERN")]:
    for declaration in re.findall(r"^" + marker + r"\s+([^;]+);", (root / "magic/src" / header).read_text(), re.M):
        declaration = declaration.split("=")[0].strip()
        if re.search(r"CRITICAL_SECTION|\bH[A-Z]+\b|SDL_|BITMAP|\bDIBSurface\b", declaration):
            continue  # Native presentation objects are never copied or reset.
        pointer = re.search(r"\*\s*(\w+)\s*\)", declaration)
        variable = re.search(r"(\w+)\s*(?:\[[^]]+\])*\s*$", declaration)
        name = pointer.group(1) if pointer else variable.group(1) if variable else None
        if not name:
            raise ValueError(f"Unrecognized global declaration: {declaration}")
        if name not in names:
            names.append(name)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("/* Engine globals; platform synchronization/presentation handles excluded. */\n" +
                  "".join(f"DUEL_GLOBAL({name})\n" for name in names))
