#ifndef SHANDALAR_MOD_OVERLAY_H
#define SHANDALAR_MOD_OVERLAY_H

#include <SDL3/SDL_iostream.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One read-only ZIP at a time. Names are relative to the asset root and match
 * case-insensitively, with either slash style. No files are extracted to disk.
 * Failures report SDL_GetError(). Call on the same thread as asset loading. */
int shandalar_mod_load(const char *zip_path);
void shandalar_mod_unload(void);

/* Removes --mod=PATH / --mod PATH from argv, or uses SHANDALAR_MOD_ZIP when
 * there is no command-line selection. Load before any chdir or asset reads. */
int shandalar_mod_init_from_args(int *argc, char **argv);

/* Internal stream-layer hook: NULL with *found == 0 means normal filesystem
 * fallback; NULL with *found == 1 means an overlay entry failed to decode.
 * Open streams own their decoded bytes and survive overlay unloading. */
SDL_IOStream *shandalar_mod_open(const char *path, int *found);
int shandalar_mod_contains(const char *path);
/* Enumerate immediate files without decompressing them. Callback may stop
 * with zero; names are normalized and valid only for the callback duration. */
int shandalar_mod_enumerate(const char *directory,
                           int (*callback)(void *, const char *), void *context);

#ifdef __cplusplus
}
#endif

#endif
