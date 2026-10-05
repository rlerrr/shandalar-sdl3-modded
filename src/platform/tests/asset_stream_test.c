#include "platform/asset_manager.h"
#include "platform/file_io.h"
#include "platform/mod_overlay.h"
#include "platform/iniparser.h"
#include "platform/winuser_compat.h"
#include "drawcardlib/src/pic.h"
#include "shandalar/src/card_pool.h"
#include "magic/src/global_strings.h"
#include <SDL3/SDL.h>
#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "Line %d: %s (%s)\n", __LINE__, #expr, SDL_GetError()); return 1; \
} } while (0)

char global_base_directory[0x105] = "stream-test-assets";
int g_duel_ai_mode_state;
char ability_tooltip_names_exe[17][50];
void manalinkeh_initialize_ability_tooltip_names(void) {}
void backtrace_and_abort(const char *message) { fprintf(stderr, "%s\n", message); abort(); }
HBITMAP load_legacy_pic(char *filename);

static int entries;
static void collect(void *context, const char *section, const char *key, const char *value)
{
  (void)context; (void)section; (void)key; (void)value;
  ++entries;
}

static int add(mz_zip_archive *zip, const char *name, const void *data, size_t size)
{
  return mz_zip_writer_add_mem(zip, name, data, size, MZ_BEST_COMPRESSION);
}

static int inspect_pictures(const char *archive)
{
  const char *patterns[] = {"*.pic", "Faces/*.pic", "Duelart/*.pic", "Statwin/*.pic",
                            "Main Folder Pics/*.pic"};
  size_t count = 0, i, j;
  shandalar_assets_set_root(".");
  CHECK(shandalar_mod_load(archive));
  for (i = 0; i < SDL_arraysize(patterns); ++i) {
    ShandalarFileList files;
    const char *slash = strrchr(patterns[i], '/');
    CHECK(shandalar_list_files(patterns[i], &files));
    for (j = 0; j < files.count; ++j) {
      char path[1024];
      HBITMAP bitmap;
      BITMAP info;
      SDL_snprintf(path, sizeof(path), "%.*s%s", slash ? (int)(slash - patterns[i] + 1) : 0,
                   patterns[i], files.names[j]);
      bitmap = load_pic(path);
      if (!bitmap) fprintf(stderr, "Could not decode %s\n", path);
      CHECK(bitmap && GetObject(bitmap, sizeof(info), &info));
      CHECK(info.bmWidth > 0 && info.bmHeight > 0);
      DeleteObject(bitmap);
      ++count;
    }
    shandalar_free_file_list(&files);
  }
  shandalar_mod_unload();
  printf("Decoded %zu ZIP-backed proprietary pictures from %s\n", count, archive);
  return 0;
}

