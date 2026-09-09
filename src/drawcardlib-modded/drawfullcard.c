// -*- tab-width:4; c-basic-offset:2; -*-
// Drawcardlib: display card and mana cost graphics.
// drawfullcard.c: draw fullsize cards

#include <math.h>

#include "drawcardlib.h"

Watermark watermarks[256];	// Up to 255 may be defined, plus one as a sentinel

void
blt_expansion_symbol(HDC hdc, const RECT* dest_rect, EXPANSION_t expansion, rarity_t rarity, int smallcard)
{
#define EXPANSIONS_WIDE	7	// Number of expansions wide (times four rarities)
#define EXPANSIONS_TALL	25	// Number of expansions tall

  if (!hdc || !dest_rect || !pics[EXPANSION_SYMBOLS])
	return;

  int src_x, src_y;
  switch (rarity)
	{
	  case NONE:
		return;

	  case COMMON:
	  case LAND:
		src_x = 0;
		break;

	  case UNCOMMON:
		src_x = 1;
		break;

	  case RARE:
		src_x = 2;
		break;

	  case MYTHIC:
	  case SPECIAL:
	  default:
		src_x = 3;
		break;
	}

  int no_early_core_sets, no_early_rarity, force_rarity;
  if (smallcard)
	{
	  no_early_core_sets = cfg->smallcard_expansion_no_early_core_sets;
	  no_early_rarity = cfg->smallcard_expansion_no_early_rarity;
	  force_rarity = cfg->smallcard_expansion_force_rarity;
	}
  else
	{
	  no_early_core_sets = cfg->fullcard_expansion_no_early_core_sets;
	  no_early_rarity = cfg->fullcard_expansion_no_early_rarity;
	  force_rarity = cfg->fullcard_expansion_force_rarity;
	}

  if (no_early_core_sets
	  && (expansion == EXPANSION_LIMITED_EDITION_ALPHA
		  || expansion == EXPANSION_LIMITED_EDITION_BETA
		  || expansion == EXPANSION_UNLIMITED_EDITION
		  || expansion == EXPANSION_REVISED_EDITION
		  || expansion == EXPANSION_FOURTH_EDITION
		  || expansion == EXPANSION_FIFTH_EDITION))
	return;
  else if (force_rarity >= 1 && force_rarity <= 4)
	src_x = force_rarity - 1;
  else if (no_early_rarity
		   && expansion <= EXPANSION_PORTAL_SECOND_AGE && expansion != EXPANSION_EXODUS)
	src_x = 0;

  src_y = expansion - 1;
  if (src_y < 0 || src_y >= (EXPANSIONS_WIDE * EXPANSIONS_TALL))
	return;

  src_x += 4 * (src_y / EXPANSIONS_TALL);
  src_y %= EXPANSIONS_TALL;

  GpGraphics* gfx;

  make_gpic_from_pic(EXPANSION_SYMBOLS);
  GdipCreateFromHDC(hdc, &gfx);
  GdipSetInterpolationMode(gfx, InterpolationModeHighQualityBicubic);

  UINT width, height;
  GdipGetImageWidth(gpics[EXPANSION_SYMBOLS], &width);
  GdipGetImageHeight(gpics[EXPANSION_SYMBOLS], &height);

  int sym_wid = width / (EXPANSIONS_WIDE * 4);
  int sym_hgt = height / EXPANSIONS_TALL;

  int dest_x = dest_rect->left;
  int dest_y = dest_rect->top;
  int dest_wid = (dest_rect->right >= dest_rect->left) ? dest_rect->right - dest_rect->left : sym_wid;
  int dest_hgt = (dest_rect->bottom >= dest_rect->top) ? dest_rect->bottom - dest_rect->top : sym_hgt;

  /* Gdi+ prefilters an image before scaling it, so even with a fully-transparent borders around each subimage, it'll still sometimes pick up edges from
   * adjacent subimages.  So what I do is first crop the image to a secondary bitmap (which does not interpolate), then blt with interpolation from *that*
   * to the final image. */

  GpBitmap* cropped = NULL;
  GdipCloneBitmapAreaI(src_x * sym_wid, src_y * sym_hgt, sym_wid, sym_hgt, PixelFormat32bppARGB, gpics[EXPANSION_SYMBOLS], &cropped);

  GdipDrawImageRectI(gfx, cropped, dest_x, dest_y, dest_wid, dest_hgt);

  GdipDisposeImage(cropped);
  GdipDeleteGraphics(gfx);
}

void
blt_dfc_symbol(HDC hdc, const RECT* dest_rect, uint8_t dfc_sym)
{
  if (!hdc || !dest_rect || !pics[DFC_SYMBOLS])
	return;

  if (dfc_sym > DFC_SYMBOL_MAX)
	return;
  int src_x = dfc_sym % 2;
  int src_y = dfc_sym / 2;

  GpGraphics* gfx;

  make_gpic_from_pic(DFC_SYMBOLS);
  GdipCreateFromHDC(hdc, &gfx);
  GdipSetInterpolationMode(gfx, InterpolationModeHighQualityBicubic);

  UINT width;
  GdipGetImageWidth(gpics[DFC_SYMBOLS], &width);

  int sym_wid = width / 2;
  int sym_hgt = sym_wid;

  int dest_x = dest_rect->left;
  int dest_y = dest_rect->top;
  int dest_wid = (dest_rect->right >= dest_rect->left) ? dest_rect->right - dest_rect->left : sym_wid;
  int dest_hgt = (dest_rect->bottom >= dest_rect->top) ? dest_rect->bottom - dest_rect->top : sym_hgt;

  /* Gdi+ prefilters an image before scaling it, so even with a fully-transparent borders around each subimage, it'll
   * still sometimes pick up edges from adjacent subimages.  So what I do is first crop the image to a secondary
   * bitmap (which does not interpolate), then blt with interpolation from *that* to the final image. */

  GpBitmap* cropped = NULL;
  GdipCloneBitmapAreaI(src_x * sym_wid, src_y * sym_hgt, sym_wid, sym_hgt, PixelFormat32bppARGB, gpics[DFC_SYMBOLS], &cropped);

  GdipDrawImageRectI(gfx, cropped, dest_x, dest_y, dest_wid, dest_hgt);

  GdipDisposeImage(cropped);
  GdipDeleteGraphics(gfx);
}

