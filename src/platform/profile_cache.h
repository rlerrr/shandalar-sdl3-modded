#ifndef SHANDALAR_PROFILE_CACHE_H
#define SHANDALAR_PROFILE_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Profile assets are cached after a successful read. Drop snapshots when
 * changing asset roots/mods, explicitly reloading configs, or writing a file. */
void shandalar_profile_cache_clear(void);
void shandalar_profile_cache_invalidate(const char *filename);

#ifdef __cplusplus
}
#endif

#endif
