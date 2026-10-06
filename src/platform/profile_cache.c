#include "platform/profile_cache.h"
#include "platform/gdi_compat.h"
#include "platform/dictionary.h"
#include "platform/file_io.h"
#include "platform/legacy_io.h"

#include <stdlib.h>
#include <string.h>

typedef struct ProfileFile {
  char path[1024];
  dictionary *values;
  struct ProfileFile *next;
} ProfileFile;

static ProfileFile *g_profiles;

static void profile_path(char *path, size_t size, const char *filename)
{
  SDL_strlcpy(path, filename ? filename : "", size);
  for (char *cursor = path; *cursor; ++cursor)
    if (*cursor == '\\')
      *cursor = '/';
#ifdef __EMSCRIPTEN__
  {
    char user_path[1024];
    if (shandalar_user_data_path(path, user_path, sizeof(user_path)))
      SDL_strlcpy(path, user_path, size);
  }
#endif
}

static int profile_key(char *out, size_t size, const char *section, const char *key)
{
  /* Include the section length so colons in section/key names cannot alias. */
  int length = SDL_snprintf(out, size, "%zu:%s:%s", strlen(section), section, key);
  if (length < 0 || (size_t)length >= size)
    return 0;
  for (char *cursor = out; *cursor; ++cursor)
    *cursor = (char)SDL_tolower((unsigned char)*cursor);
  return 1;
}

void shandalar_profile_cache_invalidate(const char *filename)
{
  char path[1024];
  ProfileFile **link = &g_profiles;
  profile_path(path, sizeof(path), filename);
  while (*link) {
    ProfileFile *profile = *link;
    if (SDL_strcasecmp(profile->path, path) == 0) {
      *link = profile->next;
      dictionary_del(profile->values);
      free(profile);
    } else {
      link = &profile->next;
    }
  }
}

void shandalar_profile_cache_clear(void)
{
  while (g_profiles) {
    ProfileFile *profile = g_profiles;
    g_profiles = profile->next;
    dictionary_del(profile->values);
    free(profile);
  }
}

static dictionary *load_profile(SDL_IOStream *file)
{
  dictionary *values = dictionary_new(0);
  char line[512], section[512], lookup[1040];
  int have_section = 0;
  if (!values)
    return NULL;
  /* Reuse iniparser's dictionary, but retain the compatibility shim's
   * permissive syntax, literal backslashes/comments, and first-match rule. */
  while (shandalar_io_gets(line, sizeof(line), file)) {
    char *cursor = line;
    char *equals, *value, *end;
    while (*cursor == ' ' || *cursor == '\t')
      ++cursor;
    if (*cursor == '[') {
      char *close = strchr(cursor + 1, ']');
      if (close) {
        *close = '\0';
        SDL_strlcpy(section, cursor + 1, sizeof(section));
        have_section = 1;
      }
      continue;
    }
    if (!have_section || *cursor == ';' || *cursor == '#' || !*cursor)
      continue;
    equals = strchr(cursor, '=');
    if (!equals)
      continue;
    *equals = '\0';
    end = equals;
    while (end > cursor && (end[-1] == ' ' || end[-1] == '\t'))
      *--end = '\0';
    value = equals + 1;
    while (*value == ' ' || *value == '\t')
      ++value;
    end = value + strlen(value);
    while (end > value && (end[-1] == '\r' || end[-1] == '\n' ||
                           end[-1] == ' ' || end[-1] == '\t'))
      *--end = '\0';
    if (end - value >= 2 && value[0] == '"' && end[-1] == '"') {
      ++value;
      end[-1] = '\0';
    }
    if (!profile_key(lookup, sizeof(lookup), section, cursor))
      continue;
    if (!dictionary_get(values, lookup, NULL) &&
        dictionary_set(values, lookup, value) != 0) {
      dictionary_del(values);
      return NULL;
    }
  }
  if (shandalar_io_error(file)) {
    dictionary_del(values);
    return NULL;
  }
  return values;
}

static dictionary *profile_values(const char *filename)
{
  char path[1024];
  ProfileFile *profile;
  SDL_IOStream *file;
  profile_path(path, sizeof(path), filename);
  for (profile = g_profiles; profile; profile = profile->next)
    if (SDL_strcasecmp(profile->path, path) == 0)
      return profile->values;
  file = shandalar_open_file_io(path, "rb");
  if (!file)
    return NULL; /* Failed opens/fetches may succeed on a later lookup. */
  profile = (ProfileFile *)malloc(sizeof(*profile));
  if (!profile) {
    SDL_CloseIO(file);
    return NULL;
  }
  profile->values = load_profile(file);
  SDL_CloseIO(file);
  if (!profile->values) {
    free(profile);
    return NULL;
  }
  SDL_strlcpy(profile->path, path, sizeof(profile->path));
  profile->next = g_profiles;
  g_profiles = profile;
  return profile->values;
}

int GetPrivateProfileIntA(LPCSTR section, LPCSTR key, int default_value, LPCSTR filename)
{
  char value[64];
  char *end;
  long parsed;
  if (!GetPrivateProfileStringA(section, key, "", value, sizeof(value), filename) || !value[0])
    return default_value;
  parsed = strtol(value, &end, 10);
  return end == value ? default_value : (int)parsed;
}

DWORD GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR default_value,
                               LPSTR returned_string, DWORD size, LPCSTR filename)
{
  char lookup[1040];
  const char *value = default_value ? default_value : "";
  if (!returned_string || !size)
    return 0;
  if (profile_key(lookup, sizeof(lookup), section ? section : "", key ? key : ""))
    value = dictionary_get(profile_values(filename), lookup, value);
  SDL_strlcpy(returned_string, value, size);
  return (DWORD)strlen(returned_string);
}
