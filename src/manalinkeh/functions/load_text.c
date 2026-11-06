#include "manalink.h"
#include "platform/file_io.h"
#include "platform/asset_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

typedef struct
{
  const char *key;
  int num;
  const char **lines;
} LoadText;

_Static_assert(sizeof(LoadText) == (sizeof(void *) == 8 ? 24 : 12), "LoadText size");
_Static_assert(offsetof(LoadText, key) == 0 && offsetof(LoadText, num) == sizeof(void *) &&
               offsetof(LoadText, lines) == 2 * sizeof(void *), "LoadText fields");

/* Recovered private caches; retained raw buffers own the borrowed strings. */
static LoadText *loadtexts;
static int num_loadtexts;
static char last_loadtext_from_file[262];
static LoadText *loadtexts_from_file;
static int num_loadtexts_from_file;

/* ManalinkEh 2022: 0x2675d70; original functions/load_text.c:52. */
static void load_text_stash(const char *filename, LoadText **out_loadtexts, int *out_num_loadtexts)
{
  SDL_IOStream *resource_file;
  char *current_line_or_error;
  char *line_end_or_scan;
  char *text_line;
  LoadText *entry_or_reduced_stash;
  const char **entry_lines;
  char *next_line_or_escape;
  const char **missing_line_cursor;
  int *error_number;
  int line_count_or_entry_offset;
  char escaped_character;
  unsigned int file_length;
  int stored_lines_or_entry_index;
  LoadText *lt;
  int entry_capacity;
  int next_entry_count;
  int entry_index;
  char path[500];

  if (!shandalar_assets_join_path(path, sizeof(path), filename))
    fatal("Resource path is too long: %s", filename);
  resource_file = shandalar_open_file_io(path, "rb");
  if (resource_file == NULL)
  {
    error_number = &errno;
    current_line_or_error = strerror(*error_number);
    fatal("Couldn\'t open \"%s\": %s", path, current_line_or_error);
  }
  shandalar_io_seek(resource_file, 0, SEEK_END);
  file_length = (unsigned int)SDL_TellIO(resource_file);
  current_line_or_error = malloc(file_length + 2);
  if (current_line_or_error == NULL)
  {
    fatal("Couldn\'t malloc %d bytes for raw_file", file_length + 2);
  }
  shandalar_io_rewind(resource_file);
  file_length = shandalar_io_read(current_line_or_error, 1, file_length, resource_file);
  shandalar_io_close(resource_file);
  /* SDL streams expose CRLF bytes; resource keys and lines omit the CR. */
  unsigned int used = 0;
  for (unsigned int i = 0; i < file_length; ++i)
    if (current_line_or_error[i] != '\r')
      current_line_or_error[used++] = current_line_or_error[i];
  file_length = used;
  current_line_or_error[file_length] = '\n';
  current_line_or_error[file_length + 1] = '\0';
  lt = malloc(600 * sizeof(*lt));
  if (lt == NULL)
  {
    fatal("Couldn\'t malloc %d*%d entries for LoadText", 600, (int)sizeof(*lt));
  }
  line_end_or_scan = strchr(current_line_or_error, 10);
  entry_index = 0;
  entry_capacity = 600;
  next_entry_count = 1;
  while (line_end_or_scan != NULL)
  {
    line_count_or_entry_offset = entry_index * sizeof(*lt);
    *line_end_or_scan = '\0';
    next_line_or_escape = line_end_or_scan + 1;
    if ((*current_line_or_error == '@') && (current_line_or_error[1] != '\0'))
    {
      if (entry_capacity == entry_index)
      {
        entry_capacity = (int)((double)entry_capacity * 1.5);
        lt = realloc(lt, entry_capacity * sizeof(*lt));
        if (lt == NULL)
        {
          fatal("Couldn\'t realloc %d*%d entries for LoadText", entry_capacity, (int)sizeof(*lt));
        }
      }
      text_line = strchr(next_line_or_escape, 10);
      if (text_line == NULL)
      {
        line_count_or_entry_offset = entry_index * sizeof(*lt);
        break;
      }
      *text_line = '\0';
      text_line = text_line + 1;
      if ((unsigned char)(line_end_or_scan[1] - 0x31U) < 9)
      {
        entry_or_reduced_stash = lt + entry_index;
        entry_or_reduced_stash->key = current_line_or_error + 1;
        line_count_or_entry_offset = atoi(next_line_or_escape);
        entry_or_reduced_stash->num = line_count_or_entry_offset;
        entry_lines = malloc(line_count_or_entry_offset * sizeof(*entry_lines));
        entry_or_reduced_stash->lines = entry_lines;
        if (entry_lines == NULL)
        {
          fatal("Couldn\'t malloc %d lines for LoadText(\"@%s\")", line_count_or_entry_offset,
                current_line_or_error + 1);
        }
        line_end_or_scan = strchr(text_line, 10);
        if (0 < line_count_or_entry_offset)
        {
          bool entry_complete = false;
          stored_lines_or_entry_index = 0;
          while (line_end_or_scan != NULL)
          {
            *line_end_or_scan = '\0';
            current_line_or_error = line_end_or_scan + 1;
            line_end_or_scan = text_line;
            while (next_line_or_escape = strchr(line_end_or_scan, 0x5c),
                   next_line_or_escape != NULL)
            {
              escaped_character = next_line_or_escape[1];
              line_end_or_scan = next_line_or_escape + 1;
              bool unescape = true;
              if (escaped_character == 'n')
                escaped_character = '\n';
              else if (escaped_character == 't')
                escaped_character = '\t';
              else if (escaped_character != '\\')
                unescape = false;
              if (unescape)
              {
                *next_line_or_escape = escaped_character;
                escaped_character = next_line_or_escape[2];
                next_line_or_escape[1] = escaped_character;
                next_line_or_escape = line_end_or_scan;
                while (escaped_character != '\0')
                {
                  escaped_character = next_line_or_escape[2];
                  next_line_or_escape = next_line_or_escape + 1;
                  *next_line_or_escape = escaped_character;
                }
              }
            }
            entry_lines[stored_lines_or_entry_index] = text_line;
            stored_lines_or_entry_index = stored_lines_or_entry_index + 1;
            if (line_count_or_entry_offset == stored_lines_or_entry_index)
            {
              line_end_or_scan = strchr(current_line_or_error, 10);
              entry_index = next_entry_count;
              next_entry_count = next_entry_count + 1;
              entry_complete = true;
              break;
            }
            line_end_or_scan = strchr(current_line_or_error, 10);
            text_line = current_line_or_error;
          }
          if (entry_complete)
            continue;
          if (line_count_or_entry_offset <= stored_lines_or_entry_index)
          {
            stored_lines_or_entry_index = next_entry_count;
            line_count_or_entry_offset = next_entry_count * sizeof(*lt);
            next_entry_count = entry_index + 2;
            entry_index = stored_lines_or_entry_index;
            break;
          }
          missing_line_cursor = entry_lines + stored_lines_or_entry_index;
          do
          {
            *missing_line_cursor = "";
            missing_line_cursor = missing_line_cursor + 1;
          } while (entry_lines + line_count_or_entry_offset != missing_line_cursor);
          line_end_or_scan = strchr(text_line, 10);
        }
        entry_index = next_entry_count;
        next_entry_count = next_entry_count + 1;
        current_line_or_error = text_line;
      }
      else
      {
        line_end_or_scan = strchr(text_line, 10);
        current_line_or_error = text_line;
      }
      continue;
    }
    line_end_or_scan = strchr(next_line_or_escape, 10);
    current_line_or_error = next_line_or_escape;
  }
  line_count_or_entry_offset = entry_index * sizeof(*lt);
  if (entry_capacity == entry_index)
  {
    lt = realloc(lt, (int)((double)entry_capacity * 1.5) * sizeof(*lt));
    if (lt == NULL)
    {
      fatal("Couldn\'t realloc %d*%d entries for LoadText", (int)((double)entry_capacity * 1.5),
            (int)sizeof(*lt));
    }
  }
  LoadText *sentinel = (LoadText *)((char *)lt + line_count_or_entry_offset);
  sentinel->key = "";
  sentinel->num = 250;
  const char **empty_lines = malloc(250 * sizeof(*empty_lines));
  sentinel->lines = empty_lines;
  if (empty_lines != NULL)
  {
    const char **end_lines = empty_lines + 250;
    do
    {
      *empty_lines = "";
      empty_lines = empty_lines + 1;
    } while (empty_lines != end_lines);
    entry_or_reduced_stash = malloc(next_entry_count * sizeof(*lt));
    *out_loadtexts = entry_or_reduced_stash;
    if (entry_or_reduced_stash != NULL)
    {
      memcpy(entry_or_reduced_stash, lt, next_entry_count * sizeof(*lt));
      free(lt);
      *out_num_loadtexts = next_entry_count;
      return;
    }
    fatal("Couldn\'t malloc %d*%d entries for LoadText reduction", next_entry_count, (int)sizeof(*lt));
  }
  fatal("Couldn\'t malloc %d lines for LoadText(ai speculation)", 0xfa);
}