// Returns 0 if text should use PowertoughnessColor, 1 if should use TitleOnLightColor
static int
blt_powertoughness_box(HDC hdc, const RECT* dest_rect, PicHandleNames frame, GpImageAttributes* alpha_xform)
{
  PicHandleNames ptbox = FRAMEPART_FROM_BASE(POWERTOUGHNESS_MODERN, cfg);
  if (!hdc || !dest_rect || !pics[ptbox])
	return 0;

  typedef enum
  {
	PTY_WHITE = 0,
	PTY_BLUE,
	PTY_BLACK,
	PTY_RED,
	PTY_GREEN,
	PTY_COLORLESS,
	PTY_GOLD,
	PTY_ARTIFACT
  } PowerToughnessY __attribute__((unused));

  typedef enum
  {
	PTX_NORMAL = 0,
	PTX_GOLD,
	PTX_DUAL_LAND
  } PowerToughnessX __attribute__((unused));

  int src_x = PTX_NORMAL, src_y, light = 0;

  // Normalize to modern frame set
  frame = BASE_FRAME(frame);

  if (cfg->fullcard_extended_powertoughness)	// Futureshift logic
	switch (frame)
	  {
		case FRAME_MODERN_SPELL_COLORLESS:
		/* These are impossible, other than CARDOV_*_COLORLESS.  They're included to silence an enumeration value not handled in switch warning; but I don't
		 * want to use a default since I need the warning for FRAME_MODERN_MIN ... FRAME_MODERN_MAX. */
		case 0 ... FRAME_MAX_LOADED:
		case FRAME_MODERN_SPELL_GOLD_BLACK ... FRAME_MODERN_SPELL_GOLD_WHITE:
		case FRAME_MODERN_ARTIFACT_GOLD_BLACK ... FRAME_MODERN_ARTIFACT_GOLD_WHITE:
		case FRAME_MODERN_LAND_GOLD_BLACK ... FRAME_MODERN_LAND_GOLD_WHITE:
		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK ... FRAME_MODERN_ARTIFACT_LAND_GOLD_WHITE:
		case FRAME_MODERN_NYX_MIN ... FRAME_MAX:
		case PICHANDLE_INVALID:
		  src_x = PTX_NORMAL;		src_y = PTY_COLORLESS;	light = 1;	break;

		case FRAME_MODERN_ARTIFACT_COLORLESS:
		  src_x = PTX_NORMAL;		src_y = PTY_ARTIFACT;	light = 1;	break;

		case FRAME_MODERN_LAND_COLORLESS:			case FRAME_MODERN_ARTIFACT_LAND_COLORLESS:
		  src_x = PTX_DUAL_LAND;	src_y = PTY_COLORLESS;	light = 1;	break;

		case FRAME_MODERN_SPELL_BLACK:				case FRAME_MODERN_ARTIFACT_BLACK:
		case FRAME_MODERN_LAND_BLACK:				case FRAME_MODERN_ARTIFACT_LAND_BLACK:
		case FRAME_MODERN_SPELL_HYBRID_WHITE_BLACK:
		case FRAME_MODERN_SPELL_HYBRID_BLUE_BLACK:
		  src_x = PTX_NORMAL;		src_y = PTY_BLACK;		light = 0;	break;

		case FRAME_MODERN_SPELL_BLUE:				case FRAME_MODERN_ARTIFACT_BLUE:
		case FRAME_MODERN_LAND_BLUE:				case FRAME_MODERN_ARTIFACT_LAND_BLUE:
		case FRAME_MODERN_SPELL_HYBRID_GREEN_BLUE:
		case FRAME_MODERN_SPELL_HYBRID_WHITE_BLUE:
		  src_x = PTX_NORMAL;		src_y = PTY_BLUE;		light = 0;	break;

		case FRAME_MODERN_SPELL_GREEN:				case FRAME_MODERN_ARTIFACT_GREEN:
		case FRAME_MODERN_LAND_GREEN:				case FRAME_MODERN_ARTIFACT_LAND_GREEN:
		case FRAME_MODERN_SPELL_HYBRID_BLACK_GREEN:
		case FRAME_MODERN_SPELL_HYBRID_RED_GREEN:
		  src_x = PTX_NORMAL;		src_y = PTY_GREEN;		light = 0;	break;

		case FRAME_MODERN_SPELL_RED:				case FRAME_MODERN_ARTIFACT_RED:
		case FRAME_MODERN_LAND_RED:					case FRAME_MODERN_ARTIFACT_LAND_RED:
		case FRAME_MODERN_SPELL_HYBRID_BLUE_RED:
		case FRAME_MODERN_SPELL_HYBRID_BLACK_RED:
		  src_x = PTX_NORMAL;		src_y = PTY_RED;		light = 0;	break;

		case FRAME_MODERN_SPELL_WHITE:				case FRAME_MODERN_ARTIFACT_WHITE:
		case FRAME_MODERN_LAND_WHITE:				case FRAME_MODERN_ARTIFACT_LAND_WHITE:
		case FRAME_MODERN_SPELL_HYBRID_RED_WHITE:
		case FRAME_MODERN_SPELL_HYBRID_GREEN_WHITE:
		  src_x = PTX_NORMAL;		src_y = PTY_WHITE;		light = 1;	break;

		case FRAME_MODERN_SPELL_GOLD_WHITE_BLUE:	case FRAME_MODERN_ARTIFACT_GOLD_WHITE_BLUE:
		case FRAME_MODERN_SPELL_GOLD_GREEN_BLUE:	case FRAME_MODERN_ARTIFACT_GOLD_GREEN_BLUE:
		  src_x = PTX_GOLD;			src_y = PTY_BLUE;		light = 1;	break;

		case FRAME_MODERN_LAND_GOLD_WHITE_BLUE:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_WHITE_BLUE:
		case FRAME_MODERN_LAND_GOLD_GREEN_BLUE:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_GREEN_BLUE:
		  src_x = PTX_DUAL_LAND;	src_y = PTY_BLUE;		light = 1;	break;

		case FRAME_MODERN_SPELL_GOLD_BLUE_BLACK:	case FRAME_MODERN_ARTIFACT_GOLD_BLUE_BLACK:
		case FRAME_MODERN_SPELL_GOLD_WHITE_BLACK:	case FRAME_MODERN_ARTIFACT_GOLD_WHITE_BLACK:
		  src_x = PTX_GOLD;			src_y = PTY_BLACK;		light = 1;	break;

		case FRAME_MODERN_LAND_GOLD_BLUE_BLACK:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLUE_BLACK:
		case FRAME_MODERN_LAND_GOLD_WHITE_BLACK:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_WHITE_BLACK:
		  src_x = PTX_DUAL_LAND;	src_y = PTY_BLACK;		light = 1;	break;

		case FRAME_MODERN_SPELL_GOLD_BLACK_RED:		case FRAME_MODERN_ARTIFACT_GOLD_BLACK_RED:
		case FRAME_MODERN_SPELL_GOLD_BLUE_RED:		case FRAME_MODERN_ARTIFACT_GOLD_BLUE_RED:
		  src_x = PTX_GOLD;			src_y = PTY_RED;		light = 1;	break;

		case FRAME_MODERN_LAND_GOLD_BLACK_RED:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK_RED:
		case FRAME_MODERN_LAND_GOLD_BLUE_RED:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLUE_RED:
		  src_x = PTX_DUAL_LAND;	src_y = PTY_RED;		light = 1;	break;

		case FRAME_MODERN_SPELL_GOLD_RED_GREEN:		case FRAME_MODERN_ARTIFACT_GOLD_RED_GREEN:
		case FRAME_MODERN_SPELL_GOLD_BLACK_GREEN:	case FRAME_MODERN_ARTIFACT_GOLD_BLACK_GREEN:
		  src_x = PTX_GOLD;			src_y = PTY_GREEN;		light = 1;	break;

		case FRAME_MODERN_LAND_GOLD_RED_GREEN:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_RED_GREEN:
		case FRAME_MODERN_LAND_GOLD_BLACK_GREEN:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK_GREEN:
		  src_x = PTX_DUAL_LAND;	src_y = PTY_GREEN;		light = 1;	break;

		case FRAME_MODERN_SPELL_GOLD_GREEN_WHITE:	case FRAME_MODERN_ARTIFACT_GOLD_GREEN_WHITE:
		case FRAME_MODERN_SPELL_GOLD_RED_WHITE:		case FRAME_MODERN_ARTIFACT_GOLD_RED_WHITE:
		  src_x = PTX_GOLD;			src_y = PTY_WHITE;		light = 1;	break;

		case FRAME_MODERN_LAND_GOLD_GREEN_WHITE:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_GREEN_WHITE:
		case FRAME_MODERN_LAND_GOLD_RED_WHITE:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_RED_WHITE:
		  src_x = PTX_DUAL_LAND;	src_y = PTY_WHITE;		light = 1;	break;

		case FRAME_MODERN_SPELL_GOLD:				case FRAME_MODERN_ARTIFACT_GOLD:
		case FRAME_MODERN_LAND_GOLD:				case FRAME_MODERN_ARTIFACT_LAND_GOLD:
		  src_x = PTX_GOLD;			src_y = PTY_GOLD;		light = 1;	break;
	  }
  else
	switch (frame)
	  {
		case FRAME_MODERN_SPELL_COLORLESS:
		case FRAME_MODERN_LAND_COLORLESS:
		case FRAME_MODERN_ARTIFACT_LAND_COLORLESS:
		case FRAME_MODERN_LAND_GOLD_MIN ... FRAME_MODERN_LAND_GOLD_MAX:
		case FRAME_MODERN_ARTIFACT_LAND_GOLD_MIN ... FRAME_MODERN_ARTIFACT_LAND_GOLD_MAX:
		case FRAME_MODERN_SPELL_HYBRID_MIN ... FRAME_MODERN_SPELL_HYBRID_MAX:
		/* These are impossible, other than CARDOV_*_COLORLESS.  They're included to silence an enumeration value not handled in switch warning; but I don't
		 * want to use a default since I need the warning for FRAME_MODERN_MIN ... FRAME_MODERN_MAX. */
		case 0 ... FRAME_MAX_LOADED:
		case FRAME_MODERN_SPELL_GOLD_BLACK ... FRAME_MODERN_SPELL_GOLD_WHITE:
		case FRAME_MODERN_ARTIFACT_GOLD_BLACK ... FRAME_MODERN_ARTIFACT_GOLD_WHITE:
		case FRAME_MODERN_LAND_GOLD_BLACK ... FRAME_MODERN_LAND_GOLD_WHITE:
		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK ... FRAME_MODERN_ARTIFACT_LAND_GOLD_WHITE:
		case FRAME_MODERN_NYX_MIN ... FRAME_MAX:
		case PICHANDLE_INVALID:
		  src_y = PTY_COLORLESS;
		  break;

		case FRAME_MODERN_SPELL_WHITE:				case FRAME_MODERN_ARTIFACT_WHITE:
		case FRAME_MODERN_LAND_WHITE:				case FRAME_MODERN_ARTIFACT_LAND_WHITE:
		  src_y = PTY_WHITE;
		  break;
		case FRAME_MODERN_SPELL_BLUE:				case FRAME_MODERN_ARTIFACT_BLUE:
		case FRAME_MODERN_LAND_BLUE:				case FRAME_MODERN_ARTIFACT_LAND_BLUE:
		  src_y = PTY_BLUE;
		  break;
		case FRAME_MODERN_SPELL_BLACK:				case FRAME_MODERN_ARTIFACT_BLACK:
		case FRAME_MODERN_LAND_BLACK:				case FRAME_MODERN_ARTIFACT_LAND_BLACK:
		  src_y = PTY_BLACK;
		  break;
		case FRAME_MODERN_SPELL_RED:				case FRAME_MODERN_ARTIFACT_RED:
		case FRAME_MODERN_LAND_RED:					case FRAME_MODERN_ARTIFACT_LAND_RED:
		  src_y = PTY_RED;
		  break;
		case FRAME_MODERN_SPELL_GREEN:				case FRAME_MODERN_ARTIFACT_GREEN:
		case FRAME_MODERN_LAND_GREEN:				case FRAME_MODERN_ARTIFACT_LAND_GREEN:
		  src_y = PTY_GREEN;
		  break;
		case FRAME_MODERN_SPELL_GOLD:				case FRAME_MODERN_ARTIFACT_GOLD:
		case FRAME_MODERN_LAND_GOLD:				case FRAME_MODERN_ARTIFACT_LAND_GOLD:
		case FRAME_MODERN_SPELL_GOLD_MIN ... FRAME_MODERN_SPELL_GOLD_MAX:
		case FRAME_MODERN_ARTIFACT_GOLD_MIN ... FRAME_MODERN_ARTIFACT_GOLD_MAX:
		  src_y = PTY_GOLD;
		  break;
		case FRAME_MODERN_ARTIFACT_COLORLESS:
		  src_y = PTY_ARTIFACT;
		  break;
	  }

  GpGraphics* gfx;

  make_gpic_from_pic(ptbox);
  GdipCreateFromHDC(hdc, &gfx);
  GdipSetInterpolationMode(gfx, InterpolationModeHighQuality);

  UINT width, height;
  GdipGetImageWidth(gpics[ptbox], &width);
  GdipGetImageHeight(gpics[ptbox], &height);
  if (cfg->fullcard_extended_powertoughness)
	width /= 3;
  height /= 8;

  int dest_x = dest_rect->left;
  int dest_y = dest_rect->top;
  int dest_wid = (dest_rect->right >= dest_rect->left) ? (UINT)(dest_rect->right - dest_rect->left) : width;
  int dest_hgt = (dest_rect->bottom >= dest_rect->top) ? (UINT)(dest_rect->bottom - dest_rect->top) : height;

  GdipDrawImageRectRectI(gfx, gpics[ptbox],
						 dest_x, dest_y, dest_wid, dest_hgt,
						 src_x * width, src_y * height, width, height,
						 UnitPixel, alpha_xform, NULL, NULL);

  GdipDeleteGraphics(gfx);

  return light;
}

