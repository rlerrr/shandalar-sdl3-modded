// -*- tab-width:4; c-basic-offset:2; -*-
// Drawcardlib: display card and mana cost graphics.
// drawcounters.c: draw counters

#include "drawcardlib.h"

WINGDIAPI BOOL WINAPI AlphaBlend(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);

int suppress_next_counters = 0;

void
draw_special_counters(HDC hdc, const RECT* rect, int player, int card)
{
  // magic.exe:0x4d3a00

  if (!hdc || !rect)
	return;

  int counter_type;
  int num;
  if (player >= 0 && player <= 1)
	{
	  card_instance_t actual;
	  card_instance_t* instance = get_displayed_card_instance(player, card);	// unsafe to dereference

	  EnterCriticalSection(critical_section_for_display);
	  memcpy(&actual, instance, sizeof(card_instance_t));
	  LeaveCriticalSection(critical_section_for_display);

	  instance = &actual;	// now safe

	  counter_type = instance->special_counter_type;

	  num = instance->special_counters;

	  if (suppress_next_counters && counter_type == COUNTER_LOYALTY)
		return;
	}
  else if (player == -1)	// Special case: counter_type and num are encoded in card
	{
	  counter_type = BYTE0(card);
	  num = BYTE1(card);
	}
  else
	return;

  if (counter_type < 0 || counter_type >= COUNTER_end)
	return;
  if (num <= 0)
	return;

  RECT r;
  get_special_counter_rect(&r, rect, num);

  if (parent == PARENT_SHANDALAR)
	EnterCriticalSection(critical_section_for_drawing);

  unsigned int src_width, src_height;
  if (counters_renderer == RENDERER_GDIPLUS)
	{
	  GdipGetImageWidth(gpics[CARDCOUNTERS], &src_width);
	  GdipGetImageHeight(gpics[CARDCOUNTERS], &src_height);
	  src_width /= counters_num_columns;
	  src_height /= counters_num_rows;
	}
  else
	{
	  BITMAP bmp;
	  GetObject(pics[CARDCOUNTERS], sizeof(BITMAP), &bmp);
	  src_width = bmp.bmWidth / counters_num_columns;
	  src_height = bmp.bmHeight / counters_num_rows;
	}

  int scaled_height = r.bottom - r.top;
  int scaled_width = scaled_height * src_width / src_height;

  int spacing;
  if (num <= 1)
	spacing = scaled_width;
  else
	spacing = (r.right - r.left - scaled_width) / (num - 1);

  int src_xpos = (counter_type / counters_num_rows) * src_width;
  int src_ypos = (counter_type % counters_num_rows) * src_height;

  int dest_x = r.right - scaled_width;

  BLENDFUNCTION blend;
  int savedc = 0;

  if (counters_renderer != RENDERER_GDIPLUS)
	{
	  savedc = SaveDC(hdc);
	  SelectObject(*spare_hdc, pics[CARDCOUNTERS]);

	  blend.BlendOp = AC_SRC_OVER;
	  blend.BlendFlags = 0;
	  blend.SourceConstantAlpha = 255;
	  blend.AlphaFormat = AC_SRC_ALPHA;
	}

  int i;
  for (i = 0; i < num; ++i)
	{
	  r.left = dest_x;
	  r.right = dest_x + scaled_width;

	  if (counters_renderer == RENDERER_GDIPLUS)
		gdip_blt(hdc, &r, CARDCOUNTERS, src_xpos, src_ypos, src_width, src_height, NULL);
	  else
		AlphaBlend(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top,
				   *spare_hdc, src_xpos, src_ypos, src_width, src_height, blend);

	  dest_x -= spacing;
	}

  if (counters_renderer != RENDERER_GDIPLUS)
	RestoreDC(hdc, savedc);

  if (parent == PARENT_SHANDALAR)
	LeaveCriticalSection(critical_section_for_drawing);
}

void
get_special_counter_rect(RECT* rect_dest, const RECT* rect_src, int num)
{
  // 0x4d3b60

  if (!rect_dest)
	return;

  if (!rect_src || num <= 0)
	{
	  SetRect(rect_dest, 0, 0, 0, 0);
	  return;
	}

  RECT r;
  r.left = rect_src->left + ((unsigned int)(18 * (rect_src->right - rect_src->left)) >> 8);
  r.right = rect_src->right - ((unsigned int)(51 * (rect_src->right - rect_src->left)) >> 8);
  r.top = rect_src->top + ((unsigned int)(20 * (rect_src->bottom - rect_src->top)) >> 8);
  r.bottom = r.top + 27 * (rect_src->bottom - rect_src->top) / 100;

  unsigned int src_width, src_height;
  if (counters_renderer == RENDERER_GDIPLUS)
	{
	  GdipGetImageWidth(gpics[CARDCOUNTERS], &src_width);
	  GdipGetImageHeight(gpics[CARDCOUNTERS], &src_height);
	  src_width /= counters_num_columns;
	  src_height /= counters_num_rows;
	}
  else
	{
	  BITMAP bmp;
	  GetObject(pics[CARDCOUNTERS], sizeof(BITMAP), &bmp);
	  src_width = bmp.bmWidth / counters_num_columns;
	  src_height = bmp.bmHeight / counters_num_rows;
	}

  int scaled_height = r.bottom - r.top;
  int scaled_width = scaled_height * src_width / src_height;

  int spacing;
  for (spacing = scaled_width;
	   spacing > 1 && r.left + scaled_width + spacing * (num - 1) > r.right;
	   --spacing)
	{}

  SetRect(rect_dest, r.left, r.top, r.left + scaled_width + spacing * (num - 1), r.bottom);
}

