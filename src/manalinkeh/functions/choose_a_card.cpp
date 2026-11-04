#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "manalink.h"

#ifndef SHANDALAR
/* October 2022 named-card picker. Native dialog template and message contracts
 * are recorded in docs/recovery-evidence/cpp-chooser-recovery.json. */
enum ChooseACardCtrl {
  CTL_OK = 1, CTL_CANCEL = 2, CTL_FULLCARD = 1030, CTL_LISTBOX = 1031,
  CTL_BLACK = 1032, CTL_BLUE = 1033, CTL_GREEN = 1034, CTL_RED = 1035,
  CTL_WHITE = 1036, CTL_CREATURES = 1037, CTL_ARTIFACTS = 1038,
  CTL_ENCHANTMENTS = 1039, CTL_SORCERIES = 1040, CTL_INSTANTS = 1041,
  CTL_LANDS = 1042, CTL_ALL_COLORS = 1043, CTL_ALL_TYPES = 1044,
  CTL_PLANESWALKERS = 1045, CTL_MULTICOLOR = 1046, CTL_COLORLESS = 1864
};

struct choose_a_card_args_t {
  color_test_t colors;
  type_t types;
  const char *prompt;
  test_definition_t *constraints;
  bool allow_cancel;
  int initial_btn_color;
  int initial_btn_type;
};

typedef std::pair<const char *, csvid_t> StrCsvid;
static std::vector<StrCsvid> choose_a_card_strcsvids;
static int last_btn_color;
static int last_btn_type;

#define main_hinstance g_app_instance
#define hwnd_MainClass g_duel_window_hwnd
#define path_DuelArt global_duelart_path
#define dblbfr_hdc g_shared_offscreen_dc
#define critical_section_for_painting &g_card_render_lock
#define read_graphics_file load_pic
#define set_palette_and_stretch_mode ApplyCardArtPaletteToDc
#define blt_pic DrawBitmapToRect
#define handle_palette_msgs handle_button_palette_message