static void
blt_type_icon(HDC hdc, const RECT* dest_rect, const card_ptr_t* cp, PicHandleNames frame)
{
  PicHandleNames iconimg = FRAMEPART_FROM_BASE(TYPE_ICON_MODERN, cfg);

  if (!hdc || !dest_rect || !iconimg)
	return;

  int src_y;

  // Normalize to modern frame set
  PicHandleNames base_frame = BASE_FRAME(frame);

  // Dark image for light backgrounds is 0.  Light image for dark backgrounds is 1.
  // Light backgrounds: artifact, colorless, white, gold.  Dark backgrounds: blue, black, green, red.
  switch (base_frame)
	{
	  case FRAME_MODERN_SPELL_BLACK:	case FRAME_MODERN_SPELL_BLUE:	case FRAME_MODERN_SPELL_GREEN:	case FRAME_MODERN_SPELL_RED:
	  case FRAME_MODERN_LAND_BLACK:		case FRAME_MODERN_LAND_BLUE:	case FRAME_MODERN_LAND_GREEN:	case FRAME_MODERN_LAND_RED:
	  case FRAME_MODERN_LAND_GOLD_BLUE_BLACK:			case FRAME_MODERN_LAND_GOLD_BLACK_RED:
	  case FRAME_MODERN_LAND_GOLD_RED_GREEN:			case FRAME_MODERN_LAND_GOLD_GREEN_WHITE:
	  case FRAME_MODERN_LAND_GOLD_BLACK_GREEN:			case FRAME_MODERN_LAND_GOLD_GREEN_BLUE:
	  case FRAME_MODERN_LAND_GOLD_BLUE_RED:				case FRAME_MODERN_LAND_GOLD_RED_WHITE:
	  case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLUE_BLACK:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK_RED:
	  case FRAME_MODERN_ARTIFACT_LAND_GOLD_RED_GREEN:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_GREEN_WHITE:
	  case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK_GREEN:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_GREEN_BLUE:
	  case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLUE_RED:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_RED_WHITE:
	  case FRAME_MODERN_SPELL_HYBRID_BLUE_BLACK:		case FRAME_MODERN_SPELL_HYBRID_BLACK_RED:
	  case FRAME_MODERN_SPELL_HYBRID_RED_GREEN:			case FRAME_MODERN_SPELL_HYBRID_GREEN_WHITE:
	  case FRAME_MODERN_SPELL_HYBRID_BLACK_GREEN:		case FRAME_MODERN_SPELL_HYBRID_GREEN_BLUE:
	  case FRAME_MODERN_SPELL_HYBRID_BLUE_RED:			case FRAME_MODERN_SPELL_HYBRID_RED_WHITE:
		src_y = 1;
		break;

	  default:
#if 0
	  case FRAME_MODERN_SPELL_COLORLESS:				case FRAME_MODERN_SPELL_WHITE:
	  case FRAME_MODERN_SPELL_GOLD_MIN ... FRAME_MODERN_SPELL_GOLD_MAX:
	  case FRAME_MODERN_SPELL_GOLD:

	  case FRAME_MODERN_ARTIFACT_MIN ... FRAME_MODERN_ARTIFACT_MAX:

	  case FRAME_MODERN_LAND_COLORLESS:					case FRAME_MODERN_LAND_WHITE:
	  case FRAME_MODERN_LAND_GOLD_WHITE_BLUE:			case FRAME_MODERN_LAND_GOLD_WHITE_BLACK:
	  case FRAME_MODERN_LAND_GOLD:

	  case FRAME_MODERN_ARTIFACT_LAND_COLORLESS:		case FRAME_MODERN_ARTIFACT_LAND_WHITE:
	  case FRAME_MODERN_ARTIFACT_LAND_GOLD_WHITE_BLUE:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_WHITE_BLACK:
	  case FRAME_MODERN_ARTIFACT_LAND_GOLD:

	  case FRAME_MODERN_SPELL_HYBRID_WHITE_BLUE:		case FRAME_MODERN_SPELL_HYBRID_WHITE_BLACK:
		// plus CARDOV_MODERN_COLORLESS, CARDBK_MODERN_SPECIAL, CARDBK_MODERN_TRIGGER
#endif
		src_y = 0;
		break;
	}

  int src_x = -1;

  // Find card type(s).  Surprisingly complex.
  typedef enum
  {
	ICON_TYPE_ARTIFACT = 0,
	ICON_TYPE_CREATURE,
	ICON_TYPE_ENCHANTMENT,
	ICON_TYPE_INSTANT,
	ICON_TYPE_LAND,
	ICON_TYPE_MULTIPLE,
	ICON_TYPE_PLANESWALKER,
	ICON_TYPE_SORCERY
  } IconType __attribute__((unused));

  const card_data_t* cd;

  if (IS_NYX_FRAME(frame, base_frame))	// If a Nyx frame, then we're drawing an enchantment+something else, so multi.
	src_x = ICON_TYPE_MULTIPLE;
  else if (cp->types[0] == SUBTYPE_TRIBAL)	// If a Tribal (and they're all in types[0] currently), always something else, so multi.
	src_x = ICON_TYPE_MULTIPLE;
  else if ((cd = get_card_data_from_csvid(cp->id)))
	switch (cd->type & (TYPE_PERMANENT | TYPE_SPELL))
	  {
		case 0:	return;
		case TYPE_LAND:			src_x = ICON_TYPE_LAND;		break;
		case TYPE_CREATURE | TYPE_INSTANT:	// creature with flash
		case TYPE_CREATURE:		src_x = ICON_TYPE_CREATURE;	break;
		case TYPE_ENCHANTMENT:	src_x = cp->types[0] == SUBTYPE_PLANESWALKER ? ICON_TYPE_PLANESWALKER : ICON_TYPE_ENCHANTMENT;	break;
		case TYPE_SORCERY:		src_x = ICON_TYPE_SORCERY;	break;
		case TYPE_INSTANT | TYPE_INTERRUPT:
		case TYPE_INTERRUPT:
		case TYPE_INSTANT:		src_x = ICON_TYPE_INSTANT;	break;
		case TYPE_ARTIFACT:		src_x = ICON_TYPE_ARTIFACT;	break;
		default:	// two or more bits set
		  src_x = ICON_TYPE_MULTIPLE;
	  }
  else
	{
	  const int16_t* typ;
	  for (typ = &cp->types[0]; typ <= &cp->types[5]; ++typ)
		switch (*typ)
		  {
			case SUBTYPE_ARTIFACT:		src_x = src_x != -1 ? ICON_TYPE_MULTIPLE : ICON_TYPE_ARTIFACT;	break;
			case SUBTYPE_CREATURE:		src_x = src_x != -1 ? ICON_TYPE_MULTIPLE : ICON_TYPE_CREATURE;	break;
			case SUBTYPE_ENCHANTMENT:	src_x = src_x != -1 ? ICON_TYPE_MULTIPLE : ICON_TYPE_ENCHANTMENT;	break;
			case SUBTYPE_INSTANT:		src_x = src_x != -1 ? ICON_TYPE_MULTIPLE : ICON_TYPE_INSTANT;	break;
			case SUBTYPE_LAND:			src_x = src_x != -1 ? ICON_TYPE_MULTIPLE : ICON_TYPE_LAND;	break;
			case SUBTYPE_PLANE:
			case SUBTYPE_PLANESWALKER:	src_x = src_x != -1 && src_x != ICON_TYPE_PLANESWALKER ? ICON_TYPE_MULTIPLE : ICON_TYPE_PLANESWALKER;	break;
			case SUBTYPE_SORCERY:		src_x = src_x != -1 ? ICON_TYPE_MULTIPLE : ICON_TYPE_SORCERY;	break;
			case SUBTYPE_TRIBAL:		src_x = ICON_TYPE_MULTIPLE;	break;
		  }
	}

  if (src_x == -1)
	return;

  GpGraphics* gfx;

  make_gpic_from_pic(iconimg);
  GdipCreateFromHDC(hdc, &gfx);
  GdipSetInterpolationMode(gfx, InterpolationModeHighQuality);

  UINT width, height;
  GdipGetImageWidth(gpics[iconimg], &width);
  GdipGetImageHeight(gpics[iconimg], &height);
  width /= 8;
  height /= 2;

  int dest_x = dest_rect->left;
  int dest_y = dest_rect->top;
  int dest_wid = (dest_rect->right >= dest_rect->left) ? (UINT)(dest_rect->right - dest_rect->left) : width;
  int dest_hgt = (dest_rect->bottom >= dest_rect->top) ? (UINT)(dest_rect->bottom - dest_rect->top) : height;

  GdipDrawImageRectRectI(gfx, gpics[iconimg],
						 dest_x, dest_y, dest_wid, dest_hgt,
						 src_x * width, src_y * height, width, height,
						 UnitPixel, NULL, NULL, NULL);

  GdipDeleteGraphics(gfx);
}

static int
cost_values_to_cost_text(const card_ptr_t* cp, char* dest)
{
  char buf[104];
  unsigned int cost_colorless;
  int cost_black, cost_blue, cost_green, cost_red, cost_white;

  if (!cp || !dest)
	return 0;

  cost_black = cp->req_black;
  cost_blue = cp->req_blue;
  cost_green = cp->req_green;
  cost_red = cp->req_red;
  cost_white = cp->req_white;
  cost_colorless = cp->req_colorless;

  char* p = buf;
  if (cost_colorless)
	{
	  if ((int)cost_colorless == -1
		  || cost_colorless == 72
		  || cost_colorless == 40)
		{
		  *p++ = '|';
		  *p++ = 'X';
		}
	  else if (cost_colorless <= 9)
		{
		  *p++ = '|';
		  *p++ = '0' + cost_colorless;
		}
	  else if (cost_colorless <= 16)
		{
		  *p++ = '|';
		  *p++ = '1';
		  *p++ = '0' + cost_colorless - 10;
		}
	  // otherwise just displays nothing!
	}

#define COLORED_MANA(var, chr)					\
  for (; var != 0; --var)						\
	{											\
	  *p++ = '|';								\
	  *p++ = chr;								\
	}

  COLORED_MANA(cost_white,	'W');
  COLORED_MANA(cost_green,	'G');
  COLORED_MANA(cost_red,	'R');
  COLORED_MANA(cost_black,	'B');
  COLORED_MANA(cost_blue,	'U');

#undef COLORED_MANA

  if (p == buf)
	{
	  *p++ = '|';
	  *p++ = '0';
	}

  *p = 0;

  strcpy(dest, buf);

  return p - buf;
}

static int
draw_fullcard_art(HDC hdc, RECT* rect, uint32_t id, int pic_version)
{
  RECT rect_big_art_pixels;
  CopyRect(&rect_big_art_pixels, &cfg->fullcard_mana);
  LPtoDP(hdc, (POINT*)(&rect_big_art_pixels), 2);

  LoadBigArt(id, pic_version, rect_big_art_pixels.right - rect_big_art_pixels.left, rect_big_art_pixels.bottom - rect_big_art_pixels.top);
  DrawBigArt(hdc, rect, id, pic_version);
  return IsBigArtRightSize(id, pic_version, rect_big_art_pixels.right - rect_big_art_pixels.left, rect_big_art_pixels.bottom - rect_big_art_pixels.top);
}

static int
has_graveyard_ability(const card_ptr_t* cp)
{
  const card_data_t* cd = get_card_data_from_csvid(cp->id);
  if (!cd)
	return 0;

  switch (cd->cc[2])
	{
	  case 0:	// Probably a bug that it's cc[2]==0.
		return cp->id == CARD_ID_KROVIKAN_HORROR;
	  case 1:	// Has alternate casting cost
		return cp->id == CARD_ID_HAAKON_STROMGALD_SCOURGE;
	  case 2:	// Continuous responders to EVENT_GRAVEYARD_ABILITY.
		switch (cp->id)
		  {
			// These don't actually have graveyard abilities, but are implemented that way for their "whenever put into a graveyard" abilities
			case CARD_ID_BLIGHTSTEEL_COLOSSUS:
			case CARD_ID_DARKSTEEL_COLOSSUS:
			case CARD_ID_DREAD:
			case CARD_ID_EMRAKUL_THE_AEONS_TORN:
			case CARD_ID_GUILE:
			case CARD_ID_KOZILEK_BUTCHER_OF_TRUTH:
			case CARD_ID_LEGACY_WEAPON:
			case CARD_ID_PROGENITUS:
			case CARD_ID_PURITY:
			case CARD_ID_SERRA_AVATAR:
			case CARD_ID_ULAMOG_THE_INFINITE_GYRE:
			case CARD_ID_VIGOR:
			case CARD_ID_WORLDSPINE_WURM:
			  // And this has a cc[2] with bit 2 set so it gets EVENT_MODIFY_MANA_PROD
			case CARD_ID_FERTILE_GROUND:
			  return 0;
			default:
			  return 1;
		  }
	  case 4:	// Activates from hand, including cycling and bloodrush.
		switch (cp->id)
		  {
			case CARD_ID_ETERNAL_DRAGON:
			case CARD_ID_PYREWILD_SHAMAN:
			case CARD_ID_UNDEAD_GLADIATOR:
			case CARD_ID_VISCERA_DRAGGER:
			  return 1;
			default:
			  return 0;
		  }
	  case 5:	// Reanimates self
		return 1;
	  case 6:	// Dredge
		return 1;
	  case 9:	// Activated responders to EVENT_GRAVEYARD_ABILITY, mostly Flashback and Unearth.  Annoyingly, also includes Planeswalkers.
		return cp->types[0] != SUBTYPE_PLANESWALKER;
	  case 10:	// Retrace.  These two are probably bugs.
		return cp->id != CARD_ID_GREAT_SABLE_STAG && cp->id != CARD_ID_TERRA_STOMPER;
	  case 12:	// Storm count
		return cp->id == CARD_ID_VENGEVINE;
	  case 13:	// Trap conditions, including Prowl
		return cp->id == CARD_ID_AUNTIES_SNITCH;
	  case 17:	// Scavenge
		return 1;
	  case 18:	// Returns to play when a creature with cmc >= 6 is played
		return 1;
	  default:
		return 0;
	}
}