void
draw_standard_counters(HDC hdc, const RECT* rect, int player, int card)
{
  if (!hdc || !rect)
	return;

  card_instance_t actual;
  card_instance_t* instance = get_displayed_card_instance(player, card);	// unsafe to dereference

  EnterCriticalSection(critical_section_for_display);
  memcpy(&actual, instance, sizeof(card_instance_t));
  LeaveCriticalSection(critical_section_for_display);

  instance = &actual;	// now safe

  int counters = instance->counters;
  int counters2 = instance->counters2;
  int counters3 = instance->counters3;
  int counters4 = instance->counters4;
  int counters5 = instance->counters5;
  int counters_m1m1 = instance->counters_m1m1;

  int counter_type_2;
  int counter_type_3;
  int counter_type_4;
  int counter_type_5;

  if (parent == PARENT_SHANDALAR)
	{
	  counter_type_2 = instance->unk52;
	  counter_type_3 = instance->unk53;
	  counter_type_4 = instance->card_color;
	  counter_type_5 = instance->unk5a;
	}
  else
	{
	  uint8_t* t18 = (void*)(&instance->targets[18].player);
	  counter_type_2 = t18[0];
	  counter_type_3 = t18[1];
	  counter_type_4 = t18[2];
	  counter_type_5 = t18[3];
	}

  if (!counters && !counters2 && !counters3 && !counters4 && !counters5 && !counters_m1m1)
	return;

  if (parent == PARENT_SHANDALAR)
	EnterCriticalSection(critical_section_for_drawing);

  unsigned int src_width, src_height;
  if (counters_renderer == RENDERER_GDIPLUS)
	{
	  GdipGetImageWidth(gpics[CARDCOUNTERS], &src_width);
	  GdipGetImageHeight(gpics[CARDCOUNTERS], &src_height);
	  src_width /= counters_num_columns;
	  src_height /= counters_num_rows;
	}
  else
	{
	  BITMAP bmp;
	  GetObject(pics[CARDCOUNTERS], sizeof(BITMAP), &bmp);
	  src_width = bmp.bmWidth / counters_num_columns;
	  src_height = bmp.bmHeight / counters_num_rows;
	}

  int scaled_height = rect->bottom - rect->top;
  int scaled_width = scaled_height * src_width / src_height;

  int one_third_card_width = (rect->right - rect->left) / 3;

  BLENDFUNCTION blend;
  RECT r;
  int spacing, dest_x, src_xpos, src_ypos, i, savedc = 0;

  if (counters_renderer != RENDERER_GDIPLUS)
	{
	  savedc = SaveDC(hdc);
	  SelectObject(*spare_hdc, pics[CARDCOUNTERS]);

	  blend.BlendOp = AC_SRC_OVER;
	  blend.BlendFlags = 0;
	  blend.SourceConstantAlpha = 255;
	  blend.AlphaFormat = AC_SRC_ALPHA;
	}

#define COUNTERS(which, init_x, init_y, imgnum, lambda)	do {																\
  if (which > 0 && imgnum != 255)																							\
	{																														\
	  for (spacing = scaled_width; scaled_width + spacing * ((which) - 1) > one_third_card_width && spacing > 2; --spacing)	\
		{}																													\
	  dest_x = (init_x) + spacing * ((which) - 1);																			\
	  src_xpos = ((imgnum) / counters_num_rows) * src_width;																\
	  src_ypos = ((imgnum) % counters_num_rows) * src_height;																\
	  r.top = (init_y);																										\
	  r.bottom = r.top + scaled_height;																						\
	  for (i = 0; i < (which); ++i)																							\
		{																													\
		  r.left = dest_x;																									\
		  r.right = r.left + scaled_width;																					\
																															\
		  { lambda; }																										\
																															\
		  if (counters_renderer == RENDERER_GDIPLUS)																		\
			gdip_blt(hdc, &r, CARDCOUNTERS, src_xpos, src_ypos, src_width, src_height, NULL);								\
		  else																												\
			AlphaBlend(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top,												\
					   *spare_hdc, src_xpos, src_ypos, src_width, src_height, blend);										\
																															\
		  dest_x -= spacing;																								\
		}																													\
	}																														\
  } while (0)

  int left_edge_of_dest = rect->left;
  int top_edge_of_dest = rect->top;
  COUNTERS(counters4, left_edge_of_dest, top_edge_of_dest, counter_type_4,);

  int left_third_of_dest = left_edge_of_dest + one_third_card_width;
  COUNTERS(counters3, left_third_of_dest, top_edge_of_dest, counter_type_3,);

  int right_third_of_dest = left_third_of_dest + one_third_card_width;
  COUNTERS(counters2, right_third_of_dest, top_edge_of_dest, counter_type_2,);

  int middle_of_dest = (rect->right + rect->left) / 2;
  int left_sixth_of_dest = middle_of_dest - one_third_card_width;
  int bottom_third_of_dest = 2 * scaled_height / 3 + top_edge_of_dest;
  int p1p1m1m1 = counters + counters_m1m1, changed = 0;
  COUNTERS(p1p1m1m1, left_sixth_of_dest, bottom_third_of_dest, COUNTER_P1_P1,
		   {
			 if (!changed && i >= counters)
			   {
				 changed = 1;
				 src_xpos = (COUNTER_M1_M1 / counters_num_rows) * src_width;
				 src_ypos = (COUNTER_M1_M1 % counters_num_rows) * src_height;
			   }
		   });

  COUNTERS(counters5, middle_of_dest, bottom_third_of_dest, counter_type_5,);

  if (counters_renderer != RENDERER_GDIPLUS)
	RestoreDC(hdc, savedc);

  if (parent == PARENT_SHANDALAR)
	LeaveCriticalSection(critical_section_for_drawing);

