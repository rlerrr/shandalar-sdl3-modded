#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "platform/legacy_io.h"
#include "platform/file_io.h"
#include <string.h>

#include "platform/gdi_compat.h"

HBITMAP load_legacy_pic(char *filename);

static HBITMAP convert_legacy_bitmap(HBITMAP source, HPALETTE palette)
{
  BITMAP source_info;
  BITMAPINFO destination_info;
  HBITMAP destination;
  HDC source_dc;
  HDC destination_dc;
  HGDIOBJ old_source;
  HGDIOBJ old_destination;
  void *pixels;

  if (!source || !GetObject(source, sizeof(source_info), &source_info))
    return source;
  if (source_info.bmBitsPixel == 32) {
    int x;
    int y;
    unsigned char *row = (unsigned char *)source_info.bmBits;
    /* The proprietary format has no alpha channel.  Its decoder leaves the
       fourth byte clear, which GDI ignored but GDI+ interprets as transparent. */
    for (y = 0; y < source_info.bmHeight; ++y) {
      for (x = 0; x < source_info.bmWidth; ++x)
        row[x * 4 + 3] = 0xff;
      row += source_info.bmWidthBytes;
    }
    return source;
  }

  memset(&destination_info, 0, sizeof(destination_info));
  destination_info.bmiHeader.biSize = sizeof(destination_info.bmiHeader);
  destination_info.bmiHeader.biWidth = source_info.bmWidth;
  destination_info.bmiHeader.biHeight = -source_info.bmHeight;
  destination_info.bmiHeader.biPlanes = 1;
  destination_info.bmiHeader.biBitCount = 32;
  destination_info.bmiHeader.biCompression = BI_RGB;
  destination = CreateDIBSection(NULL, &destination_info, DIB_RGB_COLORS,
                                 &pixels, NULL, 0);
  source_dc = CreateCompatibleDC(NULL);
  destination_dc = CreateCompatibleDC(NULL);
  if (!destination || !source_dc || !destination_dc) {
    if (destination)
      DeleteObject(destination);
    if (source_dc)
      DeleteDC(source_dc);
    if (destination_dc)
      DeleteDC(destination_dc);
    return source;
  }

  old_source = SelectObject(source_dc, source);
  if (palette) {
    SelectPalette(source_dc, palette, FALSE);
    RealizePalette(source_dc);
  }
  old_destination = SelectObject(destination_dc, destination);
  BitBlt(destination_dc, 0, 0, source_info.bmWidth, source_info.bmHeight,
         source_dc, 0, 0, SRCCOPY);
  SelectObject(source_dc, old_source);
  SelectObject(destination_dc, old_destination);
  DeleteDC(source_dc);
  DeleteDC(destination_dc);
  DeleteObject(source);
  return destination;
}

HANDLE LoadImage(const char *path, HPALETTE palette, int unused_width,
                 int unused_height)
{
  SDL_Surface *loaded;
  SDL_Surface *surface;
  BITMAPINFO info;
  HBITMAP bitmap;
  unsigned char *pixels;
  int y;
  (void)unused_width;
  (void)unused_height;

  /* The modded asset sets use .pic for both the original proprietary format
     and ordinary images such as PNGs.  SDL_image sniffs the file contents, so
     try it before selecting the legacy decoder by extension. */
  SDL_IOStream *io = shandalar_open_file_io(path, "rb");
  if (!io)
    return NULL;
  loaded = IMG_Load_IO(io, true);
  if (loaded)
    goto convert_surface;

  /* SDL_image's configured formats do not include BMP in every build.  SDL
     itself supplies that decoder, including for BMPs named .pic. */
  io = shandalar_open_file_io(path, "rb");
  if (io) {
    loaded = SDL_LoadBMP_IO(io, true);
    if (loaded)
      goto convert_surface;
  }

  {
    const char *extension = strrchr(path, '.');
    if (extension && (SDL_strcasecmp(extension, ".pic") == 0 ||
                      SDL_strcasecmp(extension, ".pcx") == 0))
      return convert_legacy_bitmap(load_legacy_pic((char *)path), palette);
  }
  return NULL;

convert_surface:
  surface = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_BGRA32);
  SDL_DestroySurface(loaded);
  if (!surface)
    return NULL;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = surface->w;
  info.bmiHeader.biHeight = -surface->h;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS,
                            (void **)&pixels, NULL, 0);
  if (bitmap) {
    for (y = 0; y < surface->h; ++y)
      memcpy(pixels + y * surface->w * 4,
             (const unsigned char *)surface->pixels + y * surface->pitch,
             (size_t)surface->w * 4);
  }
  SDL_DestroySurface(surface);
  return bitmap;
}