static const DLGTEMPLATE *generate_choose_a_card_dlg_template()
{
  alignas(4) static unsigned char templ[990] = {
    0xc0, 0x08, 0xc8, 0x90, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6b, 0x01,
    0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x4d, 0x00, 0x53, 0x00, 0x20, 0x00,
    0x53, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x73, 0x00, 0x20, 0x00, 0x53, 0x00, 0x65, 0x00, 0x72, 0x00,
    0x69, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x50, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x07, 0x00, 0xc7, 0x00, 0xf4, 0x00, 0x06, 0x04, 0x4d, 0x00, 0x41, 0x00, 0x47, 0x00,
    0x49, 0x00, 0x43, 0x00, 0x47, 0x00, 0x41, 0x00, 0x4d, 0x00, 0x45, 0x00, 0x5f, 0x00, 0x46, 0x00,
    0x75, 0x00, 0x6c, 0x00, 0x6c, 0x00, 0x43, 0x00, 0x61, 0x00, 0x72, 0x00, 0x64, 0x00, 0x43, 0x00,
    0x6c, 0x00, 0x61, 0x00, 0x73, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0xbf, 0x00, 0x07, 0x00, 0x3e, 0x00, 0x65, 0x00,
    0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x43, 0x00, 0x6f, 0x00, 0x6c, 0x00, 0x6f, 0x00, 0x72, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x03, 0x50, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x11, 0x00,
    0x2b, 0x00, 0x0a, 0x00, 0x13, 0x04, 0xff, 0xff, 0x80, 0x00, 0x41, 0x00, 0x6c, 0x00, 0x6c, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x1c, 0x00,
    0x30, 0x00, 0x0a, 0x00, 0x08, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x42, 0x00, 0x6c, 0x00,
    0x61, 0x00, 0x63, 0x00, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50,
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x27, 0x00, 0x2c, 0x00, 0x0a, 0x00, 0x09, 0x04, 0xff, 0xff,
    0x80, 0x00, 0x42, 0x00, 0x6c, 0x00, 0x26, 0x00, 0x75, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x32, 0x00, 0x31, 0x00, 0x0a, 0x00,
    0x0a, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x47, 0x00, 0x72, 0x00, 0x65, 0x00, 0x65, 0x00,
    0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00,
    0xc3, 0x00, 0x3d, 0x00, 0x2b, 0x00, 0x0a, 0x00, 0x0b, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00,
    0x52, 0x00, 0x65, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50,
    0x00, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x48, 0x00, 0x31, 0x00, 0x0a, 0x00, 0x0c, 0x04, 0xff, 0xff,
    0x80, 0x00, 0x26, 0x00, 0x57, 0x00, 0x68, 0x00, 0x69, 0x00, 0x74, 0x00, 0x65, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x53, 0x00,
    0x31, 0x00, 0x0a, 0x00, 0x48, 0x07, 0xff, 0xff, 0x80, 0x00, 0x43, 0x00, 0x26, 0x00, 0x6f, 0x00,
    0x6c, 0x00, 0x6f, 0x00, 0x72, 0x00, 0x6c, 0x00, 0x65, 0x00, 0x73, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x5e, 0x00,
    0x31, 0x00, 0x0a, 0x00, 0x16, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x4d, 0x00, 0x75, 0x00,
    0x6c, 0x00, 0x74, 0x00, 0x69, 0x00, 0x63, 0x00, 0x6f, 0x00, 0x6c, 0x00, 0x6f, 0x00, 0x72, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x05, 0x01, 0x07, 0x00,
    0x5e, 0x00, 0x65, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x80, 0x00, 0x43, 0x00, 0x61, 0x00, 0x72, 0x00,
    0x64, 0x00, 0x20, 0x00, 0x74, 0x00, 0x79, 0x00, 0x70, 0x00, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x03, 0x50, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x11, 0x00, 0x1d, 0x00, 0x0a, 0x00,
    0x14, 0x04, 0xff, 0xff, 0x80, 0x00, 0x41, 0x00, 0x6c, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x1c, 0x00, 0x2e, 0x00, 0x0a, 0x00,
    0x0d, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x43, 0x00, 0x72, 0x00, 0x65, 0x00, 0x61, 0x00,
    0x74, 0x00, 0x75, 0x00, 0x72, 0x00, 0x65, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x27, 0x00, 0x29, 0x00, 0x0a, 0x00,
    0x0e, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x41, 0x00, 0x72, 0x00, 0x74, 0x00, 0x69, 0x00,
    0x66, 0x00, 0x61, 0x00, 0x63, 0x00, 0x74, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x32, 0x00, 0x3d, 0x00, 0x0a, 0x00,
    0x0f, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x45, 0x00, 0x6e, 0x00, 0x63, 0x00, 0x68, 0x00,
    0x61, 0x00, 0x6e, 0x00, 0x74, 0x00, 0x6d, 0x00, 0x65, 0x00, 0x6e, 0x00, 0x74, 0x00, 0x73, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x3d, 0x00,
    0x2d, 0x00, 0x0a, 0x00, 0x10, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x53, 0x00, 0x6f, 0x00,
    0x72, 0x00, 0x63, 0x00, 0x65, 0x00, 0x72, 0x00, 0x69, 0x00, 0x65, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x48, 0x00,
    0x3d, 0x00, 0x0a, 0x00, 0x11, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x49, 0x00, 0x6e, 0x00,
    0x73, 0x00, 0x74, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x74, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01, 0x53, 0x00, 0x3d, 0x00, 0x0a, 0x00,
    0x15, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00, 0x50, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x6e, 0x00,
    0x65, 0x00, 0x73, 0x00, 0x77, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x6b, 0x00, 0x65, 0x00, 0x72, 0x00,
    0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00,
    0x0a, 0x01, 0x5e, 0x00, 0x20, 0x00, 0x0a, 0x00, 0x12, 0x04, 0xff, 0xff, 0x80, 0x00, 0x26, 0x00,
    0x4c, 0x00, 0x61, 0x00, 0x6e, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x21, 0xa3, 0x50,
    0x00, 0x00, 0x00, 0x00, 0xbf, 0x00, 0x70, 0x00, 0xa4, 0x00, 0x7b, 0x00, 0x07, 0x04, 0xff, 0xff,
    0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00,
    0xd6, 0x00, 0xed, 0x00, 0x32, 0x00, 0x0e, 0x00, 0x01, 0x00, 0xff, 0xff, 0x80, 0x00, 0x4f, 0x00,
    0x4b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x50, 0x00, 0x00, 0x00, 0x00,
    0x23, 0x01, 0xed, 0x00, 0x32, 0x00, 0x0e, 0x00, 0x02, 0x00, 0xff, 0xff, 0x80, 0x00, 0x43, 0x00,
    0x61, 0x00, 0x6e, 0x00, 0x63, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  return reinterpret_cast<const DLGTEMPLATE *>(templ);
}

static int dlgproc_choose_a_card_helper(HWND hwnd, color_test_t want_col,
                                        type_t want_type, test_definition_t *constraints)
{
  choose_a_card_strcsvids.clear();
  SendMessageA(hwnd, LB_RESETCONTENT, 0, 0);
  for (csvid_t csvid(0); csvid.raw < available_slots; ++csvid.raw)
  {
    if (!is_valid_card(csvid.raw))
      continue;
    iid_t iid = iid_t(get_internal_card_id_from_csv_id(csvid.raw));
    type_t typs = get_type(-1, iid.raw);
    if (!(want_type & typs))
      continue;
    color_test_t cols = static_cast<color_test_t>(
        (typs & TYPE_LAND) ? cards_ptr[csvid.raw]->mana_source_colors
                           : get_color_by_internal_id_no_hack(iid.raw));
    if (cols & COLOR_TEST_ARTIFACT)
      cols = static_cast<color_test_t>(cols | COLOR_TEST_COLORLESS);
    cols = static_cast<color_test_t>(cols & COLOR_TEST_ANY);
    if (!cols)
      cols = COLOR_TEST_COLORLESS;
    if (!(want_col & cols))
      continue;
    if ((want_col & 0x100) && num_bits_set(cols & COLOR_TEST_ANY_COLORED) < 2)
      continue;
    if (constraints && !new_make_test(0, iid.raw, -1, constraints))
      continue;
    choose_a_card_strcsvids.push_back(StrCsvid(cards_data[iid.raw].name, csvid));
  }
  std::sort(choose_a_card_strcsvids.begin(), choose_a_card_strcsvids.end(),
            [](const StrCsvid &a, const StrCsvid &b) {
              return _stricmp(a.first, b.first) < 0;
            });
#ifdef __EMSCRIPTEN__
  SendMessageA(hwnd, LB_RESETCONTENT, 0, 0);
  for (const auto &entry : choose_a_card_strcsvids)
    SendMessageA(hwnd, LB_ADDSTRING, 0, (LPARAM)entry.first);
#else
  SendMessageA(hwnd, LB_SETCOUNT, choose_a_card_strcsvids.size(), 0);
#endif
  return choose_a_card_strcsvids.size();
}

static INT_PTR CALLBACK dlgproc_choose_a_card(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  static choose_a_card_args_t args;
  static color_test_t origcolors;
  static type_t origtypes;
  static HGDIOBJ winbk_chooseacard;
  static std::string *keys_input_storage;
  static bool reset_keys_input = true;
  static int prev_selected = -1;
  static LRESULT prev_drawn_selected = -1;
  if (!keys_input_storage)
    keys_input_storage = new std::string;
  std::string &keys_input = *keys_input_storage;

  const int color_buttons[] = {CTL_WHITE, CTL_BLUE, CTL_BLACK, CTL_RED, CTL_GREEN, CTL_COLORLESS};
  const color_test_t color_values[] = {COLOR_TEST_WHITE, COLOR_TEST_BLUE, COLOR_TEST_BLACK,
                                      COLOR_TEST_RED, COLOR_TEST_GREEN, COLOR_TEST_COLORLESS};
  const int type_buttons[] = {CTL_CREATURES, CTL_ARTIFACTS, CTL_ENCHANTMENTS, CTL_SORCERIES,
                             CTL_INSTANTS, CTL_PLANESWALKERS, CTL_LANDS};
  const type_t type_values[] = {TYPE_CREATURE, TYPE_ARTIFACT, TYPE_ENCHANTMENT, TYPE_SORCERY,
                              TYPE_INSTANT, TARGET_TYPE_PLANESWALKER, TYPE_LAND};

  switch (msg)
  {
    case WM_INITDIALOG:
    {
      char path[262];
      sprintf(path, "%s\\WINBK_ChooseACard.pic", path_DuelArt);
      winbk_chooseacard = read_graphics_file(path);
      keys_input.clear();
      reset_keys_input = true;
      args = *reinterpret_cast<choose_a_card_args_t *>(lparam);
      prev_selected = prev_drawn_selected = -1;
      if (args.prompt)
        SetWindowTextA(hwnd, args.prompt);
      SendDlgItemMessageA(hwnd, CTL_FULLCARD, WM_USER + 1, -1, 0);
      SetWindowLongA(GetDlgItem(hwnd, CTL_FULLCARD), 12, 1);
      args.types = static_cast<type_t>(args.types & (TYPE_LAND | TYPE_CREATURE | TYPE_ENCHANTMENT |
                        TYPE_SORCERY | TYPE_INSTANT | TYPE_ARTIFACT | TARGET_TYPE_PLANESWALKER));
      if (!args.types)
        args.types = TYPE_NONEFFECT;
      if (!args.colors)
        args.colors = COLOR_TEST_COLORLESS;
      else
      {
        args.colors = static_cast<color_test_t>(args.colors & COLOR_TEST_ANY);
        if (!args.colors)
          args.colors = COLOR_TEST_ANY;
      }
      origtypes = args.types;
      origcolors = args.colors;
      EnableWindow(GetDlgItem(hwnd, CTL_ALL_COLORS), TRUE);
      for (unsigned int i = 0; i < 6; ++i)
      {
        EnableWindow(GetDlgItem(hwnd, color_buttons[i]), (origcolors & color_values[i]) != 0);
        if (args.initial_btn_color == color_buttons[i])
        {
          if (!(origcolors & color_values[i]))
            args.initial_btn_color = CTL_ALL_COLORS;
          else
            args.colors = color_values[i];
        }
      }
      EnableWindow(GetDlgItem(hwnd, CTL_MULTICOLOR), TRUE);
      if (args.initial_btn_color == CTL_MULTICOLOR)
        args.colors = static_cast<color_test_t>(origcolors | 0x100);
      CheckDlgButton(hwnd, args.initial_btn_color, BST_CHECKED);
      last_btn_color = args.initial_btn_color;
      EnableWindow(GetDlgItem(hwnd, CTL_ALL_TYPES), TRUE);
      for (unsigned int i = 0; i < 7; ++i)
      {
        EnableWindow(GetDlgItem(hwnd, type_buttons[i]), (origtypes & type_values[i]) != 0);
        if (args.initial_btn_type == type_buttons[i])
        {
          if (!(origtypes & type_values[i]))
            args.initial_btn_type = CTL_ALL_TYPES;
          else
            args.types = type_values[i];
        }
      }
      CheckDlgButton(hwnd, args.initial_btn_type, BST_CHECKED);
      last_btn_type = args.initial_btn_type;
      if (!args.allow_cancel)
        ShowWindow(GetDlgItem(hwnd, CTL_CANCEL), SW_HIDE);
      if (dlgproc_choose_a_card_helper(GetDlgItem(hwnd, CTL_LISTBOX), args.colors, args.types, args.constraints) > 0)
        SetFocus(GetDlgItem(hwnd, CTL_LISTBOX));
      return FALSE;
    }

    case WM_DRAWITEM:
    {
      DRAWITEMSTRUCT *dis = reinterpret_cast<DRAWITEMSTRUCT *>(lparam);
      if (wparam != CTL_LISTBOX ||
          (dis->itemAction != ODA_DRAWENTIRE && dis->itemAction != ODA_SELECT && dis->itemAction != ODA_FOCUS))
        return FALSE;
      COLORREF old_bk = SetBkColor(dis->hDC, GetSysColor((dis->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHT : COLOR_WINDOW));
      COLORREF old_fg = SetTextColor(dis->hDC, GetSysColor((dis->itemState & ODS_SELECTED) ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
      const char *name = choose_a_card_strcsvids[dis->itemID].first;
      ExtTextOutA(dis->hDC, dis->rcItem.left, dis->rcItem.top, ETO_OPAQUE | ETO_CLIPPED,
                  &dis->rcItem, name, strlen(name), NULL);
      SetTextColor(dis->hDC, old_fg);
      SetBkColor(dis->hDC, old_bk);
      if (dis->itemState & ODS_SELECTED)
      {
        LRESULT selected = SendDlgItemMessageA(hwnd, CTL_LISTBOX, LB_GETCURSEL, 0, 0);
        if (prev_drawn_selected != selected)
        {
          SendDlgItemMessageA(hwnd, CTL_FULLCARD, WM_USER + 1,
                              choose_a_card_strcsvids[selected].second.raw, 0);
          prev_drawn_selected = selected;
        }
      }
      return TRUE;
    }

    case WM_ERASEBKGND:
      if (!winbk_chooseacard)
        return FALSE;
      {
        RECT rect;
        set_palette_and_stretch_mode(reinterpret_cast<HDC>(wparam));
        GetClientRect(hwnd, &rect);
        EnterCriticalSection(critical_section_for_painting);
        int savedc = SaveDC(dblbfr_hdc);
        blt_pic(dblbfr_hdc, &rect, (HBITMAP)winbk_chooseacard);
        RestoreDC(dblbfr_hdc, savedc);
        GetClientRect(hwnd, &rect);
        BitBlt(reinterpret_cast<HDC>(wparam), 0, 0, rect.right, rect.bottom, dblbfr_hdc, 0, 0, SRCCOPY);
        LeaveCriticalSection(critical_section_for_painting);
      }
      return TRUE;

    case WM_CHARTOITEM:
      keys_input.push_back(static_cast<char>(wparam));
      for (unsigned int i = 0; i < choose_a_card_strcsvids.size(); ++i)
      {
        if (!_strnicmp(keys_input.c_str(), choose_a_card_strcsvids[i].first, keys_input.size()))
        {
          prev_selected = i;
          reset_keys_input = false;
          return i;
        }
      }
      reset_keys_input = false;
      prev_selected = HIWORD(wparam);
      return HIWORD(wparam);

    case WM_CTLCOLORSTATIC:
      if (!winbk_chooseacard)
        return FALSE;
      SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));

    case WM_QUERYNEWPALETTE:
    case WM_PALETTEISCHANGING:
    case WM_PALETTECHANGED:
      return handle_palette_msgs(hwnd, msg, (HWND)wparam, lparam);

    case WM_COMMAND:
    {
      int control = LOWORD(wparam);
      if (control == CTL_OK || (control == CTL_CANCEL && args.allow_cancel))
      {
        int result = -1;
        if (control == CTL_OK)
        {
          LRESULT selected = SendDlgItemMessageA(hwnd, CTL_LISTBOX, LB_GETCURSEL, 0, 0);
          result = choose_a_card_strcsvids[selected].second.raw;
        }
        if (winbk_chooseacard)
        {
          DeleteObject(winbk_chooseacard);
          winbk_chooseacard = NULL;
        }
        EndDialog(hwnd, result);
        return FALSE;
      }
      if (control == CTL_LISTBOX)
      {
        if (HIWORD(wparam) == LBN_SELCHANGE)
        {
          LRESULT selected = SendDlgItemMessageA(hwnd, CTL_LISTBOX, LB_GETCURSEL, 0, 0);
          SendDlgItemMessageA(hwnd, CTL_FULLCARD, WM_USER + 1,
                              choose_a_card_strcsvids[selected].second.raw, 0);
          if (!reset_keys_input || prev_selected == selected)
            reset_keys_input = true;
          else
            keys_input.clear();
          prev_selected = selected;
        }
        else if (HIWORD(wparam) == LBN_DBLCLK)
          SendMessageA(hwnd, WM_COMMAND, CTL_OK, reinterpret_cast<LPARAM>(GetDlgItem(hwnd, CTL_OK)));
        return FALSE;
      }
      if (HIWORD(wparam))
        return FALSE;
      bool color_control = control == CTL_ALL_COLORS || control == CTL_MULTICOLOR;
      bool type_control = control == CTL_ALL_TYPES;
      for (unsigned int i = 0; i < 6; ++i)
        color_control = color_control || control == color_buttons[i];
      for (unsigned int i = 0; i < 7; ++i)
        type_control = type_control || control == type_buttons[i];
      if (!color_control && !type_control)
        return FALSE;
      keys_input.clear();
      reset_keys_input = true;
      prev_selected = -1;
      if (type_control)
      {
        args.types = TYPE_NONE;
        if (IsDlgButtonChecked(hwnd, CTL_ALL_TYPES))
          args.types = origtypes;
        else
          for (unsigned int i = 0; i < 7; ++i)
            if (IsDlgButtonChecked(hwnd, type_buttons[i]) && (origtypes & type_values[i]))
              args.types = static_cast<type_t>(args.types | type_values[i]);
        if (!args.types)
          args.types = TYPE_CREATURE;
      }
      else
      {
        args.colors = COLOR_TEST_0;
        if (IsDlgButtonChecked(hwnd, CTL_ALL_COLORS))
          args.colors = origcolors;
        else
        {
          for (unsigned int i = 0; i < 6; ++i)
            if (IsDlgButtonChecked(hwnd, color_buttons[i]) && (origcolors & color_values[i]))
              args.colors = i == 5 ? COLOR_TEST_COLORLESS
                                  : static_cast<color_test_t>(args.colors | color_values[i]);
          if (IsDlgButtonChecked(hwnd, CTL_MULTICOLOR))
            args.colors = static_cast<color_test_t>(origcolors | 0x100);
        }
        if (!(args.colors & COLOR_TEST_ANY))
          args.colors = static_cast<color_test_t>(args.colors | COLOR_TEST_COLORLESS);
      }
      if (dlgproc_choose_a_card_helper(GetDlgItem(hwnd, CTL_LISTBOX), args.colors, args.types, args.constraints) > 0)
        SetFocus(GetDlgItem(hwnd, CTL_LISTBOX));
      if (IsDlgButtonChecked(hwnd, control))
      {
        if (type_control)
          last_btn_type = control;
        else
          last_btn_color = control;
      }
      return FALSE;
    }
  }
  return FALSE;
}

static csvid_t choose_a_card_impl(int who_chooses, test_definition_t *constraints, const char *prompt,
                                  color_test_t colors, type_t types, bool allow_cancel,
                                  color_test_t initial_color, type_t initial_type)
{
  (void)who_chooses;
  if (trace_mode & 2)
    backtrace_and_abort("functions/choose_a_card.cpp:867\n!(trace_mode & 2) || !\"calling choose_a_card() directly doesn't work in multiplayer\"");
  if (!allow_cancel && ai_is_speculating == 1)
    backtrace_and_abort("functions/choose_a_card.cpp:877\nallow_cancel || ai_is_speculating != 1");
  choose_a_card_args_t args;
  args.colors = colors;
  args.types = types;
  args.prompt = prompt ? prompt : "Choose a card name.";
  args.constraints = constraints;
  args.allow_cancel = allow_cancel;
  switch (initial_color) {
    case COLOR_TEST_COLORLESS: args.initial_btn_color = CTL_COLORLESS; break;
    case COLOR_TEST_BLACK: args.initial_btn_color = CTL_BLACK; break;
    case COLOR_TEST_BLUE: args.initial_btn_color = CTL_BLUE; break;
    case COLOR_TEST_GREEN: args.initial_btn_color = CTL_GREEN; break;
    case COLOR_TEST_RED: args.initial_btn_color = CTL_RED; break;
    case COLOR_TEST_WHITE: args.initial_btn_color = CTL_WHITE; break;
    case COLOR_TEST_ANY_COLORED: args.initial_btn_color = CTL_MULTICOLOR; break;
    default: args.initial_btn_color = CTL_ALL_COLORS; break;
  }
  switch (initial_type) {
    case TYPE_LAND: args.initial_btn_type = CTL_LANDS; break;
    case TYPE_CREATURE: args.initial_btn_type = CTL_CREATURES; break;
    case TYPE_ENCHANTMENT: args.initial_btn_type = CTL_ENCHANTMENTS; break;
    case TYPE_SORCERY: args.initial_btn_type = CTL_SORCERIES; break;
    case TYPE_INSTANT: args.initial_btn_type = CTL_INSTANTS; break;
    case TYPE_ARTIFACT: args.initial_btn_type = CTL_ARTIFACTS; break;
    case TARGET_TYPE_PLANESWALKER: args.initial_btn_type = CTL_PLANESWALKERS; break;
    default: args.initial_btn_type = CTL_ALL_TYPES; break;
  }
  csvid_t result(static_cast<int>(DialogBoxIndirectParamA(main_hinstance, generate_choose_a_card_dlg_template(),
                                                         hwnd_MainClass, dlgproc_choose_a_card,
                                                         reinterpret_cast<LPARAM>(&args))));
  choose_a_card_strcsvids.clear();
  return result;
}

extern "C" int choose_a_card(const char *prompt, color_test_t colors, type_t types)
{
  return choose_a_card_impl(HUMAN, NULL, prompt, colors,
                           types, false, COLOR_TEST_ANY, TYPE_ANY).raw;
}

extern "C" int choose_a_card_allow_cancel_with_defaults(char *prompt, color_test_t allowable_colors,
                                                       type_t allowable_types, color_test_t initial_color,
                                                       type_t initial_type)
{
  return choose_a_card_impl(HUMAN, NULL, prompt, allowable_colors, allowable_types,
                           true, initial_color, initial_type).raw;
}

extern "C" int choose_a_card_iid(char *prompt, color_test_t colors, type_t types)
{
  return choose_a_card_impl(HUMAN, NULL, prompt, colors, types, false, COLOR_TEST_ANY, TYPE_ANY).iid().raw;
}

extern "C" int choose_a_card_test(char *prompt, test_definition_t *constraints)
{
  return choose_a_card_impl(HUMAN, constraints, prompt, COLOR_TEST_ANY, TYPE_ANY,
                           false, COLOR_TEST_ANY, TYPE_ANY).raw;
}

extern "C" int choose_a_card_test_ct(char *prompt, test_definition_t *constraints,
                                     color_test_t colors, type_t types)
{
  return choose_a_card_impl(HUMAN, constraints, prompt, colors, types, false, COLOR_TEST_ANY, TYPE_ANY).raw;
}

extern "C" int choose_a_card_test_ct_allow_cancel(char *prompt, test_definition_t *constraints,
                                                  color_test_t colors, type_t types)
{
  return choose_a_card_impl(HUMAN, constraints, prompt, colors, types, true, COLOR_TEST_ANY, TYPE_ANY).raw;
}

extern "C" void choose_a_card_get_last_options(color_test_t *cols, type_t *typs)
{
  if (cols)
  {
    switch (last_btn_color) {
      case CTL_BLACK: *cols = COLOR_TEST_BLACK; break;
      case CTL_BLUE: *cols = COLOR_TEST_BLUE; break;
      case CTL_GREEN: *cols = COLOR_TEST_GREEN; break;
      case CTL_RED: *cols = COLOR_TEST_RED; break;
      case CTL_WHITE: *cols = COLOR_TEST_WHITE; break;
      case CTL_COLORLESS: *cols = COLOR_TEST_COLORLESS; break;
      case CTL_MULTICOLOR: *cols = COLOR_TEST_ANY_COLORED; break;
      default: *cols = COLOR_TEST_ANY; break;
    }
  }
  if (typs)
  {
    static const int types_by_button[9] = {2, 64, 4, 8, 16, 1, 255, 255, 256};
    *typs = static_cast<type_t>(last_btn_type >= CTL_CREATURES && last_btn_type <= CTL_PLANESWALKERS
                              ? types_by_button[last_btn_type - CTL_CREATURES] : TYPE_ANY);
  }
}
#endif

#ifndef SHANDALAR
/* Recovered October 2022 inline-only helpers; native code remains in their callers. */
static int color_test_to_ctl(color_test_t col)
{
  switch (col)
  {
    case COLOR_TEST_COLORLESS: return CTL_COLORLESS;
    case COLOR_TEST_BLACK: return CTL_BLACK;
    case COLOR_TEST_BLUE: return CTL_BLUE;
    case COLOR_TEST_GREEN: return CTL_GREEN;
    case COLOR_TEST_RED: return CTL_RED;
    case COLOR_TEST_WHITE: return CTL_WHITE;
    case COLOR_TEST_ANY_COLORED: return CTL_MULTICOLOR;
    default: return CTL_ALL_COLORS;
  }
}

static color_test_t ctl_to_color_test(int ctl)
{
  switch (ctl)
  {
    case CTL_COLORLESS: return COLOR_TEST_COLORLESS;
    case CTL_BLACK: return COLOR_TEST_BLACK;
    case CTL_BLUE: return COLOR_TEST_BLUE;
    case CTL_GREEN: return COLOR_TEST_GREEN;
    case CTL_RED: return COLOR_TEST_RED;
    case CTL_WHITE: return COLOR_TEST_WHITE;
    case CTL_MULTICOLOR: return COLOR_TEST_ANY_COLORED;
    default: return COLOR_TEST_ANY;
  }
}

static int type_to_ctl(type_t typ)
{
  switch (typ)
  {
    case TYPE_LAND: return CTL_LANDS;
    case TYPE_CREATURE: return CTL_CREATURES;
    case TYPE_ENCHANTMENT: return CTL_ENCHANTMENTS;
    case TYPE_SORCERY: return CTL_SORCERIES;
    case TYPE_INSTANT: return CTL_INSTANTS;
    case TYPE_ARTIFACT: return CTL_ARTIFACTS;
    case TARGET_TYPE_PLANESWALKER: return CTL_PLANESWALKERS;
    default: return CTL_ALL_TYPES;
  }
}

static type_t ctl_to_type(int ctl)
{
  switch (ctl)
  {
    case CTL_LANDS: return TYPE_LAND;
    case CTL_CREATURES: return TYPE_CREATURE;
    case CTL_ENCHANTMENTS: return TYPE_ENCHANTMENT;
    case CTL_SORCERIES: return TYPE_SORCERY;
    case CTL_INSTANTS: return TYPE_INSTANT;
    case CTL_ARTIFACTS: return TYPE_ARTIFACT;
    case CTL_PLANESWALKERS: return TARGET_TYPE_PLANESWALKER;
    default: return TYPE_ANY;
  }
}
#endif

#ifndef SHANDALAR
/* Original chooser enum operators, recovered from native inline instances. */
OP_BITWISE(color_test_t)
OP_BITWISE(type_t)
#endif
