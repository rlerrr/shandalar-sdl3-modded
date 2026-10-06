#include "platform/asset_manager.h"
#include "platform/gdi_compat.h"
#include "platform/legacy_io.h"
#include "platform/mod_overlay.h"
#include "platform/profile_cache.h"
#include <SDL3/SDL.h>
#include <miniz.h>
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "Line %d: %s (%s)\n", __LINE__, #expr, SDL_GetError()); return 1; \
} } while (0)

static int reads_as(const char *file, const char *section, const char *key,
                    const char *fallback, const char *expected)
{
  char value[128];
  DWORD length = GetPrivateProfileStringA(section, key, fallback, value, sizeof(value), file);
  return length == strlen(expected) && strcmp(value, expected) == 0;
}

int main(void)
{
  const char fixture[] =
      "Outside=ignored\r\n[Settings]\r\n;comment\r\n#comment\r\n"
      "Mixed = first\r\nmixed=second\r\nEmpty=\r\n"
      "Quoted=\"C:\\cards\\frame.png\"\r\nLiteral=hello ; # world\r\n"
      "Number=-12 trailing\r\nInvalid=word\r\nHex=0x10\r\n"
      "malformed line\r\n[settings]\r\nMixed=third\r\nOther=last\r\n"
      "[a:b]\r\nc=one\r\n[a]\r\nb:c=two";
  const char mod_profile[] = "[Settings]\nMixed=mod\n";
  const char mod_settings[] = "[DeckBuilder]\nSort=4\nKeep=mod default\n";
  mz_zip_archive zip;
  void *zip_bytes;
  size_t zip_size;
  char value[4], path[256];
#ifdef __EMSCRIPTEN__
  /* Exercise the real browser namespace routing with a local Node MEMFS. */
  EM_ASM({
    FS.mkdirTree('/userdata');
    Module.__shandalarUserDataReady = 1;
  });
#endif
  shandalar_assets_set_root(".");
  CHECK(SDL_SaveFile("profile-test.ini", fixture, sizeof(fixture) - 1));
  CHECK(reads_as("profile-test.ini", "SETTINGS", "mixed", "fallback", "first"));
  CHECK(reads_as("profile-test.ini", "Settings", "Quoted", "", "C:\\cards\\frame.png"));
  CHECK(reads_as("profile-test.ini", "Settings", "Literal", "", "hello ; # world"));
  CHECK(reads_as("profile-test.ini", "Settings", "Empty", "fallback", ""));
  CHECK(reads_as("profile-test.ini", "Settings", "Other", "", "last"));
  CHECK(reads_as("profile-test.ini", "a:b", "c", "", "one"));
  CHECK(reads_as("profile-test.ini", "a", "b:c", "", "two"));
  CHECK(reads_as("profile-test.ini", "", "Outside", "fallback", "fallback"));
  CHECK(reads_as("profile-test.ini", "Settings", "Absent", "one", "one"));
  CHECK(reads_as("profile-test.ini", "Settings", "Absent", "two", "two"));
  CHECK(GetPrivateProfileIntA("Settings", "Number", 99, "profile-test.ini") == -12);
  CHECK(GetPrivateProfileIntA("Settings", "Invalid", 99, "profile-test.ini") == 99);
  CHECK(GetPrivateProfileIntA("Settings", "Hex", 99, "profile-test.ini") == 0);
  CHECK(GetPrivateProfileStringA("Settings", "Mixed", "", value, sizeof(value), "profile-test.ini") == 3);
  CHECK(!strcmp(value, "fir"));
  CHECK(GetPrivateProfileStringA("Settings", "Mixed", "", value, 1, "profile-test.ini") == 0 && !value[0]);
  CHECK(GetPrivateProfileStringA("Settings", "Mixed", "", NULL, 0, "profile-test.ini") == 0);
  /* Removal proves subsequent key lookups use the parsed snapshot. */
  CHECK(SDL_RemovePath("profile-test.ini"));
  CHECK(reads_as("PROFILE-TEST.INI", "Settings", "Other", "", "last"));
  CHECK(SDL_SaveFile("profile-test.ini", mod_profile, sizeof(mod_profile) - 1));
  shandalar_assets_set_root(".");
  CHECK(reads_as("profile-test.ini", "Settings", "Mixed", "", "mod"));
  CHECK(WritePrivateProfileStringA("Settings", "Mixed", "written", "profile-test.ini"));
  CHECK(reads_as("profile-test.ini", "Settings", "Mixed", "", "written"));
  CHECK(WritePrivateProfileStringA("Settings", "Mixed", NULL, "profile-test.ini"));
  CHECK(reads_as("profile-test.ini", "Settings", "Mixed", "removed", "removed"));
#ifndef __EMSCRIPTEN__
  CHECK(reads_as("profile-missing.ini", "Settings", "Mixed", "missing", "missing"));
#endif
  CHECK(SDL_SaveFile("profile-missing.ini", mod_profile, sizeof(mod_profile) - 1));
  CHECK(reads_as("profile-missing.ini", "Settings", "Mixed", "", "mod"));

  memset(&zip, 0, sizeof(zip));
  CHECK(mz_zip_writer_init_heap(&zip, 0, 0));
  CHECK(mz_zip_writer_add_mem(&zip, "profile-test.ini", mod_profile, sizeof(mod_profile) - 1, MZ_BEST_COMPRESSION));
  CHECK(mz_zip_writer_add_mem(&zip, "Manalink.ini", mod_settings, sizeof(mod_settings) - 1, MZ_BEST_COMPRESSION));
  CHECK(mz_zip_writer_finalize_heap_archive(&zip, &zip_bytes, &zip_size));
  mz_zip_writer_end(&zip);
  CHECK(SDL_SaveFile("profile-test.zip", zip_bytes, zip_size));
  mz_free(zip_bytes);
  CHECK(shandalar_mod_load("profile-test.zip"));
  CHECK(reads_as("profile-test.ini", "Settings", "Mixed", "", "mod"));
#ifdef __EMSCRIPTEN__
  CHECK(GetPrivateProfileIntA("DeckBuilder", "Sort", 0, ".\\MANALINK.INI") == 4);
  SDL_snprintf(path, sizeof(path), "%s/manalink.ini", shandalar_user_data_root());
  CHECK(SDL_GetPathInfo(path, NULL));
  CHECK(WritePrivateProfileStringA("DeckBuilder", "Sort", "9", ".\\MANALINK.INI"));
  CHECK(GetPrivateProfileIntA("DeckBuilder", "Sort", 0, "Manalink.ini") == 9);
  CHECK(reads_as("Manalink.ini", "DeckBuilder", "Keep", "", "mod default"));
  CHECK(!SDL_GetPathInfo("Manalink.ini", NULL));
  /* Reopening after clearing memory retains the saved preference over ZIP defaults. */
  shandalar_profile_cache_clear();
  CHECK(GetPrivateProfileIntA("DeckBuilder", "Sort", 0, "Manalink.ini") == 9);
  CHECK(!SDL_GetPathInfo("/userdata/manalink.ini.tmp", NULL));
#else
  (void)path;
#endif
  shandalar_mod_unload();
  CHECK(reads_as("profile-test.ini", "Settings", "Mixed", "removed", "removed"));
#ifdef __EMSCRIPTEN__
  CHECK(GetPrivateProfileIntA("DeckBuilder", "Sort", 0, "Manalink.ini") == 9);
  CHECK(SDL_RemovePath(path));
#endif
  shandalar_profile_cache_clear();
  CHECK(SDL_RemovePath("profile-test.ini"));
  CHECK(SDL_RemovePath("profile-missing.ini"));
  CHECK(SDL_RemovePath("profile-test.zip"));
  puts("Profile snapshots, legacy parsing, writes, ZIP changes and user settings passed.");
  return 0;
}
