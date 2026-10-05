#include "platform/asset_manager.h"
#include "platform/file_io.h"
#include "platform/mod_overlay.h"

#include <SDL3/SDL.h>
#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "Line %d: %s (%s)\n", __LINE__, #expr, SDL_GetError()); \
  return 1; } } while (0)

static int make_zip(const char *const *names, const char *const *contents,
                    size_t count, int level, int corrupt)
{
  mz_zip_archive zip;
  void *bytes = NULL;
  size_t size = 0, i;
  int ok;
  memset(&zip, 0, sizeof(zip));
  if (!mz_zip_writer_init_heap(&zip, 0, 0))
    return 0;
  for (i = 0; i < count; ++i) {
    /* miniz's writer rejects absolute names. Build a safe name of the same
     * length, then patch header names below to exercise our reader policy. */
    char *safe_name = SDL_strdup(names[i]);
    int added;
    char *colon;
    if (!safe_name) {
      mz_zip_writer_end(&zip);
      return 0;
    }
    if (safe_name[0] == '/')
      safe_name[0] = '_';
    colon = strchr(safe_name, ':');
    if (colon)
      *colon = '_';
    added = mz_zip_writer_add_mem(&zip, safe_name, contents[i], strlen(contents[i]), level);
    SDL_free(safe_name);
    if (!added) {
      mz_zip_writer_end(&zip);
      return 0;
    }
  }
  ok = mz_zip_writer_finalize_heap_archive(&zip, &bytes, &size);
  mz_zip_writer_end(&zip);
  if (!ok)
    return 0;
  if (count == 1 && (names[0][0] == '/' || strchr(names[0], ':'))) {
    unsigned char *raw = (unsigned char *)bytes;
    for (i = 0; i + 46 + strlen(names[0]) <= size; ++i) {
      if (memcmp(raw + i, "PK\003\004", 4) == 0)
        memcpy(raw + i + 30, names[0], strlen(names[0]));
      else if (memcmp(raw + i, "PK\001\002", 4) == 0)
        memcpy(raw + i + 46, names[0], strlen(names[0]));
    }
  }
  if (corrupt) {
    /* Flip a stored data byte while leaving the recorded CRC intact. */
    ((unsigned char *)bytes)[30 + strlen(names[0])] ^= 1;
  }
  ok = SDL_SaveFile("mod-overlay-test.zip", bytes, size);
  mz_free(bytes);
  return ok;
}

static int reads_as(const char *path, const char *expected)
{
  char buffer[128] = {0};
  SDL_IOStream *io = shandalar_open_file_io(path, "rb");
  size_t size;
  if (!io)
    return 0;
  size = SDL_ReadIO(io, buffer, sizeof(buffer) - 1);
  SDL_CloseIO(io);
  return size == strlen(expected) && strcmp(buffer, expected) == 0;
}

static void set_mod_env(const char *path)
{
#ifdef _WIN32
  _putenv_s("SHANDALAR_MOD_ZIP", path);
#else
  setenv("SHANDALAR_MOD_ZIP", path, 1);
#endif
}