/* ManalinkEh 2022: 0x2676360; original functions/load_text.c:180. */
const char **load_text_nocopy(const char *header, int *num_lines)
{
  int cached_entry_count;
  LoadText *cached_stash;
  int entry_index;
  LoadText *entry;
  int fallback_entry_count;

  if (loadtexts == NULL)
  {
    load_text_stash("Text.res", &loadtexts, &num_loadtexts);
  }
  cached_stash = loadtexts;
  cached_entry_count = num_loadtexts;
  if (ai_is_speculating == 1)
  {
    if (num_lines != NULL)
    {
      *num_lines = 0;
    }
    return cached_stash[num_loadtexts + -1].lines;
  }
  entry_index = 0;
  fallback_entry_count = num_loadtexts;
  entry = loadtexts;
  if (0 < num_loadtexts)
  {
    do
    {
      if (strcmp(entry->key, header) == 0)
      {
        if (num_lines != NULL)
        {
          *num_lines = entry->num;
        }
        return entry->lines;
      }
      entry_index = entry_index + 1;
      entry = entry + 1;
    } while (entry_index != cached_entry_count);
  }
  if (num_lines != NULL)
  {
    *num_lines = 0;
    fallback_entry_count = num_loadtexts;
  }
  return cached_stash[fallback_entry_count + -1].lines;
}