static int
manacost_width(HDC hdc, const RECT* dest_rect, const card_ptr_t* cp)
{
  char cost_text[104];

  if (!hdc || !dest_rect || !cp)
	return 0;

  int len;
  if (cp->mana_cost_text)
	{
	  len = 0;
	  const char* q = cp->mana_cost_text;
	  while (*q)
		if (*q++ == '|')
		  ++len;
	}
  else
	{
	  // old style calculated from card_ptr_t values
	  cost_values_to_cost_text(cp, cost_text);
	  len = strlen(cost_text) >> 1;
	}

  len *= (dest_rect->bottom - dest_rect->top);
  return len;
}

static void
draw_manacost(HDC hdc, RECT* dest_rect, const card_ptr_t* cp)
{
  char buf[104];
  const char* p;

  if (!hdc || !dest_rect || !cp)
	return;

  int len;
  if (cp->mana_cost_text)
	{
	  len = 0;
	  const char* q = p = cp->mana_cost_text;
	  while (*q)
		if (*q++ == '|')
		  ++len;
	}
  else
	{
	  // old style calculated from card_ptr_t values
	  cost_values_to_cost_text(cp, buf);
	  len = strlen(buf) >> 1;
	  p = buf;
	}

  if (cfg->fullcard_curve_mana_diameter > 0)
	{
	  // Ignore dest_rect, use cfg->fullcard_curve_mana[] instead
	  int diam = cfg->fullcard_curve_mana_diameter;
	  int rad = diam / 2;
	  int pos = 0;
	  while (*p && pos < FULLCARD_CURVE_MANA_MAX)
		{
		  int8_t tag = convert_initial_mana_tag(&p);
		  if (!tag)
			++p;
		  else
			{
			  draw_mana_symbol(hdc, tag, cfg->fullcard_curve_mana[pos].x - rad, cfg->fullcard_curve_mana[pos].y - rad, diam, diam);
			  ++pos;
			}
		}
	}
  else
	{
	  int width_height = dest_rect->bottom - dest_rect->top;	// mana symbols have same width/height
	  int posx = dest_rect->right - (len * width_height);
	  int posy = dest_rect->top;

	  while (*p)
		{
		  int8_t tag = convert_initial_mana_tag(&p);
		  if (!tag)
			++p;
		  else
			{
			  draw_mana_symbol(hdc, tag, posx, posy, width_height, width_height);
			  posx += width_height;
			}
		}
	}
}

static void
strip_hack_and_sleight_symbols(char* dest, const char* src)
{
  while ((*dest = *src))
	{
	  if (*src == '|'
		  && (*(src + 1) == 'H' || *(src + 1) == 'S'))
		src += (*(src + 2) >= '1' && *(src + 2) <= '4') ? 3 : 2;
	  else
		{
		  ++dest;
		  ++src;
		}
	}
}

static int
strip_planeswalker_base_loyalty(char* srcdest)
{
  int l = strlen(srcdest);
  if (l <= 4)
	return 0;

  char* p = srcdest + l - 1;
  if (*p != ')')
	return 0;

  if (!strcmp(p - 2, "(X)"))
	{
	  p -= 3;
	  if (*p == ' ')	// known to be >= srcdest: srcdest + (>4) - 1 - 3
		{
		  *p = 0;
		  return -1;
		}
	}

  for (--p; p >= srcdest && *p >= '0' && *p <= '9'; --p)
	{}

  if (p < srcdest || *p != '(' || *(p + 1) == ')')
	return 0;

  int loy = atoi(p + 1);

  --p;
  if (p < srcdest || *p != ' ')
	return 0;

  *p = 0;
  return loy;
}

COLORREF
get_title_text_override_color(PicHandleNames framenum, void* alpha_xform,
							  int light_visible, COLORREF light_colorref,
							  int white_black_visible, COLORREF white_black_colorref,
							  int colorless_visible, COLORREF colorless_colorref)
{
  if (light_visible || white_black_visible || colorless_visible)
	{
	  framenum = BASE_FRAME(framenum);
	  if (colorless_visible
		  && (framenum == FRAME_MODERN_LAND_COLORLESS
			  || framenum == FRAME_MODERN_ARTIFACT_LAND_COLORLESS
			  || framenum == FRAME_MODERN_SPELL_COLORLESS
			  || framenum == CARDOV_MODERN_COLORLESS
			  || alpha_xform))
		return colorless_colorref;
	  else
		switch (framenum)
		  {
			case FRAME_MODERN_SPELL_HYBRID_WHITE_BLACK:
			  if (white_black_visible)
				return white_black_colorref;

			  // otherwise, fall through
			case FRAME_MODERN_LAND_COLORLESS:
			case FRAME_MODERN_LAND_WHITE:
			case FRAME_MODERN_LAND_GOLD_MIN ... FRAME_MODERN_LAND_GOLD_MAX:
			case FRAME_MODERN_LAND_GOLD:

			case FRAME_MODERN_ARTIFACT_LAND_COLORLESS:
			case FRAME_MODERN_ARTIFACT_LAND_WHITE:
			case FRAME_MODERN_ARTIFACT_LAND_GOLD_MIN ... FRAME_MODERN_ARTIFACT_LAND_GOLD_MAX:
			case FRAME_MODERN_ARTIFACT_LAND_GOLD:

			case FRAME_MODERN_ARTIFACT_COLORLESS:
			case FRAME_MODERN_ARTIFACT_WHITE:
			case FRAME_MODERN_ARTIFACT_GOLD_MIN ... FRAME_MODERN_ARTIFACT_GOLD_MAX:
			case FRAME_MODERN_ARTIFACT_GOLD:

			case FRAME_MODERN_SPELL_COLORLESS:
			case FRAME_MODERN_SPELL_WHITE:
			case FRAME_MODERN_SPELL_GOLD_MIN ... FRAME_MODERN_SPELL_GOLD_MAX:
			case FRAME_MODERN_SPELL_GOLD:
			case FRAME_MODERN_SPELL_HYBRID_WHITE_BLUE:
			case FRAME_MODERN_SPELL_HYBRID_RED_WHITE:
			case FRAME_MODERN_SPELL_HYBRID_GREEN_WHITE:
			  if (light_visible)
				return light_colorref;
			  break;

			default:
			  if (alpha_xform)	// CARDOV_*_COLORLESS
				{
				  if (colorless_visible)
					return colorless_colorref;
				  if (light_visible)
					return light_colorref;
				}
			  break;
		  }
	}
  return (COLORREF)(-1);
}

static COLORREF
get_fullcard_rulestext_color(PicHandleNames framenum)
{
  if (cfg->fullcard_rules_on_dark_land_visible || cfg->fullcard_rules_on_white_black_land_visible)
	switch (BASE_FRAME(framenum))
	  {
		case FRAME_MODERN_LAND_GOLD_WHITE_BLACK:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_WHITE_BLACK:
		  if (cfg->fullcard_rules_on_white_black_land_visible)
			return cfg->fullcard_rules_on_white_black_land_color.colorref;
		  // otherwise, fall through
		case FRAME_MODERN_LAND_BLACK:				case FRAME_MODERN_ARTIFACT_LAND_BLACK:
		case FRAME_MODERN_LAND_BLUE:				case FRAME_MODERN_ARTIFACT_LAND_BLUE:
		case FRAME_MODERN_LAND_GREEN:				case FRAME_MODERN_ARTIFACT_LAND_GREEN:
		case FRAME_MODERN_LAND_RED:					case FRAME_MODERN_ARTIFACT_LAND_RED:
		case FRAME_MODERN_LAND_GOLD_BLUE_BLACK:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLUE_BLACK:
		case FRAME_MODERN_LAND_GOLD_BLACK_RED:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK_RED:
		case FRAME_MODERN_LAND_GOLD_RED_GREEN:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_RED_GREEN:
		case FRAME_MODERN_LAND_GOLD_BLACK_GREEN:	case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLACK_GREEN:
		case FRAME_MODERN_LAND_GOLD_GREEN_BLUE:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_GREEN_BLUE:
		case FRAME_MODERN_LAND_GOLD_BLUE_RED:		case FRAME_MODERN_ARTIFACT_LAND_GOLD_BLUE_RED:
		  if (cfg->fullcard_rules_on_dark_land_visible)
			return cfg->fullcard_rules_on_dark_land_color.colorref;
		  break;
	  }

  return cfg->fullcard_rulestext_color.colorref;
}

static int
has_manacost(const card_ptr_t* cp)
{
  return ((int8_t)cp->card_type != CP_TYPE_LAND
		  && (int8_t)cp->card_type != CP_TYPE_TOKEN
		  && (int8_t)cp->card_type != CP_TYPE_SCHEME
		  && (int8_t)cp->card_type != CP_TYPE_PLANE
		  && (int8_t)cp->card_type != CP_TYPE_NONE);
}

typedef enum
  {
	TFM_NORMAL,
	TFM_COLORLESS,
	TFM_COLORLESS_ALSO_NORMAL_FULLCARD_ART,
	TFM_CARDBACK,
  } TranslucentFrameMode;