int main(int argc, char **argv)
{
  const char *names[] = {"overlay-test-base.bin", "Sound/Test.wav", "empty.bin", "directory/"};
  const char *contents[] = {"mod bytes", "compressed sound compressed sound compressed sound", "", ""};
  SDL_IOStream *first, *second, *empty;
  char bytes[128] = {0};
  char *cwd;
  char absolute[1024];
  int found;

  if (argc > 1) {
    /* Used by the host integration check with Module.modZip or ENV. */
    CHECK(shandalar_mod_init_from_args(&argc, argv));
    CHECK(reads_as("overlay-test-base.bin", "mod bytes"));
    shandalar_mod_unload();
    puts("WebAssembly host mod ZIP startup passed.");
    return 0;
  }

  set_mod_env("");
  shandalar_assets_set_root(".");
  CHECK(SDL_SaveFile("overlay-test-base.bin", "base bytes", 10));
  CHECK(SDL_SaveFile("overlay-test-fallback.bin", "fallback", 8));
  CHECK(make_zip(names, contents, 4, MZ_BEST_COMPRESSION, 0));
  CHECK(shandalar_mod_load("mod-overlay-test.zip"));
  CHECK(!shandalar_mod_load("mod-overlay-test.zip"));
  CHECK(reads_as(".\\OVERLAY-TEST-BASE.BIN", "mod bytes"));
  CHECK(reads_as("Sound//./TEST.wav", contents[1]));
  CHECK(reads_as("overlay-test-fallback.bin", "fallback"));
  empty = shandalar_open_file_io("empty.bin", "rb");
  CHECK(empty && SDL_GetIOSize(empty) == 0);
  CHECK(SDL_ReadIO(empty, bytes, 1) == 0);
  CHECK(SDL_CloseIO(empty));

  cwd = SDL_GetCurrentDirectory();
  CHECK(cwd);
  shandalar_assets_set_root(cwd);
  SDL_snprintf(absolute, sizeof(absolute), "%sOVERLAY-TEST-BASE.BIN", cwd);
  CHECK(reads_as(absolute, "mod bytes"));
  SDL_free(cwd);
  shandalar_assets_set_root("/assets_modded");
  CHECK(reads_as("\\ASSETS_MODDED\\sound\\test.wav", contents[1]));
  CHECK(!shandalar_mod_open("/unrelated/Sound/Test.wav", &found) && !found);
  CHECK(!shandalar_mod_open("/assets_modded_extra/Sound/Test.wav", &found) && !found);
  CHECK(!shandalar_mod_open("../Sound/Test.wav", &found) && !found);
  shandalar_assets_set_root(".");

  first = shandalar_open_file_io(names[0], "rb");
  second = shandalar_open_file_io(names[0], "rb");
  CHECK(first && second);
  CHECK(SDL_GetIOSize(first) == 9);
  CHECK(SDL_ReadIO(first, bytes, 3) == 3 && memcmp(bytes, "mod", 3) == 0);
  CHECK(SDL_TellIO(second) == 0);
  CHECK(SDL_SeekIO(first, -5, SDL_IO_SEEK_END) == 4);
  CHECK(SDL_ReadIO(first, bytes, 5) == 5 && memcmp(bytes, "bytes", 5) == 0);
  CHECK(SDL_WriteIO(first, "x", 1) == 0);
  CHECK(SDL_GetIOStatus(first) == SDL_IO_STATUS_READONLY);
  /* Writes and update modes touch the base file, never the ZIP. */
  {
    SDL_IOStream *write = shandalar_open_file_io(names[0], "wb");
    CHECK(write && SDL_WriteIO(write, "new base", 8) == 8 && SDL_CloseIO(write));
    write = shandalar_open_file_io(names[0], "r+b");
    CHECK(write && SDL_WriteIO(write, "N", 1) == 1 && SDL_CloseIO(write));
  }
  CHECK(reads_as(names[0], "mod bytes"));
  shandalar_mod_unload();
  CHECK(reads_as(names[0], "New base"));
  CHECK(SDL_ReadIO(second, bytes, 9) == 9 && memcmp(bytes, "mod bytes", 9) == 0);
  CHECK(SDL_CloseIO(first) && SDL_CloseIO(second));

  /* Stored ZIPs, malformed archives, CRC failures, and ambiguous paths. */
  CHECK(make_zip(names, contents, 1, 0, 0));
  CHECK(shandalar_mod_load("mod-overlay-test.zip"));
  CHECK(reads_as(names[0], "mod bytes"));
  shandalar_mod_unload();
  CHECK(make_zip(names, contents, 1, 0, 1));
  CHECK(shandalar_mod_load("mod-overlay-test.zip"));
  CHECK(!shandalar_open_file_io(names[0], "rb"));
  shandalar_mod_unload();
  CHECK(SDL_SaveFile("mod-overlay-test.zip", "not a zip", 9));
  CHECK(!shandalar_mod_load("mod-overlay-test.zip"));
  {
    const char *bad_names[] = {"../outside.bin", "/absolute.bin", "C:\\drive.bin", "Sound/../bad.bin"};
    size_t i;
    for (i = 0; i < SDL_arraysize(bad_names); ++i) {
      CHECK(make_zip(&bad_names[i], contents, 1, 0, 0));
      CHECK(!shandalar_mod_load("mod-overlay-test.zip"));
    }
  }
  {
    const char *duplicates[] = {"Sound/Test.wav", "sound\\test.WAV"};
    CHECK(make_zip(duplicates, contents, 2, 0, 0));
    CHECK(!shandalar_mod_load("mod-overlay-test.zip"));
  }

  /* CLI wins over environment, preserves other flags, and rejects repeats. */
  CHECK(make_zip(names, contents, 4, MZ_BEST_COMPRESSION, 0));
  set_mod_env("missing.zip");
  {
    char *args[] = {"test", "assets", "--mod=mod-overlay-test.zip", "--smoke", NULL};
    int argc = 4;
    CHECK(shandalar_mod_init_from_args(&argc, args));
    CHECK(argc == 3 && strcmp(args[1], "assets") == 0 && strcmp(args[2], "--smoke") == 0 && !args[3]);
    CHECK(reads_as(names[0], "mod bytes"));
    shandalar_mod_unload();
  }
  {
    char *args[] = {"test", "--mod", "mod-overlay-test.zip", "assets", NULL};
    int argc = 4;
    CHECK(shandalar_mod_init_from_args(&argc, args));
    CHECK(argc == 2 && strcmp(args[1], "assets") == 0 && !args[2]);
    shandalar_mod_unload();
  }
  {
    char *args[] = {"test", "--mod=a.zip", "--mod=b.zip", NULL};
    int argc = 3;
    CHECK(!shandalar_mod_init_from_args(&argc, args) && argc == 3);
  }
  {
    char *args[] = {"test", "--mod", NULL};
    int argc = 2;
    CHECK(!shandalar_mod_init_from_args(&argc, args));
    args[1] = "--mod=";
    CHECK(!shandalar_mod_init_from_args(&argc, args));
  }
  set_mod_env("mod-overlay-test.zip");
  {
    char *args[] = {"test", NULL};
    int argc = 1;
    CHECK(shandalar_mod_init_from_args(&argc, args));
    CHECK(reads_as(names[0], "mod bytes"));
    shandalar_mod_unload();
  }
  set_mod_env("");
  remove("mod-overlay-test.zip");
  remove("overlay-test-base.bin");
  remove("overlay-test-fallback.bin");
  puts("ZIP overlay, fallback, stream lifetime, and startup options passed.");
  return 0;
}
