#include "platform/file_io.h"

#include "platform/legacy_io.h"
#include "platform/mod_overlay.h"
#include "platform/asset_manager.h"
#include <SDL3/SDL_filesystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef struct ShandalarFileIO {
  FILE *file;
  int writable;
} ShandalarFileIO;

#ifdef __EMSCRIPTEN__
static void seed_user_config(const char *path)
{
  char asset_path[1024], resolved[1024];
  const char *name = strrchr(path, '/');
  SDL_IOStream *input, *output;
  unsigned char bytes[4096];
  size_t count;
  int found, ok = 1;
  if (!name || strcmp(name + 1, "manalink.ini") != 0 || SDL_GetPathInfo(path, NULL))
    return;
  /* Copy effective asset/ZIP defaults once. Subsequent opens use the saved
   * config, even when the mod ships a file with the same name. */
  if (!shandalar_assets_join_path(asset_path, sizeof(asset_path), "Manalink.ini"))
    return;
  input = shandalar_mod_open(asset_path, &found);
  if (!found) {
    if (!shandalar_resolve_existing_path(asset_path, resolved, sizeof(resolved))) {
      if (shandalar_assets_fetch_blocking("Manalink.ini") != SHANDALAR_ASSET_READY)
        return;
      shandalar_resolve_existing_path(asset_path, resolved, sizeof(resolved));
    }
    input = SDL_IOFromFile(resolved, "rb");
  }
  if (!input)
    return;
  output = SDL_IOFromFile(path, "wb");
  if (!output) {
    SDL_CloseIO(input);
    return;
  }
  while ((count = SDL_ReadIO(input, bytes, sizeof(bytes))) != 0) {
    if (SDL_WriteIO(output, bytes, count) != count) {
      ok = 0;
      break;
    }
  }
  ok = ok && SDL_GetIOStatus(input) != SDL_IO_STATUS_ERROR;
  SDL_CloseIO(input);
  if (!SDL_CloseIO(output))
    ok = 0;
  if (ok)
    shandalar_user_data_sync();
  else
    SDL_RemovePath(path);
}
#endif

static Sint64 SDLCALL file_io_seek(void *userdata, Sint64 offset,
                                   SDL_IOWhence whence)
{
  ShandalarFileIO *file_io = (ShandalarFileIO *)userdata;
  int origin;
  /* SDL_TellIO uses a zero-offset relative seek. Calling fseek here would
   * discard stdio's read-ahead buffer on every text/scanner position query. */
  if (whence == SDL_IO_SEEK_CUR && offset == 0)
    return (Sint64)ftell(file_io->file);
  switch (whence) {
  case SDL_IO_SEEK_SET: origin = SEEK_SET; break;
  case SDL_IO_SEEK_CUR: origin = SEEK_CUR; break;
  case SDL_IO_SEEK_END: origin = SEEK_END; break;
  default: return -1;
  }
  if (fseek(file_io->file, (long)offset, origin) != 0)
    return -1;
  return (Sint64)ftell(file_io->file);
}

static size_t SDLCALL file_io_read(void *userdata, void *ptr, size_t size,
                                   SDL_IOStatus *status)
{
  ShandalarFileIO *file_io = (ShandalarFileIO *)userdata;
  size_t bytes = fread(ptr, 1, size, file_io->file);
  if (bytes < size)
    *status = ferror(file_io->file) ? SDL_IO_STATUS_ERROR : SDL_IO_STATUS_EOF;
  return bytes;
}

static size_t SDLCALL file_io_write(void *userdata, const void *ptr,
                                    size_t size, SDL_IOStatus *status)
{
  ShandalarFileIO *file_io = (ShandalarFileIO *)userdata;
  size_t bytes;
  if (!file_io->writable) {
    *status = SDL_IO_STATUS_READONLY;
    return 0;
  }
  bytes = fwrite(ptr, 1, size, file_io->file);
  if (bytes < size)
    *status = SDL_IO_STATUS_ERROR;
  return bytes;
}

static bool SDLCALL file_io_flush(void *userdata, SDL_IOStatus *status)
{
  ShandalarFileIO *file_io = (ShandalarFileIO *)userdata;
  if (!file_io->writable || fflush(file_io->file) == 0)
    return true;
  *status = SDL_IO_STATUS_ERROR;
  return false;
}

static bool SDLCALL file_io_close(void *userdata)
{
  ShandalarFileIO *file_io = (ShandalarFileIO *)userdata;
  int result = fclose(file_io->file);
  free(file_io);
  return result == 0;
}

SDL_IOStream *shandalar_open_file_io(const char *path, const char *mode)
{
  ShandalarFileIO *file_io;
  SDL_IOStream *io;
  SDL_IOStreamInterface interface;
  char binary_mode[8];
  size_t mode_length = 0;
#ifdef __EMSCRIPTEN__
  char user_path[1024];
#endif

  if (!path || !path[0] || !mode || !mode[0])
    return NULL;

#ifdef __EMSCRIPTEN__
  if (shandalar_user_data_path(path, user_path, sizeof(user_path))) {
    if (mode[0] == 'r' || mode[0] == 'a')
      seed_user_config(user_path);
    path = user_path;
  }
#endif

  if (mode[0] == 'r' && !strchr(mode, '+')) {
    int found;
    io = shandalar_mod_open(path, &found);
    if (found)
      return io;
  }

  file_io = (ShandalarFileIO *)malloc(sizeof(*file_io));
  if (!file_io)
    return NULL;
  /* Text normalization belongs to the line/token helpers, so byte offsets
   * and binary decoders behave identically for disk and ZIP streams. */
  for (const char *cursor = mode; *cursor; ++cursor) {
    if (*cursor != 't' && *cursor != 'b' && mode_length < sizeof(binary_mode) - 2)
      binary_mode[mode_length++] = *cursor;
  }
  binary_mode[mode_length++] = 'b';
  binary_mode[mode_length] = 0;
  file_io->file = fopen(path, binary_mode);
  if (!file_io->file) {
    SDL_SetError("Could not open %s: %s", path, strerror(errno));
    free(file_io);
    return NULL;
  }
  file_io->writable =
      strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL ||
      strchr(mode, '+') != NULL;

  SDL_INIT_INTERFACE(&interface);
  interface.seek = file_io_seek;
  interface.read = file_io_read;
  interface.write = file_io_write;
  interface.flush = file_io_flush;
  interface.close = file_io_close;
  io = SDL_OpenIO(&interface, file_io);
  if (!io) {
    fclose(file_io->file);
    free(file_io);
  }
  return io;
}
