#include "magic/src/global_strings.h"
#include <stdio.h>
#include <string.h>

char global_base_directory[0x105] = ".";
int g_duel_ai_mode_state;

static int check(int condition, const char *message)
{
  if (!condition)
    fprintf(stderr, "%s\n", message);
  return condition;
}

int main(void)
{
  const char *fixture = "global_strings_test_fixture.txt";
  FILE *file = fopen(fixture, "wt");
  int passed;
  if (!file)
    return 1;
  fputs("@PROMPT_PROC1\n2\n%s processes...\n%s processes...\\nX is %d.\n", file);
  fclose(file);
  strcpy(global_ui_strings_filename, fixture);

  passed = check(load_text(NULL, "PROMPT_PROC1") == 2,
                 "NULL filename must select the default UI strings file");
  passed &= check(strcmp(g_text_lines[0], "%s processes...") == 0,
                  "Trigger prompt must come from the selected section");
  passed &= check(LoadTextSectionLines(NULL, "PROMPT_PROC1") == 2 &&
                      strcmp(g_text_lines[1], "%s processes...\nX is %d.") == 0,
                  "Default-file wrapper must still expand escaped newlines");
  passed &= check(load_text(fixture, "PROMPT_PROC1") == 2,
                  "Explicit filenames must remain supported");
  passed &= check(load_text(NULL, "MISSING_SECTION") == -1,
                  "Missing default-file sections must return an error");
  remove(fixture);
  return passed ? 0 : 1;
}
