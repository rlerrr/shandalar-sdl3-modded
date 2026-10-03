"""Compile actual campaign selection functions and imported records for tests."""
import re
import sys
from pathlib import Path

FUNCTIONS = {
    "shandalar/src/adventure_new_game.c": ["LoadCardRaritiesAndCsvOffsets", "InitializeNewGameState",
        "PickRandomColorBitExcludingMask", "AddRandomStarterDeckCards",
        "PickRandomCardMatchingTypeAndColor", "GetRemainingAllowedCardCopies",
        "RebuildDeckEntriesByCardGroup"],
    "shandalar/src/shandalar.c": ["FindCardIndexByCsvid", "IsCardColorCompatibleWithMask"],
    "magic/src/duel.c": ["AddCardToDeckSorted"],
    "magic/src/duel_card_state.c": ["single_color_test_bit_to_color_t"],
    "magic/src/duel_engine_main_wndproc.c": ["GetCardRarity", "ReadCsvFieldByCsvid"],
    "magic/src/shared_startup.c": ["CardIDFromType"],
    "magic/src/duel_ai_support.c": ["internal_rand"],
}

root, output = map(Path, sys.argv[1:])
definitions = []
for filename, names in FUNCTIONS.items():
    blocks = re.split(r"(?m)^// FUNCTION:.*\n", (root / filename).read_text())
    for name in names:
        pattern = rf"(?m)^(?:unsigned int|int|void|color_t) {name}\([^;{{}}]*\)\s*{{"
        matches = [block for block in blocks if re.search(pattern, block)]
        if len(matches) != 1:
            raise ValueError(f"Expected one definition for {name}, found {len(matches)}")
        block = matches[0]
        match = re.search(pattern, block)
        depth = 0
        for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|[{}]',
                                 block[match.end() - 1:], re.S):
            if token.group() == '{': depth += 1
            elif token.group() == '}':
                depth -= 1
                if depth == 0:
                    definitions.append(block[match.start():match.end() - 1 + token.end()])
                    break
        else:
            raise ValueError(f"Unterminated definition for {name}")
records = (root / "magic/src/carddata.c").read_text().split("card_data_t global_cards_data", 1)[1]
records = "card_data_t global_cards_data" + records
records = re.sub(r", (?:manalinkeh_\w+|card_\w+), (0x[0-9a-fA-F]+u)", r", NULL, \1", records)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include <stdlib.h>\n#include <string.h>\n'
    '#include "shandalar/src/shandalar_internal.h"\n'
    '#include "magic/src/game_support.h"\n'
    '#include "magic/src/global_other.h"\n'
    'int GetCardRarity(int);\nint AddCardToDeckSorted(int);\n'
    'color_t single_color_test_bit_to_color_t(color_test_t);\n'
    + records + '\n' + '\n'.join(definitions))