static TranslucentFrameMode
draw_art_first(HDC hdc, uint32_t csvid, int pic_version, PicHandleNames framenum, int* suppress_later_art, int* big_art_exists)
{
  int colorless_option, cardback;
  if (framenum == CARDBACK)
	{
	  colorless_option = cfg->frames_facedown_creature;
	  cardback = 1;
	  framenum = FRAME_MODERN_SPELL_COLORLESS;
	}
  else
	{
	  colorless_option = cfg->frames_translucent_colorless;
	  cardback = 0;
	}

#pragma message "suppress for nyx frames"
  if (BASE_FRAME(framenum) == FRAME_MODERN_SPELL_COLORLESS && colorless_option >= 0)
	{
	  *suppress_later_art = 1;

	  if (cardback)
		{
		  make_gpic_from_pic(CARDBACK);
		  gdip_blt_whole(hdc, &cfg->fullcard_frame, CARDBACK, NULL);
		  *big_art_exists = 1;
		  return TFM_CARDBACK;
		}
	  else
		{
		  *big_art_exists = draw_fullcard_art(hdc, &cfg->fullcard_frame, csvid, pic_version);
		  if (colorless_option & FRAMES_TRANSLUCENT_COLORLESS_ALSO_NORMAL_FULLCARD_ART)
			return TFM_COLORLESS_ALSO_NORMAL_FULLCARD_ART;
		  else
			return TFM_COLORLESS;
		}
	}
  else
	{
	  if (cfg->fullcard_art_first)
		{
		  *big_art_exists = draw_fullcard_art(hdc, &cfg->fullcard_art, csvid, pic_version);
		  *suppress_later_art = 1;
		}

	  return TFM_NORMAL;
	}
}

static void
draw_fullcard_frame(HDC hdc, PicHandleNames framenum, GpImageAttributes** alpha_xform, TranslucentFrameMode translucent_frame)
{
  if (translucent_frame == TFM_NORMAL)
	gdip_blt_whole(hdc, &cfg->fullcard_frame, framenum, NULL);
  else
	{
	  framenum = FRAMEPART_FROM_BASE(CARDOV_MODERN_COLORLESS, cfg);
	  *alpha_xform = cfg->colorless_alpha_xform;
	  gdip_blt_whole(hdc, &cfg->fullcard_frame, framenum, *alpha_xform);
	}
}

static COLORREF
draw_fullcard_dfcsym_tombstone_title(HDC hdc, const card_ptr_t* cp, const Rarities* r, PicHandleNames framenum,
									 GpImageAttributes* alpha_xform)
{
  RECT rect_title;
  CopyRect(&rect_title, &cfg->fullcard_title);

  if (cfg->fullcard_dfc_visible && r->dfc_symbol <= DFC_SYMBOL_MAX)
	blt_dfc_symbol(hdc, &cfg->fullcard_dfc, r->dfc_symbol);

  int tombstone_drawn = 0;
  if (cfg->fullcard_tombstone_visible && has_graveyard_ability(cp))
	{
	  rect_title.left += cfg->fullcard_tombstone_title_offset;
	  gdip_blt_whole(hdc, &cfg->fullcard_tombstone, FRAMEPART_FROM_BASE(TOMBSTONE_MODERN, cfg), NULL);
	  tombstone_drawn = 1;
	}

  if (cfg->fullcard_type_icon_visible && !(tombstone_drawn && cfg->fullcard_type_icon_suppressed_by_tombstone))
	blt_type_icon(hdc, &cfg->fullcard_type_icon, cp, framenum);

  int rt = 0;
  if (has_manacost(cp) && cfg->fullcard_curve_mana_diameter <= 0)
	rt = manacost_width(hdc, &cfg->fullcard_mana, cp);
  if (rt > 0)
	rt = cfg->fullcard_mana.right - rt - 6; // wanted space between name and mana cost
  else
	rt = cfg->fullcard_title.right;

  SIZE textsize;
  int fnt, len = strlen(cp->name);
  for (fnt = BIGCARDTITLE_FONT; fnt < BIGCARDTITLE_FONT9; ++fnt)
	{
	  SelectObject(hdc, fonts[fnt]);
	  GetTextExtentPoint32(hdc, cp->name, len, &textsize);
	  if (rect_title.left + textsize.cx <= rt)
		goto found_bigcardtitle_font;
	}
  SelectObject(hdc, fonts[BIGCARDTITLE_FONT9]);
 found_bigcardtitle_font:;

  COLORREF title_text_override_color =
	get_title_text_override_color(framenum, alpha_xform,
								  cfg->fullcard_title_on_light_visible,
								  cfg->fullcard_title_on_light_color.colorref,
								  cfg->fullcard_title_on_white_black_visible,
								  cfg->fullcard_title_on_white_black_color.colorref,
								  cfg->fullcard_title_on_colorless_visible,
								  cfg->fullcard_title_on_colorless_color.colorref);

  int pos = (cfg->fullcard_title_center
			 ? (rect_title.left + rect_title.right) / 2
			 : rect_title.left);

  SetTextAlign(hdc, cfg->fullcard_title_center ? TA_CENTER : TA_LEFT);

  draw_text_with_shadow_at_x(hdc, &rect_title, pos, &cfg->fullcard_title_txt, title_text_override_color, cp->name, len);

  if (cfg->fullcard_title_center)
	SetTextAlign(hdc, TA_LEFT);

  return title_text_override_color;
}

static int
draw_fullcard_type(HDC hdc, const card_ptr_t* cp, COLORREF title_text_override_color, int is_a_planeswalker, int is_a_creature)
{
  int loyalty = 0;

  if ((cp->card_type != (uint32_t)(-1) && cp->card_type != CP_TYPE_NONE)	// deliberately non-narrowed
	  || parent == PARENT_SHANDALAR)
	{
	  char buf[100];	// longest observed: Jeska, Warrior Adept at 44

	  strip_hack_and_sleight_symbols(buf, cp->type_text);

	  if (is_a_planeswalker && !is_a_creature && cfg->fullcard_show_loyalty_base)
		loyalty = strip_planeswalker_base_loyalty(buf);

	  SIZE textsize;
	  int fnt, len = strlen(buf);
	  for (fnt = BIGCARDTYPE_FONT; fnt < BIGCARDTYPE_FONT9; ++fnt)
		{
		  SelectObject(hdc, fonts[fnt]);
		  GetTextExtentPoint32(hdc, buf, len, &textsize);
		  if (cfg->fullcard_type.left + textsize.cx <= cfg->fullcard_type.right)
			goto found_bigcardtype_font;
		}
	  SelectObject(hdc, fonts[BIGCARDTYPE_FONT9]);
	found_bigcardtype_font:;

	  int pos = (cfg->fullcard_type_center
				 ? (cfg->fullcard_type.left + cfg->fullcard_type.right) / 2
				 : cfg->fullcard_type.left);

	  SetTextAlign(hdc, cfg->fullcard_type_center ? TA_CENTER : TA_LEFT);

	  draw_text_with_shadow_at_x(hdc, &cfg->fullcard_type, pos, &cfg->fullcard_type_txt, title_text_override_color,
								 buf, len);

	  if (cfg->fullcard_type_center)
		SetTextAlign(hdc, TA_LEFT);
	}

  return loyalty;
}

typedef struct
{
  double h;
  double s;
  double v;
} Hsv;

static void
bgr_to_hsv(const uint8_t* bgr, Hsv* hsv)
{
  double r = bgr[2] / 255.0;
  double g = bgr[1] / 255.0;
  double b = bgr[0] / 255.0;

  float k = 0.0;

  if (g < b)
    {
	  double tmp = g;
	  g = b;
	  b = tmp;
	  k = -1.0;
    }

  if (r < g)
    {
	  double tmp = g;
	  g = r;
	  r = tmp;
	  k = -2.0 / 6.0 - k;
    }

  double chroma = r - MIN(g, b);
  hsv->h = fabs(k + (g - b) / (6.0 * chroma + 1e-20f));
  hsv->s = chroma / (r + 1e-20f);
  hsv->v = r;
}

static void
hsv_to_bgr(const Hsv* hsv, uint8_t* bgr)
{
  double h = 6 * hsv->h, s = hsv->s, v = hsv->v;

  double fract = h - floor(h);

  double p = v * (1 - s);
  double q = v * (1 - s * fract);
  double t = v * (1 - s * (1 - fract));

  double r, g, b;

  if      (0 <= h && h < 1)
	{ r = v; g = t; b = p; }
  else if (1 <= h && h < 2)
	{ r = q; g = v; b = p; }
  else if (2 <= h && h < 3)
	{ r = p; g = v; b = t; }
  else if (3 <= h && h < 4)
	{ r = p; g = q; b = v; }
  else if (4 <= h && h < 5)
	{ r = t; g = p; b = v; }
  else if (5 <= h && h < 6)
	{ r = v; g = p; b = q; }
  else
	r = g = b = 0;

  bgr[2] = r * 255 + 0.5;
  bgr[1] = g * 255 + 0.5;
  bgr[0] = b * 255 + 0.5;
}

void make_frame(PicHandleNames frame);

static void
draw_fullcard_watermark(HDC hdc, const Rarities* r, const RECT* rect_rulestext)
{
  if (!r || r->watermark == 0xFF)
	return;

  Watermark* wmark = &watermarks[r->watermark];

  if (wmark->expansions && !cfg->fullcard_watermark_outside_set && !wmark->expansions[r->default_expansion])
	return;

  RECT rect_watermark;
  CopyRect(&rect_watermark, &cfg->fullcard_watermark);
  if (rect_rulestext->top < cfg->fullcard_rulestext.top)	// i.e., it was expanded
	{
	  int delta = (cfg->fullcard_rulestext.top - rect_rulestext->top) / 2;
	  rect_watermark.top -= delta;
	  rect_watermark.bottom -= delta;
	}

  int x, y;
  x = wmark->col * watermark_size_x;
  y = wmark->row * watermark_size_y;

  if (!wmark->saturation)
	{
	  gdip_blt(hdc, &rect_watermark, WATERMARKS, x, y, watermark_size_x, watermark_size_y, NULL);
	  return;
	}

  // Darken and saturate.  It's rather appalling how much effort it takes just to read back the data properly.

  int width = rect_watermark.right - rect_watermark.left;
  int height = rect_watermark.bottom - rect_watermark.top;

  // Create a new bitmap to scale the watermark to
  GpBitmap* bmp_wmark = NULL;
  GdipCreateBitmapFromScan0(width, height, width * 4, PixelFormat32bppARGB, NULL, &bmp_wmark);
  GpGraphics* gfx_wmark = NULL;
  GdipGetImageGraphicsContext(bmp_wmark, &gfx_wmark);
  GdipSetInterpolationMode(gfx_wmark, InterpolationModeHighQualityBicubic);
  if (!gpics[WATERMARKS])
	make_frame(WATERMARKS);
  GdipDrawImageRectRectI(gfx_wmark, gpics[WATERMARKS],
						 0, 0, width, height,
						 x, y, watermark_size_x, watermark_size_y,
						 UnitPixel, NULL, NULL, NULL);

  // crop from existing image
  HDC memdc = CreateCompatibleDC(hdc);
  BITMAPINFO bmi;
  bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = width * height * 4;
  bmi.bmiHeader.biClrUsed = 0;
  bmi.bmiHeader.biClrImportant = 0;
  uint8_t* bmbits;
  HBITMAP hbmp = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, (void**)(&bmbits), NULL, 0);
  HANDLE oldbmp = SelectObject(memdc, hbmp);
  BitBlt(memdc, 0, 0, width, height, hdc, rect_watermark.left, rect_watermark.top, SRCCOPY);

  GpRect rect;
  rect.X = rect.Y = 0;
  rect.Width = width;
  rect.Height = height;

  BitmapData wmarkdata;
  GdipBitmapLockBits(bmp_wmark, &rect, ImageLockModeRead, PixelFormat32bppARGB, &wmarkdata);

  // wmarkdata now contains data from watermark.  We use its alpha, and modify rgb from the existing image.
  INT wmark_stride = abs(wmarkdata.Stride);
  uint8_t* prevbits = bmbits;
  for (int yp = 0; yp < height; ++yp)
	{
	  uint8_t* wmarkbits = (uint8_t*)wmarkdata.Scan0 + yp * wmark_stride;
	  for (int xp = 0; xp < width; ++xp, wmarkbits += 4, prevbits += 4)
		if (wmarkbits[3])	// alpha
		  {
			int alpha = wmarkbits[3];
			alpha *= watermark_saturation_opacity;
			if (alpha < 0)
			  alpha = 0;
			else if (alpha > 255)
			  alpha = 255;
			wmarkbits[3] = alpha;

			Hsv hsv;
			bgr_to_hsv(prevbits, &hsv);
			hsv.v *= watermark_saturation_darken;
			if (hsv.v > 1.0)
			  hsv.v = 1.0;
			hsv.s = sqrt(sqrt(hsv.s));
			hsv_to_bgr(&hsv, wmarkbits);
		  }
	  // else alpha == 0, so we don't care.
	}

  GdipBitmapUnlockBits(bmp_wmark, &wmarkdata);

  // Now blit it back
  GpGraphics* gfxhdc = NULL;
  GdipCreateFromHDC(hdc, &gfxhdc);
  GdipSetInterpolationMode(gfxhdc, InterpolationModeHighQualityBicubic);
  GdipDrawImageRectI(gfxhdc, bmp_wmark,
					 rect_watermark.left, rect_watermark.top,
					 rect_watermark.right - rect_watermark.left, rect_watermark.bottom - rect_watermark.top);
  GdipDeleteGraphics(gfxhdc);

  GdipDeleteGraphics(gfx_wmark);
  GdipDisposeImage(bmp_wmark);

  SelectObject(memdc, oldbmp);
  DeleteDC(memdc);
}

