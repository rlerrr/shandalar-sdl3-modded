#include "platform/mod_overlay.h"
#include "platform/asset_manager.h"

#include <SDL3/SDL.h>
#include <miniz.h>
#include <stdlib.h>
#include <string.h>

typedef struct ModEntry {
  char *name;
  mz_uint index;
  size_t size;
} ModEntry;

static mz_zip_archive g_zip;
static void *g_zip_bytes;
static ModEntry *g_entries;
static size_t g_entry_count;

/* Collapse separators and dot components, but never allow parent traversal.
 * Preserve the leading slash so absolute paths cannot alias relative assets. */
static char *normalize_path(const char *path)
{
  size_t length = strlen(path);
  char *result = (char *)SDL_malloc(length + 1);
  size_t used = 0;
  const char *cursor = path;
  if (!result)
    return NULL;
  if (*cursor == '/' || *cursor == '\\')
    result[used++] = '/';
  while (*cursor) {
    const char *start;
    size_t count, i;
    while (*cursor == '/' || *cursor == '\\')
      ++cursor;
    start = cursor;
    while (*cursor && *cursor != '/' && *cursor != '\\')
      ++cursor;
    count = (size_t)(cursor - start);
    if (!count || (count == 1 && start[0] == '.'))
      continue;
    if (count == 2 && start[0] == '.' && start[1] == '.') {
      SDL_free(result);
      return NULL;
    }
    if (used && result[used - 1] != '/')
      result[used++] = '/';
    for (i = 0; i < count; ++i) {
      unsigned char c = (unsigned char)start[i];
      result[used++] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);
    }
  }
  result[used] = '\0';
  return result;
}

static int compare_entries(const void *a, const void *b)
{
  return strcmp(((const ModEntry *)a)->name, ((const ModEntry *)b)->name);
}

static void free_entries(ModEntry *entries, size_t count)
{
  size_t i;
  for (i = 0; i < count; ++i)
    SDL_free(entries[i].name);
  SDL_free(entries);
}

int shandalar_mod_load(const char *zip_path)
{
  mz_zip_archive zip;
  void *bytes;
  size_t size, count = 0, i;
  mz_uint total;
  ModEntry *entries;
  if (g_zip_bytes)
    return SDL_SetError("Only one mod ZIP can be loaded at a time");
  if (!zip_path || !zip_path[0])
    return SDL_SetError("A mod ZIP path is required");
  bytes = SDL_LoadFile(zip_path, &size);
  if (!bytes)
    return 0;
  memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_mem(&zip, bytes, size, 0)) {
    SDL_free(bytes);
    return SDL_SetError("Invalid mod ZIP: %s", zip_path);
  }
  total = mz_zip_reader_get_num_files(&zip);
  entries = (ModEntry *)SDL_calloc(total ? total : 1, sizeof(*entries));
  if (!entries)
    goto fail;
  for (i = 0; i < total; ++i) {
    mz_zip_archive_file_stat stat;
    mz_uint name_size;
    char *name, *normalized;
    if (!mz_zip_reader_file_stat(&zip, (mz_uint)i, &stat)) {
      SDL_SetError("Invalid mod ZIP entry %u", (unsigned)i);
      goto fail;
    }
    if (stat.m_is_directory)
      continue;
    if (!stat.m_is_supported || stat.m_is_encrypted || stat.m_uncomp_size > SIZE_MAX) {
      SDL_SetError("Unsupported mod ZIP entry: %s", stat.m_filename);
      goto fail;
    }
    name_size = mz_zip_reader_get_filename(&zip, (mz_uint)i, NULL, 0);
    name = (char *)SDL_malloc(name_size);
    if (!name)
      goto fail;
    mz_zip_reader_get_filename(&zip, (mz_uint)i, name, name_size);
    /* Embedded NULs would make two different archive names alias. */
    normalized = strlen(name) + 1 == name_size ? normalize_path(name) : NULL;
    SDL_free(name);
    if (!normalized || !normalized[0] || normalized[0] == '/' || strchr(normalized, ':')) {
      SDL_free(normalized);
      SDL_SetError("Mod ZIP paths must be relative, without '..': %s", stat.m_filename);
      goto fail;
    }
    entries[count].name = normalized;
    entries[count].size = (size_t)stat.m_uncomp_size;
    entries[count++].index = (mz_uint)i;
  }
  qsort(entries, count, sizeof(*entries), compare_entries);
  for (i = 1; i < count; ++i) {
    if (strcmp(entries[i - 1].name, entries[i].name) == 0) {
      SDL_SetError("Duplicate mod ZIP path: %s", entries[i].name);
      goto fail;
    }
  }
  g_zip = zip;
  /* miniz's memory reader stores a pointer to its archive in the callback. */
  g_zip.m_pIO_opaque = &g_zip;
  g_zip_bytes = bytes;
  g_entries = entries;
  g_entry_count = count;
  return 1;

fail:
  free_entries(entries, count);
  mz_zip_reader_end(&zip);
  SDL_free(bytes);
  return 0;
}

