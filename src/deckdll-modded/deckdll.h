#ifndef DECKDLL_H	// -*- tab-width:8; c-basic-offset:2; -*-
#define DECKDLL_H 1
// DeckDll: standalone and integrated deckbuilder.

#include <string.h>
#include <math.h>
#include <stdio.h>

#ifdef SHANDALAR_SDL_WIN32_COMPAT
#include "platform/winuser_compat.h"
#include "platform/legacy_io.h"
#else
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#endif

// windows.h defines this to either LoadImageA or LoadImageW.
// We want the function in image.dll.
#undef LoadImage

#define DECKBUILDER 1

#include "../defs.h"
#include "modded_compat.h"
#include "resources.h"

#ifdef SHANDALAR_SDL_WIN32_COMPAT
#define req_colorless req.req_colorless
#define req_black req.req_black
#define req_blue req.req_blue
#define req_hybrid req.req_hybrid
#define hybrid_type req.hybrid_type
#define req_green req.req_green
#define req_red req.req_red
#define req_white req.req_white
#endif

struct DeckEntry;
struct GlobalDeckEntry;

extern "C"
{
  // Our exports
  BOOL WINAPI DllEntryPoint(HINSTANCE dll, DWORD reason, LPVOID reserved);
  BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID reserved);
  WPARAM WINAPI DeckBuilderMain(HWND parent_hwnd, int db_flags_1, int db_flags_2);
  WPARAM deckbuilder_main(HWND parent_hwnd, int db_flags_1, int db_flags_2);
  void deck_set_host_pump_hooks(void (*begin)(void), void (*end)(void));
  void deck_set_debug_card_picker(int (*pick_card)(HWND), int (*card_sort_key)(int));
  void Deckdll_initialize_for_shandalar_sa(const card_ptr_t* i_raw_cards_ptr,
					   int i_available_slots,
					   char* i_card_coded,
					   int (*check_card_count_fn)(const DeckEntry*, int, int, int),
					   int (*is_valid_card_fn)(int),
					   bool (*colors_match_fn)(iid_t, color_test_t),
					   int (*check_colors_inout_edited_deck_fn)(const GlobalDeckEntry*, int, bool),
					   int cardsowned_size,
					   void* i_deck,
					   const card_data_t* i_raw_cards_data,
					   int (*i_get_internal_card_id_from_csv_id_fn)(int),
					   int (*i_min_deck_size)(void),
					   bool (*i_draw_manastripes)(HDC, RECT*, int, int, csvid_t, int));
  void Deckdll_initialize_for_shandalar(const card_ptr_t* i_raw_cards_ptr,
					int i_available_slots,
					char* i_card_coded,
					int (*check_card_count_fn)(const DeckEntry*, int, int, int),
					int (*is_valid_card_fn)(int),
					bool (*colors_match_fn)(iid_t, color_test_t),
					int (*check_colors_inout_edited_deck_fn)(const GlobalDeckEntry*, int, bool));

  // Imports
#ifdef SHANDALAR_SDL_WIN32_COMPAT
#define DLLIMPORT
#else
#define DLLIMPORT __attribute__((dllimport))
#endif
  // from image.dll
  DLLIMPORT HANDLE LoadImage(const char*, HPALETTE, int, int);
  // from Drawcardlib.dll
  DLLIMPORT int DrawFullCard(HDC hdc, const RECT* rect, const card_ptr_t* cp, int version, int big_art_style, int expand_text_box, const char* illus);
  DLLIMPORT int DrawSmallCard(HDC hdc, const RECT* card_rect, const card_ptr_t* cp, int version, int mode, int player, int card);
};

void fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