#define MAX_LOYALTY_MODS 5

static int
fullcard_exclude_expanded_text_box(HDC hdc, const card_ptr_t* cp, char* inout_rules_text, RECT* rect_rulestext,
								   RECT* rect_rulesbox, int is_a_planeswalker)
{
  int txt_hgt = 0;

  strip_hack_and_sleight_symbols(inout_rules_text, cp->rules_text);

  if (*inout_rules_text)
	{
	  SelectObject(hdc, fonts[BIGCARDTEXT_FONT]);

	  TEXTMETRIC metrics;
	  GetTextMetrics(hdc, &metrics);

	  // This will just get thrown away, to be recomputed later when actually drawing text
	  LoyaltyCost loyalty_costs[MAX_LOYALTY_MODS + 1];	// last is a sentinel
	  LoyaltyCost* lc = is_a_planeswalker && cfg->fullcard_show_loyalty_cost ? loyalty_costs : NULL;

	  txt_hgt += HIWORD(calc_draw_mana_text(hdc, rect_rulestext, inout_rules_text, 1, 0, lc)) + metrics.tmHeight / 2;
	}

  if (*cp->flavor_text)
	{
	  SelectObject(hdc, fonts[FLAVOR_FONT]);

	  txt_hgt += HIWORD(calc_draw_mana_text(hdc, rect_rulestext, cp->flavor_text, 0, 1, NULL));
	}

  if (rect_rulestext->bottom - rect_rulestext->top < txt_hgt)
	{
	  int diff = rect_rulestext->top - rect_rulesbox->top;
	  rect_rulestext->top = rect_rulestext->bottom - txt_hgt;
	  rect_rulesbox->top = rect_rulestext->top - diff;

	  int savedc = SaveDC(hdc);
	  ExcludeClipRect(hdc, rect_rulesbox->left, rect_rulesbox->top + 1, rect_rulesbox->right, rect_rulesbox->bottom - 1);
	  return savedc;
	}
  else
	return 0;	// fits, no need to expand
}

static void
fullcard_expand_text_box(HDC hdc, RECT* rect_rulesbox, PicHandleNames framenum, GpImageAttributes* alpha_xform, TranslucentFrameMode translucent_frame)
{
  if (translucent_frame != TFM_NORMAL)
	framenum = FRAMEPART_FROM_BASE(CARDOV_MODERN_COLORLESS, cfg);

  UINT width, height;
  GdipGetImageWidth(gpics[framenum], &width);
  GdipGetImageHeight(gpics[framenum], &height);

  // Blit from full width to full width with clipping instead of just to rect_rulesbox to avoid artifacts from the horrid nearest-neighbor filtering.
  int savedc = SaveDC(hdc);
  IntersectClipRect(hdc, rect_rulesbox->left, rect_rulesbox->top + 1, rect_rulesbox->right, rect_rulesbox->bottom - 1);
  rect_rulesbox->left = cfg->fullcard_frame.left;
  rect_rulesbox->right = cfg->fullcard_frame.right;

  gdip_blt(hdc, rect_rulesbox, framenum,
		   0, (height * cfg->fullcard_expand_top / 10000),
		   width, (height * (cfg->fullcard_expand_height) / 10000), alpha_xform);
  RestoreDC(hdc, savedc);
}

static void
draw_fullcard_loyalty_costs(HDC hdc, const RECT* rect_rulestext, LoyaltyCost* loyalty_costs, GpImageAttributes* alpha_xform)
{
  RECT rect_loyalty_cost;
  rect_loyalty_cost.left = cfg->fullcard_loyalty_cost.left;
  rect_loyalty_cost.right = cfg->fullcard_loyalty_cost.right;

  RECT rect_loyalty_cost_box;
  rect_loyalty_cost_box.left = cfg->fullcard_loyalty_cost_box.left;
  rect_loyalty_cost_box.right = cfg->fullcard_loyalty_cost_box.right;
  int loyalty_cost_box_hgt = cfg->fullcard_loyalty_cost_box.bottom - cfg->fullcard_loyalty_cost_box.top;

  SelectObject(hdc, fonts[BIGCARDLOYALTYCOST_FONT]);
  SetTextAlign(hdc, TA_CENTER);

  int i;
  for (i = 0; i < MAX_LOYALTY_MODS && loyalty_costs[i].top != INT_MIN; ++i)
	{
	  if (loyalty_costs[i].top >= rect_rulestext->bottom - cfg->fullcard_loyalty_cull)
		continue;

	  rect_loyalty_cost_box.top = (loyalty_costs[i].top + loyalty_costs[i].bottom) / 2 + cfg->fullcard_loyalty_cost_box.top;
	  rect_loyalty_cost_box.bottom = rect_loyalty_cost_box.top + loyalty_cost_box_hgt;
	  if (rect_loyalty_cost_box.bottom > rect_rulestext->bottom - cfg->fullcard_loyalty_lowest)
		{
		  rect_loyalty_cost_box.top += (rect_rulestext->bottom - cfg->fullcard_loyalty_lowest - rect_loyalty_cost_box.bottom);
		  rect_loyalty_cost_box.bottom = rect_rulestext->bottom - cfg->fullcard_loyalty_lowest;
		}

	  rect_loyalty_cost.top = rect_loyalty_cost_box.top - cfg->fullcard_loyalty_cost_box.top + cfg->fullcard_loyalty_cost.top;
	  rect_loyalty_cost.bottom = rect_loyalty_cost_box.bottom - cfg->fullcard_loyalty_cost_box.bottom + cfg->fullcard_loyalty_cost.bottom;

	  PicHandleNames frm;
	  if (loyalty_costs[i].txt[0] == '-' || loyalty_costs[i].txt[0] == '\227')
		frm = FRAMEPART_FROM_BASE(LOYALTY_MINUS_MODERN, cfg);
	  else if (loyalty_costs[i].txt[0] == '+')
		frm = FRAMEPART_FROM_BASE(LOYALTY_PLUS_MODERN, cfg);
	  else
		frm = FRAMEPART_FROM_BASE(LOYALTY_ZERO_MODERN, cfg);

	  gdip_blt_whole(hdc, &rect_loyalty_cost_box, frm, alpha_xform);

	  int pos = (rect_loyalty_cost.right + rect_loyalty_cost.left) / 2;

	  draw_text_with_shadow_at_x(hdc, &rect_loyalty_cost, pos, &cfg->fullcard_loyalty_txt, -1, loyalty_costs[i].txt, -1);
	}

  SetTextAlign(hdc, TA_LEFT);
}

static void
draw_fullcard_textbox_contents(HDC hdc, const card_ptr_t* cp, RECT* rect_rulestext, char* rules_text,
							   const Rarities* r, PicHandleNames framenum, GpImageAttributes* alpha_xform,
							   int is_a_planeswalker)
{
  if (!*rules_text)	// i.e., not already computed
	strip_hack_and_sleight_symbols(rules_text, cp->rules_text);

  SetTextColor(hdc, get_fullcard_rulestext_color(framenum));

  LoyaltyCost loyalty_costs[MAX_LOYALTY_MODS + 1];	// last is a sentinel
  int show_loyalty_costs = is_a_planeswalker && cfg->fullcard_show_loyalty_cost;

  // Watermark - draw after calculating text height and maybe expanding textbox, but before actually drawing text
  draw_fullcard_watermark(hdc, r, rect_rulestext);

  RECT rect_flavor_text;
  CopyRect(&rect_flavor_text, rect_rulestext);

  if (*rules_text)
	{
	  SelectObject(hdc, fonts[BIGCARDTEXT_FONT]);

	  TEXTMETRIC metrics;
	  GetTextMetrics(hdc, &metrics);

	  rect_flavor_text.top += HIWORD(draw_mana_text(hdc, rect_rulestext, rules_text, 1, 1, 0, show_loyalty_costs ? loyalty_costs : NULL)) + metrics.tmHeight / 3;

	  if (show_loyalty_costs)
		draw_fullcard_loyalty_costs(hdc, rect_rulestext, loyalty_costs, alpha_xform);
	}

  SelectObject(hdc, fonts[FLAVOR_FONT]);

  if (*cp->flavor_text)
	draw_mana_text(hdc, &rect_flavor_text, cp->flavor_text, 1, 0, 1, NULL);
}