#undef COUNTERS
}

void
draw_counters_for_fullcard(HDC hdc, const RECT* rect, counter_t counter_type, int num)
{
  // assumes already in appropriate critical sections
  if (!hdc || !rect || num <= 0 || counter_type == COUNTER_invalid)
	return;

  int wid = rect->right - rect->left;
  int hgt = rect->bottom - rect->top;

  RECT r;
  r.left = rect->left + wid/3 + wid/12;
  //r.right = rect->right - wid/3;
  r.top = rect->top + hgt/3;
  r.bottom = rect->bottom - hgt/3;

  unsigned int src_width, src_height;
  if (counters_renderer == RENDERER_GDIPLUS)
	{
	  GdipGetImageWidth(gpics[CARDCOUNTERS], &src_width);
	  GdipGetImageHeight(gpics[CARDCOUNTERS], &src_height);
	  src_width /= counters_num_columns;
	  src_height /= counters_num_rows;
	}
  else
	{
	  BITMAP bmp;
	  GetObject(pics[CARDCOUNTERS], sizeof(BITMAP), &bmp);
	  src_width = bmp.bmWidth / counters_num_columns;
	  src_height = bmp.bmHeight / counters_num_rows;
	}

  int scaled_height = r.bottom - r.top;
  int scaled_width = scaled_height * src_width / src_height;

  int src_xpos = (counter_type / counters_num_rows) * src_width;
  int src_ypos = (counter_type % counters_num_rows) * src_height;

  BLENDFUNCTION blend;
  int savedc = SaveDC(hdc);

  if (counters_renderer != RENDERER_GDIPLUS)
	{
	  SelectObject(*spare_hdc, pics[CARDCOUNTERS]);

	  blend.BlendOp = AC_SRC_OVER;
	  blend.BlendFlags = 0;
	  blend.SourceConstantAlpha = 255;
	  blend.AlphaFormat = AC_SRC_ALPHA;
	}

  r.right = r.left + scaled_width;

  if (counters_renderer == RENDERER_GDIPLUS)
	gdip_blt(hdc, &r, CARDCOUNTERS, src_xpos, src_ypos, src_width, src_height, NULL);
  else
	AlphaBlend(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top,
			   *spare_hdc, src_xpos, src_ypos, src_width, src_height, blend);

  if (num > 1)
	{
	  /* The normally-allocated fonts are absofuckingly tiny when displaying graveyard/exiled cards, which is what
	   * this is used for, and we can't just draw them bigger - we have to make the effective window smaller instead.
	   * Man, fuck Windows. */
	  /* This should really all be configurable - counter placement and size, text placement and size, text colors -
	   * but I can't be bothered. */
#define INFLATE  4
	  SetWindowExtEx(hdc, 800 / INFLATE, 1200 / INFLATE, NULL);
	  SetWindowOrgEx(hdc, 400 / INFLATE, 600 / INFLATE, NULL);
	  r.left = (rect->right - wid * 0.35) / INFLATE;
	  r.right = rect->right / INFLATE;
	  r.top = (rect->bottom - hgt * 0.45) / INFLATE;
	  r.bottom = rect->bottom / INFLATE;

	  SelectObject(hdc, fonts[BIGCARDLOYALTYCOST_FONT]);
	  SetTextAlign(hdc, TA_LEFT);

	  char buf[20];
	  sprintf(buf, "%d", num);

	  TextWithShadow tws;
	  tws.color.r = tws.color.g = tws.color.b = 255;
	  tws.color.colorref = RGB(255,255,255);
	  tws.shadow_color.r = tws.shadow_color.g = tws.shadow_color.b = 0;
	  tws.shadow_color.colorref = RGB(0,0,0);
	  tws.shadow_offset.x = tws.shadow_offset.y = 2;
	  tws.shadow_visible = 1;

	  draw_text_with_shadow_at_x(hdc, &r, r.left, &tws, -1, buf, -1);
	}

  RestoreDC(hdc, savedc);
}
