#include <stdint.h>
#include <string.h>
#include "platform/legacy_io.h"
#include "platform/asset_manager.h"

static int fetches;

const char *shandalar_assets_root(void)
{
  return "/assets_modded";
}

ShandalarAssetState shandalar_assets_fetch_blocking(const char *path)
{
  FILE *file;
  ++fetches;
  if (strcmp(path, "Sound/test.wav") != 0)
    return SHANDALAR_ASSET_ERROR;
  mkdir("/assets_modded/sound", 0777);
  file = fopen("/assets_modded/sound/test.wav", "wb");
  if (!file)
    return SHANDALAR_ASSET_ERROR;
  fputs("streamed sound", file);
  fclose(file);
  return SHANDALAR_ASSET_READY;
}

int main(void)
{
  FILE *file;
  char bytes[16] = {0};
  mkdir("/assets_modded", 0777);
  mkdir("/assets_modded/cardart", 0777);
  mkdir("/assets_modded/cardart/scheme", 0777);
  file = fopen("/assets_modded/cardart/scheme/cardbk_special.png", "wb");
  if (!file)
    return 1;
  fputs("scheme fixture", file);
  fclose(file);

  /* Exactly the Windows-style path produced by Scheme.dat's frame loader. */
  file = fopen("\\assets_modded\\CARDART\\scheme\\CardBk_Special.png", "rb");
  if (!file)
    return 2;
  fread(bytes, 1, sizeof(bytes) - 1, file);
  fclose(file);
  if (strcmp(bytes, "scheme fixture") != 0 || fetches != 0)
    return 3;
  puts("Mixed-case Windows Scheme path opens from the browser VFS without fetching.");

  memset(bytes, 0, sizeof(bytes));
  file = fopen("\\assets_modded\\Sound\\test.wav", "rb");
  if (!file)
    return 4;
  fread(bytes, 1, sizeof(bytes) - 1, file);
  fclose(file);
  if (strcmp(bytes, "streamed sound") != 0 || fetches != 1)
    return 5;
  file = fopen("\\assets_modded\\Sound\\test.wav", "rb");
  if (!file)
    return 6;
  fclose(file);
  if (fetches != 1)
    return 7;
  puts("Unbundled sound is fetched on first open and reused on subsequent opens.");
  return 0;
}