static void
draw_fullcard_power_toughness(HDC hdc, PicHandleNames framenum, GpImageAttributes* alpha_xform, COLORREF override_color)
{
  SelectObject(hdc, fonts[BIGCARDPT_FONT]);

  COLORREF pt_color = cfg->fullcard_powertoughness_txt.color.colorref;
  if (cfg->fullcard_powertoughness_box_visible
	  && blt_powertoughness_box(hdc, &cfg->fullcard_powertoughness_box, framenum, alpha_xform)
	  && cfg->fullcard_title_on_light_visible)
	pt_color = cfg->fullcard_title_on_light_color.colorref;

  if (override_color != (COLORREF)(-1))
	pt_color = override_color;

  char buf[100];
  *buf = 0;

  const char* star = "*";
  const char* half = "\275";
  const char* exp2 = "\262";

  // Power
  if (hack_power < 100)
	sprintf(buf, "%d/", hack_power);
  else if (hack_power == 100)
	sprintf(buf, "%s/", star);
  else if (hack_power < 200)
	sprintf(buf, "%d+%s/", hack_power - 100, star);
  else if (hack_power < 300)
	sprintf(buf, "%d-%s" "/", hack_power - 200, star);
  else if (hack_power < 400)
	{
	  if (hack_toughness == 300)
		sprintf(buf, "%s/", half);
	  else
		sprintf(buf, "%d%s/", hack_power - 300, half);
	}
  else // hack_power == 500
	sprintf(buf, "%s%s/", star, exp2);

  char* p = buf + strlen(buf);

  // Toughness
  if (hack_toughness < 100)
	sprintf(p, "%d", hack_toughness);
  else if (hack_toughness == 100)
	sprintf(p, "%s", star);
  else if (hack_toughness < 200)
	sprintf(p, "%d+%s", hack_toughness - 100, star);
  else if (hack_toughness < 300)
	sprintf(p, "%d-%s", hack_toughness - 200, star);
  else if (hack_toughness < 400)
	{
	  if (hack_toughness == 300)
		sprintf(p, "%s", half);
	  else
		sprintf(p, "%d%s", hack_toughness - 300, half);
	}
  else // hack_toughness == 500
	sprintf(p, "%s%s", star, exp2);

  int pos = (cfg->fullcard_powertoughness_center
			 ? (cfg->fullcard_powertoughness.right + cfg->fullcard_powertoughness.left) / 2
			 : cfg->fullcard_powertoughness.right);
  SetTextAlign(hdc, cfg->fullcard_powertoughness_center ? TA_CENTER : TA_RIGHT);

  draw_text_with_shadow_at_x(hdc, &cfg->fullcard_powertoughness, pos, &cfg->fullcard_powertoughness_txt, pt_color, buf, -1);
}

static void
draw_fullcard_base_loyalty(HDC hdc, int loyalty, GpImageAttributes* alpha_xform)
{
  gdip_blt_whole(hdc, &cfg->fullcard_loyalty_base_box, FRAMEPART_FROM_BASE(LOYALTY_BASE_MODERN, cfg), alpha_xform);

  SelectObject(hdc, fonts[BIGCARDLOYALTYBASE_FONT]);

  int pos = (cfg->fullcard_loyalty_base.right + cfg->fullcard_loyalty_base.left) / 2;

  SetTextAlign(hdc, TA_CENTER);
  char buf[10];
  if (loyalty == -1)	// X
	strcpy(buf, "X");
  else
	sprintf(buf, "%d", loyalty);

  draw_text_with_shadow_at_x(hdc, &cfg->fullcard_loyalty_base, pos, &cfg->fullcard_loyalty_txt, -1, buf, -1);
}

static void
draw_fullcard_vanguard(HDC hdc, const card_ptr_t* cp)
{
  SetTextAlign(hdc, TA_CENTER);
  char buf[10];
  int pos, fnt, len;
  SIZE textsize;

  TextWithShadow tws;
  tws.color = tws.shadow_color = cfg->fullcard_vanguard_color;
  tws.shadow_offset.x = tws.shadow_offset.y = 0;
  tws.shadow_visible = 0;

  len = sprintf(buf, "%+d", (int8_t)(cp->ai_power));
  for (fnt = BIGCARDVANGUARD_FONT; fnt < BIGCARDVANGUARD_FONT3; ++fnt)
	{
	  SelectObject(hdc, fonts[fnt]);
	  GetTextExtentPoint32(hdc, buf, len, &textsize);
	  if (cfg->fullcard_vanguard_hand.left + textsize.cx <= cfg->fullcard_vanguard_hand.right)
		goto found_hand_font;
	}
  SelectObject(hdc, fonts[BIGCARDVANGUARD_FONT3]);
 found_hand_font:;
  pos = (cfg->fullcard_vanguard_hand.right + cfg->fullcard_vanguard_hand.left) / 2;
  draw_text_with_shadow_at_x(hdc, &cfg->fullcard_vanguard_hand, pos, &tws, -1, buf, -1);

  len = sprintf(buf, "%+d", (int8_t)(cp->ai_toughness));
  for (fnt = BIGCARDVANGUARD_FONT; fnt < BIGCARDVANGUARD_FONT3; ++fnt)
	{
	  SelectObject(hdc, fonts[fnt]);
	  GetTextExtentPoint32(hdc, buf, len, &textsize);
	  if (cfg->fullcard_vanguard_life.left + textsize.cx <= cfg->fullcard_vanguard_life.right)
		goto found_life_font;
	}
  SelectObject(hdc, fonts[BIGCARDVANGUARD_FONT3]);
 found_life_font:;
  pos = (cfg->fullcard_vanguard_life.right + cfg->fullcard_vanguard_life.left) / 2;
  draw_text_with_shadow_at_x(hdc, &cfg->fullcard_vanguard_life, pos, &tws, -1, buf, -1);
}

static border_t
get_border_color_from_rarity(const Rarities* r, uint32_t csvid)
{
  switch (r->default_expansion)
	{
	  case EXPANSION_UNLIMITED_EDITION:
	  case EXPANSION_REVISED_EDITION:
	  case EXPANSION_FOURTH_EDITION:
	  case EXPANSION_CHRONICLES:
	  case EXPANSION_FIFTH_EDITION:
	  case EXPANSION_CLASSIC_SIXTH_EDITION:
	  case EXPANSION_PORTAL_THREE_KINGDOMS:
	  case EXPANSION_BATTLE_ROYALE_BOX_SET:
	  case EXPANSION_STARTER_2000:
	  case EXPANSION_BEATDOWN_BOX_SET:
	  case EXPANSION_SEVENTH_EDITION:
	  case EXPANSION_EIGHTH_EDITION:
	  case EXPANSION_NINTH_EDITION:
		return BORDER_WHITE;

	  case EXPANSION_UNGLUED:
	  case EXPANSION_UNHINGED:
	  case EXPANSION_UNSTABLE:
		return BORDER_SILVER;

	  case EXPANSION_PROMO:
		// It's complicated.

		// These are black-bordered.
		switch (csvid)
		  {
			// book inserts, never reprinted
			case CARD_ID_NALATHNI_DRAGON:
			case CARD_ID_SEWERS_OF_ESTARK:
			case CARD_ID_WINDSEEKER_CENTAUR:

			// promo-only planes, not in Planechase or Planechase 2012
			case CARD_ID_CELESTINE_REEF:
			case CARD_ID_HORIZON_BOUGHS:
			case CARD_ID_MIRRORED_DEPTHS:
			case CARD_ID_TEMBER_CITY:

			// promo-only schemes, not in Archenemy or Archenemy: Nicol Bolas
			case CARD_ID_DRENCH_THE_SOIL_IN_THEIR_BLOOD:
			case CARD_ID_IMPRISON_THIS_INSOLENT_WRETCH:
			case CARD_ID_PERHAPS_YOUVE_MET_MY_COHORT:
			case CARD_ID_PLOTS_THAT_SPAN_CENTURIES:
			case CARD_ID_YOUR_INESCAPABLE_DOOM:

			// Heroes of the Realm cards, though at least Dungeon Master should really have been silver.
			case CARD_ID_CHANDRA_GREMLIN_WRANGLER:
			case CARD_ID_DUNGEON_MASTER:
			case CARD_ID_NIRA_HELLKITE_DUELIST:

			// MTG:Arena New-Player-Experience cards, never printed on paper.
			case CARD_ID_ANGELIC_REWARD:
			case CARD_ID_BLINDING_RADIANCE:
			case CARD_ID_CONFRONT_THE_ASSAULT:
			case CARD_ID_CRUEL_CUT:
			case CARD_ID_FERAL_ROAR:
			case CARD_ID_GOBLIN_BRUISER:
			case CARD_ID_GOBLIN_GANG_LEADER:
			case CARD_ID_INSPIRING_COMMANDER:
			case CARD_ID_NIMBLE_PILFERER:
			case CARD_ID_OGRE_PAINBRINGER:
			case CARD_ID_RIVERS_FAVOR:
			case CARD_ID_SHORECOMBER_CRAB:
			case CARD_ID_SHRINE_KEEPER:
			case CARD_ID_SOULHUNTER_RAKSHASA:
			case CARD_ID_TACTICAL_ADVANTAGE:
			case CARD_ID_TITANIC_PELAGOSAUR:
			case CARD_ID_TREETOP_WARDEN:
			case CARD_ID_ZEPHYR_GULL:
			  return BORDER_BLACK;
		  }

		// Otherwise, it's a Happy Holidays or Hascon card.
		return BORDER_SILVER;

	  default:	// everything else is black-bordered
		return BORDER_BLACK;
	}
}

border_t
get_border_color(uint32_t csvid)
{
  return get_border_color_from_rarity(get_rarity(csvid), csvid);
}

static HBRUSH
create_border_brush(int big_art_style, const Rarities* r, uint32_t csvid, int* was_black_border)
{
  COLORREF col;
  if (big_art_style == 42)
	col = RGB(241,217,39);
  else
	switch (get_border_color_from_rarity(r, csvid))
	  {
		case BORDER_WHITE:
		  col = cfg->fullcard_white_border_color.colorref;
		  break;

		case BORDER_SILVER:
		  col = cfg->fullcard_silver_border_color.colorref;
		  break;

		default:
		  col = cfg->fullcard_black_border_color.colorref;
		  if (was_black_border)
			*was_black_border = 1;
		  break;
	  }

  return CreateSolidBrush(col);
}

static int
draw_fullcard_border_initial(HDC hdc, int big_art_style, const Rarities* r, uint32_t csvid)
{
  /* This is done before drawing anything else, to ensure there's no transparent strips left over no matter how
   * horribly Windows' drawing functions disagree on where the edges should be. */

  int was_black_border = 0;
  HBRUSH solid_brush = create_border_brush(big_art_style, r, csvid, &was_black_border);
  SelectObject(hdc, solid_brush);
  SelectObject(hdc, GetStockObject(NULL_PEN));

  // left
  Rectangle(hdc, cfg->fullcard_frame.left-2, cfg->fullcard_frame.top-2, cfg->fullcard_frame.left+50, cfg->fullcard_frame.bottom+2);
  // right
  Rectangle(hdc, cfg->fullcard_frame.right-50, cfg->fullcard_frame.top-2, cfg->fullcard_frame.right+2, cfg->fullcard_frame.bottom+2);
  // top
  Rectangle(hdc, cfg->fullcard_frame.left-2, cfg->fullcard_frame.top-2, cfg->fullcard_frame.right+2, cfg->fullcard_frame.top+50);
  // bottom
  Rectangle(hdc, cfg->fullcard_frame.left-2, cfg->fullcard_frame.bottom-50, cfg->fullcard_frame.right+2, cfg->fullcard_frame.bottom+2);

  if (solid_brush)
	DeleteObject(solid_brush);

  return was_black_border;
}

