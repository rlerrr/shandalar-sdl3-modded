#include "gdiplus.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "platform/gdi_compat.h"

struct GpImage {
  SDL_Surface *surface;
};

struct GpGraphics {
  HDC dc;
  GpImage *image;
  SDL_ScaleMode scale_mode;
};

struct GpImageAttributes {
  float alpha;
};

static GpStatus draw_image(GpGraphics *graphics, GpImage *image,
                           int dst_x, int dst_y, int dst_w, int dst_h,
                           int src_x, int src_y, int src_w, int src_h,
                           const GpImageAttributes *attributes)
{
  SDL_Rect src_rect;
  SDL_Rect dst_rect;
  Uint8 alpha;
  if (!graphics || !image || !image->surface)
    return InvalidParameter;
  src_rect.x = src_x;
  src_rect.y = src_y;
  src_rect.w = src_w;
  src_rect.h = src_h;
  dst_rect.x = dst_x;
  dst_rect.y = dst_y;
  dst_rect.w = dst_w;
  dst_rect.h = dst_h;
  alpha = attributes ? (Uint8)(attributes->alpha * 255.0f + 0.5f) : 255;
  if (graphics->image) {
    SDL_BlendMode old_blend;
    Uint8 old_alpha;
    SDL_GetSurfaceBlendMode(image->surface, &old_blend);
    SDL_GetSurfaceAlphaMod(image->surface, &old_alpha);
    SDL_SetSurfaceBlendMode(image->surface, SDL_BLENDMODE_BLEND);
    SDL_SetSurfaceAlphaMod(image->surface, alpha);
    SDL_BlitSurfaceScaled(image->surface, &src_rect,
                          graphics->image->surface, &dst_rect,
                          graphics->scale_mode);
    SDL_SetSurfaceAlphaMod(image->surface, old_alpha);
    SDL_SetSurfaceBlendMode(image->surface, old_blend);
    return Ok;
  }
  if (graphics->dc && shandalar_dc_blit_bgra32(
          graphics->dc, dst_x, dst_y, dst_w, dst_h,
          image->surface->pixels, image->surface->pitch,
          image->surface->w, image->surface->h,
          src_x, src_y, src_w, src_h, alpha))
    return Ok;
  return GenericError;
}

VOID WINAPI GdiplusShutdown(ULONG_PTR token) { (void)token; }

Status WINAPI GdiplusStartup(ULONG_PTR *token, const GdiplusStartupInput *input,
                             GdiplusStartupOutput *output)
{
  (void)input;
  if (token) *token = 1;
  if (output) memset(output, 0, sizeof(*output));
  return Ok;
}

GpStatus WINGDIPAPI GdipBitmapLockBits(GpBitmap *bitmap, const GpRect *rect,
                                       UINT flags, PixelFormat format,
                                       BitmapData *data)
{
  (void)flags;
  (void)format;
  if (!bitmap || !bitmap->surface || !rect || !data)
    return InvalidParameter;
  data->Width = rect->Width;
  data->Height = rect->Height;
  data->Stride = bitmap->surface->pitch;
  data->PixelFormat = PixelFormat32bppARGB;
  data->Scan0 = (Uint8 *)bitmap->surface->pixels +
                rect->Y * bitmap->surface->pitch + rect->X * 4;
  data->Reserved = 0;
  return Ok;
}

GpStatus WINGDIPAPI GdipBitmapUnlockBits(GpBitmap *bitmap, BitmapData *data)
{
  (void)bitmap;
  (void)data;
  return Ok;
}

GpStatus WINGDIPAPI GdipCloneBitmapAreaI(INT x, INT y, INT width, INT height,
                                         PixelFormat format, GpBitmap *source,
                                         GpBitmap **destination)
{
  SDL_Rect src = {x, y, width, height};
  SDL_Rect dst = {0, 0, width, height};
  GpImage *result;
  (void)format;
  if (!source || !source->surface || !destination)
    return InvalidParameter;
  result = (GpImage *)calloc(1, sizeof(*result));
  if (!result) return OutOfMemory;
  result->surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_BGRA32);
  if (!result->surface) { free(result); return OutOfMemory; }
  SDL_SetSurfaceBlendMode(source->surface, SDL_BLENDMODE_NONE);
  SDL_BlitSurface(source->surface, &src, result->surface, &dst);
  *destination = result;
  return Ok;
}

GpStatus WINGDIPAPI GdipCreateBitmapFromScan0(INT width, INT height, INT stride,
                                               PixelFormat format, BYTE *scan0,
                                               GpBitmap **bitmap)
{
  GpImage *result;
  (void)format;
  if (!bitmap || width <= 0 || height <= 0) return InvalidParameter;
  result = (GpImage *)calloc(1, sizeof(*result));
  if (!result) return OutOfMemory;
  result->surface = scan0
      ? SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_BGRA32, scan0, stride)
      : SDL_CreateSurface(width, height, SDL_PIXELFORMAT_BGRA32);
  if (!result->surface) { free(result); return OutOfMemory; }
  *bitmap = result;
  return Ok;
}

