#include <stdint.h>
#include "platform/legacy_io.h"
#include "platform/file_io.h"
#include <SDL3/SDL.h>
#include <emscripten.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
  fprintf(stderr, "Line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(int argc, char **argv)
{
  const char *names[] = {"magic0.sve", "MAGICa.MAP", "magicf.fce", "saveDescs",
                         "registry.ini", "FaceMostRecent.txt", "face.pic", "MANALINK.INI"};
  const char *mapped[] = {"magic0.sve", "magica.map", "magicf.fce", "saveDescs",
                          "registry.ini", "FaceMostRecent.txt", "face.pic", "manalink.ini"};
  const char *root;
  char path[256], contents[16];
  size_t i;
  CHECK(argc == 2);
  /* Node has no IndexedDB. Exercise real path routing and close/sync without
   * the browser persistence backend; the host supplies the namespace. */
  EM_ASM({
    if (typeof window === 'undefined') {
      FS.mkdirTree(Module['userDataRoot'] || '/userdata');
      Module.__shandalarUserDataReady = 1;
    }
  });
  root = shandalar_user_data_root();
  CHECK(!strcmp(root, argv[1]));
  {
    const char defaults[] = "[DeckBuilder]\nSort=2\n";
    SDL_IOStream *io;
    CHECK(SDL_SaveFile("Manalink.ini", defaults, sizeof(defaults) - 1));
    io = shandalar_open_file_io(".\\MANALINK.INI", "rb");
    CHECK(io);
    CHECK(SDL_CloseIO(io));
    snprintf(path, sizeof(path), "%s/manalink.ini", root);
    CHECK(SDL_GetPathInfo(path, NULL));
  }
  for (i = 0; i < SDL_arraysize(names); ++i) {
    SDL_IOStream *io = shandalar_open_file_io(names[i], "wb");
    CHECK(io && SDL_WriteIO(io, "mod save", 8) == 8 && SDL_CloseIO(io));
    snprintf(path, sizeof(path), "%s/%s", root, mapped[i]);
    CHECK(SDL_GetPathInfo(path, NULL));
    io = shandalar_open_file_io(names[i], "rb");
    memset(contents, 0, sizeof(contents));
    CHECK(io && SDL_ReadIO(io, contents, 8) == 8 && !strcmp(contents, "mod save") && SDL_CloseIO(io));
    if (strcmp(root, "/userdata")) {
      snprintf(path, sizeof(path), "/userdata/%s", mapped[i]);
      CHECK(!SDL_GetPathInfo(path, NULL));
    }
  }
  {
    size_t size;
    char *defaults = SDL_LoadFile("Manalink.ini", &size);
    CHECK(defaults && !strcmp(defaults, "[DeckBuilder]\nSort=2\n"));
    SDL_free(defaults);
  }
  {
    int fd = _open("magic1.sve", _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
    CHECK(fd >= 0 && _write(fd, "fd save", 7) == 7 && _close(fd) == 0);
    snprintf(path, sizeof(path), "%s/magic1.sve", root);
    CHECK(SDL_GetPathInfo(path, NULL));
  }
  puts("Mod user data stream/fd routing and base save isolation passed.");
  return 0;
}
