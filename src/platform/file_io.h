#ifndef SHANDALAR_FILE_IO_H
#define SHANDALAR_FILE_IO_H

#include <SDL3/SDL_iostream.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read-only opens check the in-memory mod ZIP first, then the platform's
 * legacy I/O layer. Writes always use the normal filesystem. The returned
 * SDL_IOStream owns its file or decoded bytes; close it with SDL_CloseIO().
 */
SDL_IOStream *shandalar_open_file_io(const char *path, const char *mode);

/* Small typed adapters for legacy readers. Binary reads retain byte offsets;
 * line/token readers accept both LF and CRLF, including memory-backed ZIPs. */
size_t shandalar_io_read(void *ptr, size_t size, size_t count, SDL_IOStream *io);
size_t shandalar_io_write(const void *ptr, size_t size, size_t count, SDL_IOStream *io);
int shandalar_io_seek(SDL_IOStream *io, Sint64 offset, int whence);
int shandalar_io_getc(SDL_IOStream *io);
int shandalar_io_text_getc(SDL_IOStream *io);
char *shandalar_io_gets(char *str, int count, SDL_IOStream *io);
int shandalar_io_eof(SDL_IOStream *io);
int shandalar_io_error(SDL_IOStream *io);
int shandalar_io_close(SDL_IOStream *io);
int shandalar_io_flush(SDL_IOStream *io);
void shandalar_io_rewind(SDL_IOStream *io);
int shandalar_io_printf(SDL_IOStream *io, const char *format, ...) SDL_PRINTF_VARARG_FUNC(2);
int shandalar_io_vprintf(SDL_IOStream *io, const char *format, va_list args) SDL_PRINTF_VARARG_FUNCV(2);
/* Integer, string, character and scanset conversions used by legacy assets.
 * Unsupported conversions fail with SDL_GetError(); no FILE* emulation. */
int shandalar_io_scanf(SDL_IOStream *io, const char *format, ...) SDL_SCANF_VARARG_FUNC(2);

typedef struct ShandalarFileList {
  size_t count;
  char **names;
} ShandalarFileList;
/* Immediate regular files, disk + ZIP, case-insensitive wildcard/deduplication.
 * The returned snapshot remains valid after unloading the mod. */
int shandalar_list_files(const char *pattern, ShandalarFileList *list);
void shandalar_free_file_list(ShandalarFileList *list);
int shandalar_file_exists(const char *path);
typedef struct ShandalarFindData {
  Sint64 time_write;
  char name[260];
} ShandalarFindData;
intptr_t shandalar_io_findfirst(const char *pattern, ShandalarFindData *data);
int shandalar_io_findnext(intptr_t handle, ShandalarFindData *data);
int shandalar_io_findclose(intptr_t handle);

#ifdef __cplusplus
}
#endif

#endif
