#include "platform/file_io.h"
#include "platform/mod_overlay.h"
#include "platform/legacy_io.h"
#include <SDL3/SDL.h>
#include <string.h>

typedef struct ListBuilder {
  ShandalarFileList *list;
  const char *pattern;
  int failed;
} ListBuilder;

static int wildcard_matches(const char *pattern, const char *name)
{
  const char *star = NULL, *retry = NULL;
  while (*name) {
    if (*pattern == '?' || (*pattern && SDL_tolower((unsigned char)*pattern) ==
                                       SDL_tolower((unsigned char)*name))) {
      ++pattern; ++name;
    } else if (*pattern == '*') { star = pattern++; retry = name; }
    else if (star) { pattern = star + 1; name = ++retry; }
    else return 0;
  }
  while (*pattern == '*') ++pattern;
  return !*pattern;
}

static int append_name(void *context, const char *name)
{
  ListBuilder *builder = (ListBuilder *)context;
  ShandalarFileList *list = builder->list;
  char **names;
  char *copy;
  size_t i;
  if (!wildcard_matches(builder->pattern, name)) return 1;
  for (i = 0; i < list->count; ++i)
    if (!SDL_strcasecmp(list->names[i], name)) return 1;
  copy = SDL_strdup(name);
  if (!copy) { builder->failed = 1; return 0; }
  names = (char **)SDL_realloc(list->names, (list->count + 1) * sizeof(*names));
  if (!names) { SDL_free(copy); builder->failed = 1; return 0; }
  list->names = names;
  list->names[list->count++] = copy;
  return 1;
}

static SDL_EnumerationResult SDLCALL append_disk_file(void *context,
                                                      const char *directory,
                                                      const char *name)
{
  SDL_PathInfo info;
  char *path = NULL;
  if (SDL_asprintf(&path, "%s/%s", directory, name) < 0) {
    ((ListBuilder *)context)->failed = 1;
    return SDL_ENUM_FAILURE;
  }
  if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE &&
      !append_name(context, name)) {
    SDL_free(path);
    return SDL_ENUM_FAILURE;
  }
  SDL_free(path);
  return SDL_ENUM_CONTINUE;
}

static int compare_names(const void *a, const void *b)
{
  return SDL_strcasecmp(*(char *const *)a, *(char *const *)b);
}

void shandalar_free_file_list(ShandalarFileList *list)
{
  size_t i;
  for (i = 0; i < list->count; ++i) SDL_free(list->names[i]);
  SDL_free(list->names);
  memset(list, 0, sizeof(*list));
}

int shandalar_list_files(const char *pattern, ShandalarFileList *list)
{
  char *path, *slash;
  const char *directory, *wildcard;
  ListBuilder builder;
  if (!pattern || !list) return 0;
  memset(list, 0, sizeof(*list));
  path = SDL_strdup(pattern);
  if (!path) return 0;
  for (char *cursor = path; *cursor; ++cursor) if (*cursor == '\\') *cursor = '/';
  slash = strrchr(path, '/');
  if (slash) { *slash = 0; directory = path[0] ? path : "/"; wildcard = slash + 1; }
  else { directory = "."; wildcard = path; }
  builder.list = list;
  builder.pattern = wildcard;
  builder.failed = 0;
  shandalar_mod_enumerate(directory, append_name, &builder);
#ifndef _WIN32
  {
    char resolved[2048];
    if (shandalar_resolve_existing_path(directory, resolved, sizeof(resolved)))
      SDL_EnumerateDirectory(resolved, append_disk_file, &builder);
  }
#else
  SDL_EnumerateDirectory(directory, append_disk_file, &builder);
#endif
  SDL_free(path);
  if (builder.failed) { shandalar_free_file_list(list); return 0; }
  if (list->count > 1) SDL_qsort(list->names, list->count, sizeof(*list->names), compare_names);
  return 1;
}

typedef struct FindFiles {
  ShandalarFileList list;
  size_t index;
  char *directory;
} FindFiles;

int shandalar_io_findnext(intptr_t handle, ShandalarFindData *data)
{
  FindFiles *state = (FindFiles *)handle;
  SDL_PathInfo info;
  char *path = NULL;
  if (!state || handle == -1 || !data || state->index >= state->list.count) return -1;
  SDL_strlcpy(data->name, state->list.names[state->index++], sizeof(data->name));
  data->time_write = 0;
  if (SDL_asprintf(&path, "%s/%s", state->directory, data->name) >= 0) {
    if (shandalar_mod_contains(path)) data->time_write = SDL_MAX_SINT64;
    else if (SDL_GetPathInfo(path, &info)) data->time_write = info.modify_time / SDL_NS_PER_SECOND;
    SDL_free(path);
  }
  return 0;
}

intptr_t shandalar_io_findfirst(const char *pattern, ShandalarFindData *data)
{
  FindFiles *state = (FindFiles *)SDL_calloc(1, sizeof(*state));
  char *slash;
  if (!state) return -1;
  if (!shandalar_list_files(pattern, &state->list)) { SDL_free(state); return -1; }
  /* Exact-name probes used by Facemaker also trigger browser lazy fetching,
   * just as the original _findfirst compatibility implementation did. */
  if (!state->list.count && !strpbrk(pattern, "*?") && shandalar_file_exists(pattern)) {
    shandalar_free_file_list(&state->list);
    if (!shandalar_list_files(pattern, &state->list)) { SDL_free(state); return -1; }
  }
  state->directory = SDL_strdup(pattern);
  if (!state->directory) { shandalar_free_file_list(&state->list); SDL_free(state); return -1; }
  for (char *cursor = state->directory; *cursor; ++cursor) if (*cursor == '\\') *cursor = '/';
  slash = strrchr(state->directory, '/');
  if (slash && slash != state->directory) *slash = 0;
  else if (slash) slash[1] = 0;
  else { SDL_free(state->directory); state->directory = SDL_strdup("."); }
#ifndef _WIN32
  if (state->directory) {
    char resolved[2048];
    if (shandalar_resolve_existing_path(state->directory, resolved, sizeof(resolved))) {
      char *copy = SDL_strdup(resolved);
      SDL_free(state->directory);
      state->directory = copy;
    }
  }
#endif
  if (!state->directory || shandalar_io_findnext((intptr_t)state, data) != 0) {
    shandalar_io_findclose((intptr_t)state);
    return -1;
  }
  return (intptr_t)state;
}

int shandalar_io_findclose(intptr_t handle)
{
  FindFiles *state = (FindFiles *)handle;
  if (!state || handle == -1) return -1;
  shandalar_free_file_list(&state->list);
  SDL_free(state->directory);
  SDL_free(state);
  return 0;
}
