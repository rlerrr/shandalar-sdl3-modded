"""Compile actual campaign selection functions and imported records for tests."""
import re
import sys
from pathlib import Path

FUNCTIONS = {
    "shandalar/src/adventure_new_game.c": ["LoadCardRaritiesAndCsvOffsets", "InitializeNewGameState",
        "PickRandomColorBitExcludingMask", "PickConfiguredStarterCard", "AddConfiguredStarterDeckCards", "AddRandomStarterDeckCards",
        "PickRandomCardMatchingTypeAndColor", "GetRemainingAllowedCardCopies",
        "RebuildDeckEntriesByCardGroup"],
    "shandalar/src/shandalar.c": ["FindCardIndexByCsvid", "IsCardColorCompatibleWithMask"],
    "shandalar/src/visitLocation.c": ["PickConfiguredTownShopCard", "ParseDeckFileIntoInitialLibrary",
        "CalculateCardShopPrice"],
    "magic/src/duel_stack.c": ["ClampIntToRange"],
    "magic/src/duel.c": ["AddCardToDeckSorted"],
    "magic/src/duel_card_state.c": ["single_color_test_bit_to_color_t"],
    "magic/src/duel_engine_main_wndproc.c": ["GetCardRarity", "ReadCsvFieldByCsvid"],
    "magic/src/shared_startup.c": ["CardIDFromType", "CopyRawCardNamesAndRarities"],
    "magic/src/duel_ai_support.c": ["internal_rand", "DrawRandomCardFromInitialLibrary"],
    "magic/src/cardlist.c": ["show_cardlist"],
}

root, output = map(Path, sys.argv[1:])
definitions = []
for filename, names in FUNCTIONS.items():
    blocks = re.split(r"(?m)^// FUNCTION:.*\n", (root / filename).read_text())
    for name in names:
        pattern = rf"(?m)^(?:static )?(?:unsigned int|int|void|color_t) {name}\([^;{{}}]*\)\s*{{"
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
                    definition = re.sub(r"^static ", "", block[match.start():match.end() - 1 + token.end()])
                    if name == "show_cardlist":
                        definition = definition.replace("DialogBoxParamA(", "campaign_test_cardlist_dialog(")
                    definitions.append(definition)
                    break
        else:
            raise ValueError(f"Unterminated definition for {name}")
records = (root / "magic/src/carddata.c").read_text().split("card_data_t global_cards_data", 1)[1]
records = "card_data_t global_cards_data" + records
records = re.sub(r", (manalinkeh_\w+|card_\w+), (0x[0-9a-fA-F]+u)",
                 lambda m: f', {"card_dummy" if m[1] == "card_dummy" else "campaign_test_handler"}, {m[2]}', records)
output.parent.mkdir(parents=True, exist_ok=True)
cardlist_source = (root / "magic/src/cardlist.c").read_text()
cardlist_context = re.search(r"(?s)typedef struct show_card_list_dialog_context_struct.*?show_card_list_dialog_context_t;",
                             cardlist_source).group()
output.write_text('#include <stdlib.h>\n#include <string.h>\n'
    '#include "shandalar/src/shandalar_internal.h"\n'
    '#include "magic/src/game_support.h"\n'
    '#include "magic/src/global_other.h"\n'
    '#include "deckdll/src/card_db.h"\n'
    'int GetCardRarity(int);\nint AddCardToDeckSorted(int);\n'
    'extern char g_last_parsed_deck_path[];\n'
    'extern int global_available_slots;\nint CardTypeFromID(int);\n'
    'int card_dummy(int, int, event_t);\nint campaign_test_handler(int, int, event_t);\n'
    'color_t single_color_test_bit_to_color_t(color_test_t);\n'
    'extern int g_showlist_card_window_extra_bytes;\nextern HWND g_duel_window_hwnd;\n'
    'LRESULT CALLBACK wndproc_ShowListCard(HWND, UINT, WPARAM, LPARAM);\n'
    'INT_PTR CALLBACK dlgfunc_show_deck(HWND, UINT, WPARAM, LPARAM);\n'
    + cardlist_context + '\n'
    'extern int campaign_test_cardlist_csvids[500], campaign_test_cardlist_count;\n'
    'INT_PTR campaign_test_cardlist_dialog(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param)\n'
    '{\n  show_card_list_dialog_context_t *context = (show_card_list_dialog_context_t *)param;\n'
    '  (void)instance; (void)name; (void)parent; (void)proc;\n'
    '  campaign_test_cardlist_count = context->item_count;\n'
    '  memcpy(campaign_test_cardlist_csvids, context->displayed_csvids, context->item_count * sizeof(int));\n'
    '  return 7;\n}\n'
    + records + '\n' + '\n'.join(definitions))