GpStatus WINGDIPAPI GdipCreateFromHDC(HDC dc, GpGraphics **graphics)
{
  GpGraphics *result;
  if (!dc || !graphics) return InvalidParameter;
  result = (GpGraphics *)calloc(1, sizeof(*result));
  if (!result) return OutOfMemory;
  result->dc = dc;
  result->scale_mode = SDL_SCALEMODE_LINEAR;
  *graphics = result;
  return Ok;
}

GpStatus WINGDIPAPI GdipCreateHBITMAPFromBitmap(GpBitmap *bitmap,
                                                HBITMAP *result, ARGB background)
{
  (void)bitmap; (void)result; (void)background;
  return NotImplemented;
}

GpStatus WINGDIPAPI GdipCreateImageAttributes(GpImageAttributes **attributes)
{
  if (!attributes) return InvalidParameter;
  *attributes = (GpImageAttributes *)calloc(1, sizeof(**attributes));
  if (!*attributes) return OutOfMemory;
  (*attributes)->alpha = 1.0f;
  return Ok;
}

GpStatus WINGDIPAPI GdipDeleteGraphics(GpGraphics *graphics)
{
  free(graphics);
  return Ok;
}

GpStatus WINGDIPAPI GdipDisposeImage(GpImage *image)
{
  if (image) {
    SDL_DestroySurface(image->surface);
    free(image);
  }
  return Ok;
}

GpStatus WINGDIPAPI GdipDisposeImageAttributes(GpImageAttributes *attributes)
{
  free(attributes);
  return Ok;
}

GpStatus WINGDIPAPI GdipDrawImageI(GpGraphics *graphics, GpImage *image,
                                   INT x, INT y)
{
  if (!image || !image->surface) return InvalidParameter;
  return draw_image(graphics, image, x, y, image->surface->w, image->surface->h,
                    0, 0, image->surface->w, image->surface->h, NULL);
}

GpStatus WINGDIPAPI GdipDrawImageRectI(GpGraphics *graphics, GpImage *image,
                                       INT x, INT y, INT width, INT height)
{
  if (!image || !image->surface) return InvalidParameter;
  return draw_image(graphics, image, x, y, width, height,
                    0, 0, image->surface->w, image->surface->h, NULL);
}

GpStatus WINGDIPAPI GdipDrawImageRectRectI(
    GpGraphics *graphics, GpImage *image,
    INT dstx, INT dsty, INT dstwidth, INT dstheight,
    INT srcx, INT srcy, INT srcwidth, INT srcheight,
    GpUnit srcUnit, const GpImageAttributes *attributes,
    DrawImageAbort callback, VOID *callbackData)
{
  (void)srcUnit; (void)callback; (void)callbackData;
  return draw_image(graphics, image, dstx, dsty, dstwidth, dstheight,
                    srcx, srcy, srcwidth, srcheight, attributes);
}

GpStatus WINGDIPAPI GdipGetImageGraphicsContext(GpImage *image,
                                                GpGraphics **graphics)
{
  GpGraphics *result;
  if (!image || !graphics) return InvalidParameter;
  result = (GpGraphics *)calloc(1, sizeof(*result));
  if (!result) return OutOfMemory;
  result->image = image;
  result->scale_mode = SDL_SCALEMODE_LINEAR;
  *graphics = result;
  return Ok;
}

GpStatus WINGDIPAPI GdipGetImageHeight(GpImage *image, UINT *height)
{
  if (!image || !image->surface || !height) return InvalidParameter;
  *height = image->surface->h;
  return Ok;
}

GpStatus WINGDIPAPI GdipGetImageWidth(GpImage *image, UINT *width)
{
  if (!image || !image->surface || !width) return InvalidParameter;
  *width = image->surface->w;
  return Ok;
}

GpStatus WINGDIPAPI GdipImageRotateFlip(GpImage *image, RotateFlipType type)
{
  (void)image; (void)type;
  return NotImplemented;
}

GpStatus WINGDIPAPI GdipSetImageAttributesColorMatrix(
    GpImageAttributes *attributes, ColorAdjustType type, BOOL enabled,
    const ColorMatrix *matrix, const ColorMatrix *grayMatrix,
    ColorMatrixFlags flags)
{
  (void)type; (void)grayMatrix; (void)flags;
  if (!attributes) return InvalidParameter;
  attributes->alpha = enabled && matrix ? matrix->m[3][3] : 1.0f;
  return Ok;
}

GpStatus WINGDIPAPI GdipSetInterpolationMode(GpGraphics *graphics,
                                             InterpolationMode mode)
{
  if (!graphics) return InvalidParameter;
  graphics->scale_mode = mode == InterpolationModeNearestNeighbor
                           ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR;
  return Ok;
}