static void
draw_fullcard_border(HDC hdc, int big_art_style, const Rarities* r, uint32_t csvid)
{
  // Make very, very sure that that the edges are even, even if there are multiple visible frame elements abutting one (as in the futureshifted frames)
  ExcludeClipRect(hdc, cfg->fullcard_frame.left+2, cfg->fullcard_frame.top+2, cfg->fullcard_frame.right-2, cfg->fullcard_frame.bottom-2);

  HBRUSH solid_brush = create_border_brush(big_art_style, r, csvid, NULL);
  SelectObject(hdc, solid_brush);
  SelectObject(hdc, GetStockObject(NULL_PEN));

  RoundRect(hdc, 0, 0, 800, 1200, cfg->fullcard_rounding.x, cfg->fullcard_rounding.y);

  if (solid_brush)
	DeleteObject(solid_brush);
}

static void
draw_card_back_no_crit(HDC hdc, const RECT* dest_rect)
{
  RECT pic_rect;
  int borderx, bordery;

  borderx = (dest_rect->right - dest_rect->left) * 3 / 100;
  if (borderx < 1)
	borderx = 1;

  bordery = (dest_rect->bottom - dest_rect->top) / 50;
  if (bordery < 1)
	bordery = 1;

  FillRect(hdc, dest_rect, GetStockObject(BLACK_BRUSH));

  SetRect(&pic_rect,
		  dest_rect->left + borderx, dest_rect->top + bordery,
		  dest_rect->right - borderx, dest_rect->bottom - bordery);

  if (cardback_renderer == RENDERER_GDIPLUS)
	gdip_blt_whole(hdc, &pic_rect, CARDBACK, NULL);
  else
	{
	  int savedc = SaveDC(hdc);
	  SelectObject(*spare_hdc, pics[CARDBACK]);

	  BITMAP bmp;
	  GetObject(pics[CARDBACK], sizeof(BITMAP), &bmp);
	  int src_width = bmp.bmWidth;
	  int src_height = bmp.bmHeight;

	  StretchBlt(hdc,
				 dest_rect->left, dest_rect->top,
				 dest_rect->right - dest_rect->left, dest_rect->bottom - dest_rect->top,
				 *spare_hdc, 0, 0, src_width, src_height, SRCCOPY);

	  RestoreDC(hdc, savedc);
	}
}

int
DrawCardBack(HDC hdc, const RECT* dest_rect,
			 // An appalling hack to use this as an interface for other functions, but still safer than adding new imports.
			 unsigned int mode, int arg1, int arg2, int arg3, int arg4)
{
  int rval;

  if (!hdc || !dest_rect)
	return 0;

  EnterCriticalSection(critical_section_for_drawing);

  if ((int)hdc == -1 && (int)dest_rect == -1 && (mode & 0xFFFFFF00) == 0x7D260A00)
	{
	  switch (mode & 0xFF)
		{
		  case 1:
			draw_special_counters((HDC)arg1, (RECT*)arg2, arg3, arg4);
			rval = 0;
			break;

		  case 2:
			draw_standard_counters((HDC)arg1, (RECT*)arg2, arg3, arg4);
			rval = 0;
			break;

		  case 3:
			get_special_counter_rect((RECT*)arg1, (const RECT*)arg2, arg3);
			rval = 0;
			break;

		  default:
			rval = 0;
			break;
		}
	}
  else
	{
	  draw_card_back_no_crit(hdc, dest_rect);

	  rval = 1;
	}

  LeaveCriticalSection(critical_section_for_drawing);
  return rval;
}

int
DrawFullCard(HDC hdc, const RECT* dest_rect, const card_ptr_t* cp, int pic_version, int big_art_style,
			 int expand_text_box, const char* illus)
{
  /* big_art_style is variously 0, 2, 17, or 18 when called from magic.exe or shandalar.exe, depending on the calling
   * function.
   *
   * When the low byte is set to 42, it's from Shandalar.dll's wndproc_GraveyardCards() and means to draw the border
   * yellow instead of black.  When the low byte is 43, it's also from Shandalar.dll's wndproc_GraveyardCards(), but
   * draw the border normally.  In both cases, BYTE2() is a counter_t, and BYTE3() is a number of counters. */
  if (!hdc || !dest_rect || !cp)
	return 0;

  (void)illus;

  counter_t counter_type;
  int counters;
  if (BYTE0(big_art_style) >= 42
	  && BYTE0(big_art_style) <= 44)
	{
	  counter_type = (counter_t)(BYTE2(big_art_style));
	  counters = BYTE3(big_art_style);
	  big_art_style = BYTE0(big_art_style);
	  if (big_art_style == 44)	// Yet another special case - draw the card back, maybe with counters on it.  Sigh.
		{
		  EnterCriticalSection(critical_section_for_drawing);

		  draw_card_back_no_crit(hdc, dest_rect);

		  if (counters > 0)
			{
			  int savedc = SaveDC(hdc);
			  SetMapMode(hdc, MM_ISOTROPIC);
			  SetWindowExtEx(hdc, 800, 1200, NULL);

			  SetViewportExtEx(hdc, dest_rect->right - dest_rect->left, dest_rect->bottom - dest_rect->top, NULL);
			  SetWindowOrgEx(hdc, 400, 600, NULL);
			  SetViewportOrgEx(hdc,
							   (dest_rect->right + dest_rect->left) / 2, (dest_rect->bottom + dest_rect->top) / 2,
							   NULL);
			  RECT rc;
			  rc.left = 0;
			  rc.right = 800;
			  rc.top = 0;
			  rc.bottom = 1200;
			  draw_counters_for_fullcard(hdc, &rc, counter_type, counters);

			  RestoreDC(hdc, savedc);
			}

		  LeaveCriticalSection(critical_section_for_drawing);
		  return 1;
		}
	}
  else
	{
	  counter_type = COUNTER_invalid;
	  counters = 0;
	}

  EnterCriticalSection(critical_section_for_drawing);

  if (configs[CFG_BASE].fullcard_reread_config_on_redraw)
	{
	  read_cfg();
	  assign_default_rarities();
	}

  int savedc = SaveDC(hdc);
  SetMapMode(hdc, MM_ISOTROPIC);
  SetWindowExtEx(hdc, 800, 1200, NULL);

  SetViewportExtEx(hdc, dest_rect->right - dest_rect->left, dest_rect->bottom - dest_rect->top, NULL);
  SetWindowOrgEx(hdc, 400, 600, NULL);
  SetViewportOrgEx(hdc, (dest_rect->right + dest_rect->left) / 2, (dest_rect->bottom + dest_rect->top) / 2, NULL);

  const Rarities* r = get_rarity(cp->id);
  PicHandleNames framenum = select_frame(cp, r, -1, -1);

  int is_a_creature = (hack_power || hack_toughness
					   || (int8_t)cp->card_type == CP_TYPE_CREATURE
					   || (((int8_t)cp->card_type == CP_TYPE_TOKEN
							|| (int8_t)cp->card_type == CP_TYPE_ARTIFACT)
						   && (cp->types[0] == SUBTYPE_CREATURE
							   || cp->types[1] == SUBTYPE_CREATURE
							   || cp->types[2] == SUBTYPE_CREATURE
							   || cp->types[3] == SUBTYPE_CREATURE
							   || cp->types[4] == SUBTYPE_CREATURE
							   || cp->types[5] == SUBTYPE_CREATURE
							   || cp->types[6] == SUBTYPE_CREATURE)));
  int is_a_planeswalker = is_planeswalker_by_cp(cp);

  GpImageAttributes* alpha_xform = NULL;
  int suppress_later_art = 0, big_art_exists = 0;

  char rules_text[2000];	// longest observed: Dance of the Dead at 658
  *rules_text = 0;

  SetBkMode(hdc, TRANSPARENT);
  SetTextAlign(hdc, TA_LEFT);

  int was_black_border = draw_fullcard_border_initial(hdc, BYTE0(big_art_style), r, cp->id);

  RECT rect_rulestext, rect_rulesbox;
  CopyRect(&rect_rulestext, &cfg->fullcard_rulestext);
  CopyRect(&rect_rulesbox, &cfg->fullcard_rulesbox);

  TranslucentFrameMode translucent_frame = draw_art_first(hdc, cp->id, pic_version, framenum, &suppress_later_art, &big_art_exists);

  int exclude_text_box_dc = 0;
  if (expand_text_box)
	exclude_text_box_dc = fullcard_exclude_expanded_text_box(hdc, cp, rules_text, &rect_rulestext, &rect_rulesbox,
															 is_a_planeswalker);

  if (translucent_frame == TFM_COLORLESS_ALSO_NORMAL_FULLCARD_ART)
	draw_fullcard_art(hdc, &cfg->fullcard_art, cp->id, pic_version);	// after clipping expanded text box

  draw_fullcard_frame(hdc, framenum, &alpha_xform, translucent_frame);

  if (cfg->fullcard_outline_visible && !was_black_border)
	draw_rect_outline(hdc, &cfg->fullcard_frame, cfg->fullcard_outline_color.colorref);

  COLORREF title_text_override_color = draw_fullcard_dfcsym_tombstone_title(hdc, cp, r, framenum, alpha_xform);

  int loyalty = draw_fullcard_type(hdc, cp, title_text_override_color, is_a_planeswalker, is_a_creature);

  if (cfg->fullcard_expansion_visible && r)
	blt_expansion_symbol(hdc, &cfg->fullcard_expansion, r->default_expansion, r->default_rarity, 0);

  if (has_manacost(cp))
	draw_manacost(hdc, &cfg->fullcard_mana, cp);

  if (!suppress_later_art)
	big_art_exists = draw_fullcard_art(hdc, &cfg->fullcard_art, cp->id, pic_version);

  if (cfg->fullcard_artoutline_visible)
	draw_rect_outline(hdc, &cfg->fullcard_art, cfg->fullcard_artoutline_color.colorref);

  if (exclude_text_box_dc)
	{
	  RestoreDC(hdc, exclude_text_box_dc);
	  fullcard_expand_text_box(hdc, &rect_rulesbox, framenum, alpha_xform, translucent_frame);
	}

  draw_fullcard_textbox_contents(hdc, cp, &rect_rulestext, rules_text, r, framenum, alpha_xform, is_a_planeswalker);

  if (is_a_creature)
	draw_fullcard_power_toughness(hdc, framenum, alpha_xform,
								  cfg->fullcard_extended_powertoughness ? (COLORREF)(-1) : title_text_override_color);

  if (is_a_planeswalker && loyalty != 0 && !is_a_creature)
	draw_fullcard_base_loyalty(hdc, loyalty, alpha_xform);

  if (cfg->fullcard_vanguard_visible && is_vanguard_by_cp(cp))
	draw_fullcard_vanguard(hdc, cp);

  if (counters > 0)
	{
	  RECT rc;
	  rc.left = 0;
	  rc.right = 800;
	  rc.top = 0;
	  rc.bottom = 1200;
	  draw_counters_for_fullcard(hdc, &rc, counter_type, counters);
	}

  draw_fullcard_border(hdc, BYTE0(big_art_style), r, cp->id);	// last - it sets clipping

  RestoreDC(hdc, savedc);
  LeaveCriticalSection(critical_section_for_drawing);
  return big_art_exists;
}
