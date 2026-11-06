#include "platform/winuser_compat.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"%d: %s\n",__LINE__,#expr); ++failures; } } while (0)
static INT_PTR CALLBACK inspect_dialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
  (void)wparam; (void)lparam;
  if (message == WM_INITDIALOG) {
    char label[16];
    RECT client;
    CHECK(GetClientRect(window, &client));
    CHECK(client.right - client.left == 200);
    CHECK(client.bottom - client.top == 120);
    HWND button = GetDlgItem(window,42);
    CHECK(button != NULL);
    CHECK(GetWindowTextA(button,label,sizeof(label)) == 2);
    CHECK(strcmp(label,"OK") == 0);
    CHECK(GetParent(button) == window);
    CHECK(GetWindowLongA(button,GWL_STYLE) & WS_CHILD);
    {
      HWND combo = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE,
                                  0, 0, 80, 20, window, (HMENU)49, NULL, NULL);
      CHECK(combo);
      CHECK(SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)"booster") == 0);
      CHECK(SendMessageA(combo, CB_ADDSTRING, 0, (LPARAM)"starter") == 1);
      CHECK(SendMessageA(combo, CB_SETITEMDATA, 0, 7) == 0);
      CHECK(SendMessageA(combo, CB_SETITEMDATA, 1, 0x10007) == 0);
      CHECK(SendMessageA(combo, CB_SETCURSEL, 1, 0) == 1);
      CHECK(SendMessageA(combo, CB_GETITEMDATA,
                         SendMessageA(combo, CB_GETCURSEL, 0, 0), 0) == 0x10007);
      CHECK(SendMessageA(combo, CB_GETITEMDATA, 0, 0) == 7);
      CHECK(SendMessageA(combo, CB_GETITEMDATA, (WPARAM)-1, 0) == -1);
      CHECK(SendMessageA(combo, CB_SETITEMDATA, 2, 7) == -1);
    }
    EndDialog(window,88);
    return TRUE;
  }
  return FALSE;
}
int main(void)
{
  /* An aligned standard template with one ordinal-class BUTTON control. */
  static const union { DWORD alignment; WORD words[27]; } templ = { .words = {
    0,0x90c8,0,0,1,0,0,100,60,0,0,0,
    0,0x5001,0,0,5,7,30,12,42,0xffff,0x80,'O','K',0,0
  }};
  HWND parent = shandalar_win32_create_dialog(NULL,NULL,"parent",0,0,200,100);
  CHECK(parent != NULL);
  CHECK(DialogBoxIndirectParamA(NULL,(LPCDLGTEMPLATE)templ.words,parent,inspect_dialog,0)==88);
  CHECK(IsWindowEnabled(parent));
  CHECK(GetWindow(parent,GW_CHILD)==NULL);
  CHECK(DestroyWindow(parent));
  shandalar_win32_reset();
  return failures ? 1 : 0;
}
