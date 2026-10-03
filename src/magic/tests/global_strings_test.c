#include "magic/src/global_strings.h"
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char global_base_directory[0x105] = "global_strings_test_assets";
int g_duel_ai_mode_state;
char ability_tooltip_names_exe[17][50];
extern char *counter_names[];
const char *mok_test_ability_name(int icon);
const char *mok_test_creature_type_name(int index);

void backtrace_and_abort(const char *message)
{
  fprintf(stderr, "Unexpected assertion: %s\n", message);
  abort();
}

static int check(int condition, const char *message)
{
  if (!condition)
    fprintf(stderr, "%s\n", message);
  return condition;
}

static int check_asset_text(const char *directory)
{
  int passed;
  if (strlen(directory) >= sizeof(global_base_directory)) return 1;
  strcpy(global_base_directory, directory);
  strcpy(global_ui_strings_filename, "UIStrings.txt");
  passed = check(load_text(NULL, "ABILITYWORDS") == 75,
                 "Installed Text.res must supply the expanded ability section");
  load_global_ui_strings(global_ui_strings_filename);
  passed &= check(strcmp(mok_test_ability_name(4), "Vigilance") == 0 &&
                  strcmp(mok_test_ability_name(74), "Hexproof from white") == 0 &&
                  strcmp(gs_ability_word_008b4de0[4], "First strike") == 0,
                  "Installed assets must initialize real extended and legacy ability names");
  passed &= check(strcmp(counter_names[141], "Counters") == 0 &&
                  strcmp(mok_test_creature_type_name(252), "Autobot") == 0,
                  "Installed counters and creature subtypes must come from Text.res");
  return passed ? 0 : 1;
}

int main(int argc, char **argv)
{
  static const char *legacy_names[17] = {
    "Flying", "Reach", "Banding", "Trample", "First strike", "Regenerates",
    "Swampwalk", "Islandwalk", "Forestwalk", "Mountainwalk", "Plainswalk",
    "Protection from black", "Protection from blue", "Protection from green",
    "Protection from red", "Protection from white", "Protection from artifacts"
  };
  FILE *file;
  int passed, i;
  if (argc == 2) return check_asset_text(argv[1]);
  _mkdir(global_base_directory);
  remove("global_strings_test_assets/Text.res");
  file = fopen("global_strings_test_assets/UIStrings.txt", "wt");
  if (!file) return 1;
  fputs("@PROMPT_PROC1\n2\n%s processes...\n%s processes...\\nX is %d.\n", file);
  fputs("@ABILITYWORDS\n17\n", file);
  for (i = 0; i < 17; ++i) fprintf(file, "%s\n", legacy_names[i]);
  fputs("@HUNTING_SUBTYPENAMES\n1\nCreature\n", file);
  fputs("@TOO_MANY\n301\n@NO_FINAL_NEWLINE\n1\nLast character", file);
  fclose(file);
  strcpy(global_ui_strings_filename, "UIStrings.txt");

  passed = check(load_text(NULL, "PROMPT_PROC1") == 2,
                 "Default sections must fall back to the legacy UI file");
  passed &= check(LoadTextSectionLines(NULL, "PROMPT_PROC1") == 2 &&
                  strcmp(g_text_lines[1], "%s processes...\nX is %d.") == 0,
                  "Default-file wrapper must expand escaped newlines");
  passed &= check(load_text("UIStrings.txt", "PROMPT_PROC1") == 2,
                  "Explicit filenames must remain supported");
  passed &= check(load_text(NULL, "MISSING_SECTION") == -1 && g_text_lines[0][0] == 0 &&
                  g_text_lines[299][0] == 0,
                  "Missing sections must clear old shared text");
  passed &= check(load_text("UIStrings.txt", "NO_FINAL_NEWLINE") == 1 &&
                  strcmp(g_text_lines[0], "Last character") == 0,
                  "An unterminated final line must retain its last character");
  passed &= check(load_text("UIStrings.txt", "TOO_MANY") == -1 && g_text_lines[0][0] == 0,
                  "Counts larger than the shared buffer must be rejected");

  memset(g_text_lines, 'X', sizeof(g_text_lines));
  for (i = 0; i < 300; ++i) g_text_lines[i][299] = 0;
  load_global_ui_strings(global_ui_strings_filename);
  passed &= check(strcmp(gs_ability_word_008b4de0[4], "First strike") == 0 &&
                  strcmp(gs_ability_word_008b4de0[16], "Protection from artifacts") == 0,
                  "The original 17-name layout must not enter the 75-name initializer");

  file = fopen("global_strings_test_assets/Text.res", "wt");
  if (!file) return 1;
  fputs("@ABILITYWORDS\n75\n", file);
  for (i = 0; i < 75; ++i)
  {
    const char *name = i == 0 ? "Flying" : i == 1 ? "Reach" : i == 4 ? "Vigilance" :
                       i == 5 ? "First strike" : i == 7 ? "Regenerates" :
                       i == 20 ? "Protection from artifacts" : i == 74 ? "Hexproof from white" : NULL;
    if (name) fprintf(file, "%s\n", name);
    else fprintf(file, "Ability %d\n", i);
  }
  fputs("@CUECARD_COUNTERS_SPECIAL\n142\n", file);
  for (i = 0; i < 142; ++i) fprintf(file, "Counter %d\n", i);
  fputs("@CREATURE_TYPE\n254\n", file);
  for (i = 0; i < 254; ++i) fprintf(file, "Creature type %d\n", i);
  fputs("@FULL_BUFFER\n300\n", file);
  for (i = 0; i < 300; ++i) fprintf(file, "Row %d\n", i);
  fclose(file);

  passed &= check(load_text("UIStrings.txt", "ABILITYWORDS") == 17 &&
                  strcmp(g_text_lines[4], "First strike") == 0,
                  "Explicit UI reads must retain the legacy ability order");
  passed &= check(load_text(NULL, "ABILITYWORDS") == 75 &&
                  strcmp(g_text_lines[4], "Vigilance") == 0 &&
                  strcmp(g_text_lines[74], "Hexproof from white") == 0,
                  "Default Manalink reads must prefer the extended Text.res section");
  load_global_ui_strings(global_ui_strings_filename);
  passed &= check(strcmp(gs_ability_word_008b4de0[4], "First strike") == 0 &&
                  strcmp(gs_ability_word_008b4de0[5], "Regenerates") == 0 &&
                  strcmp(gs_ability_word_008b4de0[16], "Protection from artifacts") == 0 &&
                  strcmp(mok_test_ability_name(4), "Vigilance") == 0 &&
                  strcmp(mok_test_ability_name(74), "Hexproof from white") == 0,
                  "Real initialization must map extended abilities to all legacy slots");
  passed &= check(strcmp(counter_names[141], "Counter 141") == 0 &&
                  strcmp(mok_test_creature_type_name(252), "Creature type 252") == 0,
                  "Real counter and subtype initializers must load their full Text.res sections");
  passed &= check(load_text(NULL, "FULL_BUFFER") == 300 && strcmp(g_text_lines[299], "Row 299") == 0,
                  "The loader must accept the full 300-row buffer");
  passed &= check(load_text(NULL, "PROMPT_PROC1") == 2 && g_text_lines[2][0] == 0 &&
                  g_text_lines[299][0] == 0,
                  "A shorter fallback section must discard all old rows");

  remove("global_strings_test_assets/Text.res");
  remove("global_strings_test_assets/UIStrings.txt");
  _rmdir(global_base_directory);
  return passed ? 0 : 1;
}