/* ManalinkEh 2022: 0x2676450; original functions/load_text.c:216. */
/* The released fallback indexes this cache using num_loadtexts from the main cache. */
const char **load_text_nocopy_from_file(const char *filename, const char *header, int *num_lines)
{
  if (ai_is_speculating == 1)
  {
    if (num_lines)
      *num_lines = 0;
    if (!loadtexts)
      load_text_stash("Text.res", &loadtexts, &num_loadtexts);
    return loadtexts[num_loadtexts - 1].lines;
  }
  int filename_match = strncmp(last_loadtext_from_file, filename, 0x106);
  LoadText *cached_stash = loadtexts_from_file;
  if (filename_match != 0)
  {
    strncpy(last_loadtext_from_file, filename, 0x106);
    last_loadtext_from_file[0x105] = '\0';
    if (cached_stash)
      free(cached_stash);
    load_text_stash(filename, &loadtexts_from_file, &num_loadtexts_from_file);
  }
  cached_stash = loadtexts_from_file;
  int entry_count = num_loadtexts_from_file;
  for (int i = 0; i < entry_count; ++i)
  {
    LoadText *entry = cached_stash + i;
    if (strcmp(entry->key, header) == 0)
    {
      if (num_lines)
        *num_lines = entry->num;
      return entry->lines;
    }
  }
  if (num_lines)
    *num_lines = 0;
  return cached_stash[num_loadtexts - 1].lines;
}

/* ManalinkEh 2022: 0x26765b0; original functions/load_text.c:257. */
/* Preserve the released 250-row cap; the native declaration has only 249 rows. */
int load_text(const char *ignored, const char *header)
{
  const char **source_line;
  const char **source_lines;
  int i;
  char (*destination_line)[300];
  int num_lines;

  source_lines = ignored ? load_text_nocopy_from_file(ignored, header, &num_lines)
                         : load_text_nocopy(header, &num_lines);
  if (num_lines >= 0xfb)
    num_lines = 0xfa;
  if (num_lines > 0)
  {
    destination_line = text_lines;
    i = 0;
    do
    {
      source_line = source_lines + i;
      i = i + 1;
      snprintf(*destination_line, 300, "%s", *source_line);
      destination_line = destination_line + 1;
    } while (i < num_lines);
  }
  if (num_lines < 0xf9)
  {
    destination_line = text_lines + num_lines;
    do
    {
      (*destination_line)[0] = '\0';
      destination_line = destination_line + 1;
    } while (destination_line != (text_lines + 249));
  }
  return num_lines;
}

#ifndef SHANDALAR
/* Recovered October 2022 inline helpers; existing native callers contain the expansions. */
static char *destr_read_a_line(char **raw_file)
{
  char *line = *raw_file;
  char *newline = strchr(line, '\n');
  if (!newline)
    return NULL;
  *newline = '\0';
  *raw_file = newline + 1;
  return line;
}
#endif