int main(int argc, char **argv)
{
  /* Minimal one-pixel X0 PIC: 9-bit literal 7 after an 11-bit dictionary header. */
  unsigned char pic[] = {'X', '0', 8, 0, 1, 0, 1, 0, 11, 7, 0, 0};
  unsigned char pcx[130] = {0};
  const char ini[] = "[Disable]\r\n1\r\n1\r\n[Configs]\r\nextra.ini\r\n";
  const char extra[] = "[Nochoose]\r\n2";
  const char text[] = "@ZIP_TEST\r\n2\r\nOverridden\r\nLast line";
  const char scan[] = "12 -34,56\r\n.name\r\n.123 tail\r\n";
  const char deck[] = ";ZIP-only deck\r\n.1\t2\tCard\r\n";
  mz_zip_archive zip;
  void *zip_bytes;
  size_t zip_size;
  SDL_IOStream *io;
  char line[128], token[128];
  int a, b, c;
  long d;
  dictionary *dict;
  ShandalarFileList files;
  ShandalarFindData found;
  intptr_t handle;
  HBITMAP bitmap;
  BITMAP info;
  CardPool pool = {0};
  CardPoolCard cards[] = {{0, "Plains", 1, 1}, {1, "Card", 1, 0}, {2, "Other", 1, 0}};
#ifdef __EMSCRIPTEN__
  /* Node has no browser XMLHttpRequest. The GDI default font is incidental
   * to bitmap decoding: keep its probe local instead of requesting a server. */
  CHECK(SDL_CreateDirectory("/stream-test-fonts"));
  CHECK(SDL_SaveFile("/stream-test-fonts/TT0127M_.TTF", "", 0));
  shandalar_set_default_font_root("/stream-test-fonts");
#endif
  if (argc == 2) return inspect_pictures(argv[1]);
  CHECK(SDL_CreateDirectory(global_base_directory));
  CHECK(SDL_CreateDirectory("stream-test-assets/Decks"));
  shandalar_assets_set_root(global_base_directory);
  CHECK(SDL_SaveFile("stream-test-assets/Decks/duplicate.dck", "base", 4));
  CHECK(SDL_SaveFile("stream-test-assets/Decks/base.dck", "base", 4));
  CHECK(SDL_SaveFile("stream-test-assets/Shandalar.ini", "[Enable]\n1\n", 11));
  memset(&zip, 0, sizeof(zip));
  CHECK(mz_zip_writer_init_heap(&zip, 0, 0));
  pcx[0] = 0x0a; pcx[1] = 5; pcx[2] = 1; pcx[3] = 8;
  pcx[65] = 1; pcx[66] = 2; pcx[128] = 9; pcx[129] = 0;
  CHECK(add(&zip, "test.pic", pic, sizeof(pic)));
  CHECK(add(&zip, "test.pcx", pcx, sizeof(pcx)));
  CHECK(add(&zip, "Shandalar.ini", ini, sizeof(ini)-1));
  CHECK(add(&zip, "extra.ini", extra, sizeof(extra)-1));
  CHECK(add(&zip, "Text.res", text, sizeof(text)-1));
  CHECK(add(&zip, "Menus.txt", "@EXPANSIONNAMES\r\n0\r\n", 20));
  CHECK(add(&zip, "scan.txt", scan, sizeof(scan)-1));
  CHECK(add(&zip, "Decks/DUPLICATE.DCK", deck, sizeof(deck)-1));
  CHECK(add(&zip, "Decks/new.dck", deck, sizeof(deck)-1));
  CHECK(add(&zip, "Decks/subdir/hidden.dck", deck, sizeof(deck)-1));
  CHECK(mz_zip_writer_finalize_heap_archive(&zip, &zip_bytes, &zip_size));
  mz_zip_writer_end(&zip);
  CHECK(SDL_SaveFile("asset-stream-test.zip", zip_bytes, zip_size));
  mz_free(zip_bytes);
  CHECK(shandalar_mod_load("asset-stream-test.zip"));
  CHECK(shandalar_assets_is_ready("test.pic"));
  CHECK(shandalar_file_exists("stream-test-assets/test.pic"));
  CHECK(shandalar_file_exists("stream-test-assets/Decks/new.dck"));
  CHECK(!shandalar_mod_contains("stream-test-assets/missing.bin"));
#ifndef __EMSCRIPTEN__
  CHECK(!shandalar_file_exists("stream-test-assets/missing.bin"));
#endif

  io = shandalar_open_file_io("scan.txt", "rt");
  CHECK(io);
  CHECK(shandalar_io_scanf(io, "%d %d,%ld", &a, &b, &d) == 3 && a == 12 && b == -34 && d == 56);
  CHECK(shandalar_io_gets(line, sizeof(line), io) && !strcmp(line, "\n"));
  CHECK(shandalar_io_scanf(io, "%[^\n]%*c", token) == 1 && !strcmp(token, ".name"));
  CHECK(shandalar_io_scanf(io, "%[.]", token) == 1 && !strcmp(token, "."));
  CHECK(shandalar_io_scanf(io, "%d", &c) == 1 && c == 123);
  CHECK(shandalar_io_gets(line, sizeof(line), io) && !strcmp(line, " tail\n"));
  CHECK(shandalar_io_scanf(io, "%d", &a) == EOF);
  CHECK(shandalar_io_seek(io, 0, SEEK_SET) == 0);
  CHECK(shandalar_io_scanf(io, "%2[0-9]", token) == 1 && !strcmp(token, "12"));
  CHECK(shandalar_io_close(io) == 0);
  io = shandalar_open_file_io("stream-test-output.txt", "wt");
  CHECK(io && shandalar_io_printf(io, "%s:%d\n", "disk", 42) == 8);
  CHECK(shandalar_io_flush(io) == 0 && shandalar_io_close(io) == 0);
  io = shandalar_open_file_io("stream-test-output.txt", "rt");
  CHECK(io && shandalar_io_gets(line, sizeof(line), io) && !strcmp(line, "disk:42\n"));
  CHECK(shandalar_io_close(io) == 0 && SDL_RemovePath("stream-test-output.txt"));
  /* Byte reads preserve CRLF; line reads normalize it. Seek offsets stay physical. */
  io = shandalar_open_file_io("Decks/new.dck", "rt");
  CHECK(io && shandalar_io_gets(line, sizeof(line), io) && !strcmp(line, ";ZIP-only deck\n"));
  CHECK(SDL_TellIO(io) == 16);
  CHECK(shandalar_io_seek(io, -2, SEEK_CUR) == 0 && shandalar_io_getc(io) == '\r');
  CHECK(shandalar_io_close(io) == 0);

  dict = iniparser_load("stream-test-assets/Shandalar.ini");
  CHECK(!dict); /* The standard parser still rejects bare keys. */
  io = shandalar_open_file_io("stream-test-assets/Shandalar.ini", "rt");
  CHECK(io);
  dict = iniparser_load_io_entries(io, "Shandalar.ini", collect, NULL);
  CHECK(dict && entries == 3);
  iniparser_freedict(dict); SDL_CloseIO(io);
  CHECK(card_pool_load(&pool, global_base_directory, cards, SDL_arraysize(cards)));
  CHECK(!card_pool_allows(&pool, 1, CARD_POOL_NOCHOOSE));
  CHECK(!card_pool_allows(&pool, 2, CARD_POOL_NOSALE) && card_pool_allows(&pool, 2, CARD_POOL_NOCHOOSE));
  card_pool_free(&pool);
  CHECK(load_text(NULL, "ZIP_TEST") == 2 && !strcmp(g_text_lines[0], "Overridden") &&
        !strcmp(g_text_lines[1], "Last line"));

  {
    DWORD size, bytes_read;
    HANDLE file = CreateFileA("stream-test-assets/Text.res", GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, 0, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    size = GetFileSize(file, NULL);
    CHECK(size == sizeof(text)-1 && ReadFile(file, token, size, &bytes_read, NULL));
    CHECK(bytes_read == size && !memcmp(token, text, size));
    CHECK(ReadFile(file, token, 1, &bytes_read, NULL) && bytes_read == 0);
    CHECK(CloseHandle(file));
    CHECK(!ReadFile(file, token, 1, &bytes_read, NULL));
  }
  bitmap = load_pic("test.pic");
  CHECK(bitmap && GetObject(bitmap, sizeof(info), &info));
  CHECK(info.bmWidth == 1 && info.bmHeight == 1 && ((unsigned char *)info.bmBits)[0] == 7);
  DeleteObject(bitmap);
  bitmap = load_legacy_pic("test.pcx");
  CHECK(bitmap && GetObject(bitmap, sizeof(info), &info));
  CHECK(info.bmWidth == 4 && info.bmHeight == 1 && ((unsigned char *)info.bmBits)[0] == 9);
  DeleteObject(bitmap);
  CHECK(shandalar_list_files("stream-test-assets/DECKS/*.DCK", &files));
  CHECK(files.count == 3 && !SDL_strcasecmp(files.names[0], "base.dck") &&
        !SDL_strcasecmp(files.names[1], "duplicate.dck") && !SDL_strcasecmp(files.names[2], "new.dck"));
  handle = shandalar_io_findfirst("stream-test-assets/Decks/new.dck", &found);
  CHECK(handle != -1 && found.time_write == SDL_MAX_SINT64);
  CHECK(shandalar_io_findnext(handle, &found) == -1 && shandalar_io_findclose(handle) == 0);
  /* Discovery results retain their own storage after archive unloading. */
  shandalar_mod_unload();
  CHECK(files.count == 3 && !strcmp(files.names[2], "new.dck"));
  shandalar_free_file_list(&files);
  CHECK(shandalar_list_files("stream-test-assets/Decks/*.dck", &files) && files.count == 2);
  shandalar_free_file_list(&files);
  SDL_RemovePath("asset-stream-test.zip");
  SDL_RemovePath("stream-test-assets/Decks/duplicate.dck");
  SDL_RemovePath("stream-test-assets/Decks/base.dck");
  SDL_RemovePath("stream-test-assets/Decks");
  SDL_RemovePath("stream-test-assets/Shandalar.ini");
  SDL_RemovePath(global_base_directory);
  puts("ZIP-backed text, INI/includes, card pool, Win32 reads, PIC/PCX and file discovery passed.");
  return 0;
}