void shandalar_mod_unload(void)
{
  if (!g_zip_bytes)
    return;
  mz_zip_reader_end(&g_zip);
  SDL_free(g_zip_bytes);
  free_entries(g_entries, g_entry_count);
  memset(&g_zip, 0, sizeof(g_zip));
  g_zip_bytes = NULL;
  g_entries = NULL;
  g_entry_count = 0;
}

int shandalar_mod_init_from_args(int *argc, char **argv)
{
  const char *path = NULL;
  int i, out = 1;
  /* Validate first, so invalid options do not partially rewrite argv. */
  for (i = 1; i < *argc; ++i) {
    const char *candidate;
    if (strcmp(argv[i], "--mod") == 0) {
      if (++i >= *argc || !argv[i][0] || strncmp(argv[i], "--", 2) == 0)
        return SDL_SetError("--mod requires a ZIP path");
      candidate = argv[i];
    } else if (strncmp(argv[i], "--mod=", 6) == 0) {
      candidate = argv[i] + 6;
      if (!candidate[0])
        return SDL_SetError("--mod requires a ZIP path");
    } else {
      continue;
    }
    if (path)
      return SDL_SetError("Specify --mod only once");
    path = candidate;
  }
  if (!path)
    path = getenv("SHANDALAR_MOD_ZIP");
  if (path && path[0] && !shandalar_mod_load(path))
    return 0;
  for (i = 1; i < *argc; ++i) {
    if (strcmp(argv[i], "--mod") == 0)
      ++i;
    else if (strncmp(argv[i], "--mod=", 6) != 0)
      argv[out++] = argv[i];
  }
  argv[out] = NULL;
  *argc = out;
  return 1;
}

/* Convert both root-qualified paths and relative asset names to ZIP keys. */
static char *relative_path(const char *path)
{
  char *normalized = normalize_path(path);
  char *root = normalize_path(shandalar_assets_root());
  const char *relative;
  size_t root_length;
  if (!normalized || !root) {
    SDL_free(normalized); SDL_free(root); return NULL;
  }
  relative = normalized;
  root_length = strlen(root);
  if (strcmp(relative, root) == 0) relative += root_length;
  else if (root_length && strncmp(relative, root, root_length) == 0 && relative[root_length] == '/')
    relative += root_length + 1;
  else if (strcmp(root, "/") == 0 && relative[0] == '/') ++relative;
  SDL_free(root);
  if (relative[0] == '/' || strchr(relative, ':')) { SDL_free(normalized); return NULL; }
  memmove(normalized, relative, strlen(relative) + 1);
  return normalized;
}

static ModEntry *find_entry(const char *path)
{
  char *relative;
  ModEntry key, *entry;
  if (!g_zip_bytes) return NULL;
  relative = relative_path(path);
  if (!relative) return NULL;
  key.name = relative;
  entry = (ModEntry *)bsearch(&key, g_entries, g_entry_count, sizeof(*g_entries), compare_entries);
  SDL_free(relative);
  return entry;
}

int shandalar_mod_contains(const char *path) { return find_entry(path) != NULL; }

int shandalar_mod_enumerate(const char *directory,
                           int (*callback)(void *, const char *), void *context)
{
  char *relative;
  size_t length, i;
  int result = 1;
  if (!g_zip_bytes) return 1;
  relative = relative_path(directory);
  if (!relative) return 1;
  length = strlen(relative);
  for (i = 0; i < g_entry_count; ++i) {
    const char *name = g_entries[i].name;
    if (length) {
      if (strncmp(name, relative, length) != 0 || name[length] != '/') continue;
      name += length + 1;
    }
    if (!strchr(name, '/') && !callback(context, name)) { result = 0; break; }
  }
  SDL_free(relative);
  return result;
}

SDL_IOStream *shandalar_mod_open(const char *path, int *found)
{
  ModEntry *entry = find_entry(path);
  size_t size;
  void *data;
  SDL_IOStream *io;
  *found = entry != NULL;
  if (!entry) return NULL;
  /* Each open gets independent seek state and decoded bytes. CRC errors must
   * fail the read, never silently fall back to the unmodified asset. */
  size = entry->size;
  data = SDL_malloc(size ? size : 1);
  if (!data)
    return NULL;
  if (!mz_zip_reader_extract_to_mem(&g_zip, entry->index, data, size, 0)) {
    SDL_SetError("Could not decode mod entry %s: %s", entry->name,
                 mz_zip_get_error_string(mz_zip_get_last_error(&g_zip)));
    SDL_free(data);
    return NULL;
  }
  io = SDL_IOFromConstMem(data, size);
  if (!io) {
    SDL_free(data);
    return NULL;
  }
  if (!SDL_SetPointerProperty(SDL_GetIOProperties(io),
                              SDL_PROP_IOSTREAM_MEMORY_FREE_FUNC_POINTER, (void *)SDL_free)) {
    SDL_CloseIO(io);
    SDL_free(data);
    return NULL;
  }
  return io;
}
