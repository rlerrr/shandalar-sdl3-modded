#include "platform/file_io.h"
#include <SDL3/SDL.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

size_t shandalar_io_read(void *ptr, size_t size, size_t count, SDL_IOStream *io)
{
  if (!size || count > SIZE_MAX / size) return 0;
  return SDL_ReadIO(io, ptr, size * count) / size;
}

size_t shandalar_io_write(const void *ptr, size_t size, size_t count, SDL_IOStream *io)
{
  if (!size || count > SIZE_MAX / size) return 0;
  return SDL_WriteIO(io, ptr, size * count) / size;
}

int shandalar_io_seek(SDL_IOStream *io, Sint64 offset, int whence)
{
  SDL_IOWhence origin;
  switch (whence) {
  case SEEK_SET: origin = SDL_IO_SEEK_SET; break;
  case SEEK_CUR: origin = SDL_IO_SEEK_CUR; break;
  case SEEK_END: origin = SDL_IO_SEEK_END; break;
  default: return -1;
  }
  return SDL_SeekIO(io, offset, origin) < 0 ? -1 : 0;
}

int shandalar_io_close(SDL_IOStream *io) { return SDL_CloseIO(io) ? 0 : -1; }
int shandalar_io_flush(SDL_IOStream *io) { return SDL_FlushIO(io) ? 0 : -1; }
void shandalar_io_rewind(SDL_IOStream *io) { SDL_SeekIO(io, 0, SDL_IO_SEEK_SET); }
int shandalar_io_eof(SDL_IOStream *io) { return SDL_GetIOStatus(io) == SDL_IO_STATUS_EOF; }
int shandalar_io_error(SDL_IOStream *io) { return SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR; }

int shandalar_io_getc(SDL_IOStream *io)
{
  unsigned char c;
  return SDL_ReadIO(io, &c, 1) == 1 ? c : EOF;
}

int shandalar_io_text_getc(SDL_IOStream *io)
{
  int c = shandalar_io_getc(io);
  if (c == '\r') {
    Sint64 position = SDL_TellIO(io);
    int next = shandalar_io_getc(io);
    if (next == '\n') return '\n';
    SDL_SeekIO(io, position, SDL_IO_SEEK_SET);
  }
  return c;
}

char *shandalar_io_gets(char *str, int count, SDL_IOStream *io)
{
  int used = 0, c;
  if (!str || !io || count <= 0) return NULL;
  while (used < count - 1 && (c = shandalar_io_text_getc(io)) != EOF) {
    str[used++] = (char)c;
    if (c == '\n') break;
  }
  str[used] = 0;
  return used || count == 1 ? str : NULL;
}

int shandalar_io_vprintf(SDL_IOStream *io, const char *format, va_list args)
{
  char *text = NULL;
  int length = SDL_vasprintf(&text, format, args);
  if (length >= 0 && SDL_WriteIO(io, text, (size_t)length) != (size_t)length)
    length = -1;
  SDL_free(text);
  return length;
}

int shandalar_io_printf(SDL_IOStream *io, const char *format, ...)
{
  va_list args;
  int result;
  va_start(args, format);
  result = shandalar_io_vprintf(io, format, args);
  va_end(args);
  return result;
}

/* Consume whitespace without consuming the first byte of the next token. */
static void skip_space(SDL_IOStream *io)
{
  Sint64 position;
  int c;
  do {
    position = SDL_TellIO(io);
    c = shandalar_io_text_getc(io);
  } while (c != EOF && isspace((unsigned char)c));
  if (c != EOF) SDL_SeekIO(io, position, SDL_IO_SEEK_SET);
}

int shandalar_io_scanf(SDL_IOStream *io, const char *format, ...)
{
  va_list args;
  int assigned = 0, converted = 0, input_failure = 0;
  va_start(args, format);
  while (*format) {
    int c, suppress, width, is_long, matched = 0;
    Sint64 position;
    char conversion;
    if (isspace((unsigned char)*format)) {
      skip_space(io);
      ++format;
      continue;
    }
    if (*format != '%' || format[1] == '%') {
      if (*format == '%') ++format;
      position = SDL_TellIO(io);
      c = shandalar_io_text_getc(io);
      if (c != (unsigned char)*format++) {
        if (c != EOF) SDL_SeekIO(io, position, SDL_IO_SEEK_SET);
        else input_failure = 1;
        break;
      }
      continue;
    }
    ++format;
    suppress = *format == '*';
    if (suppress) ++format;
    width = 0;
    while (isdigit((unsigned char)*format)) {
      if (width > INT_MAX / 10 - 10) { width = INT_MAX; }
      else width = width * 10 + (*format - '0');
      ++format;
    }
    is_long = *format == 'l';
    if (is_long) ++format;
    conversion = *format++;
    if (conversion == 'd' || conversion == 'u') {
      char token[64];
      int used = 0;
      skip_space(io);
      if (!width || width >= (int)sizeof(token)) width = sizeof(token) - 1;
      while (used < width) {
        position = SDL_TellIO(io);
        c = shandalar_io_text_getc(io);
        if (c == EOF) { input_failure = 1; break; }
        if (!isdigit((unsigned char)c) && !(used == 0 && (c == '+' || c == '-'))) {
          SDL_SeekIO(io, position, SDL_IO_SEEK_SET);
          break;
        }
        token[used++] = (char)c;
      }
      token[used] = 0;
      matched = used > 0 && isdigit((unsigned char)token[used - 1]);
      if (matched && !suppress) {
        if (conversion == 'd') {
          long value = strtol(token, NULL, 10);
          if (is_long) *va_arg(args, long *) = value;
          else *va_arg(args, int *) = (int)value;
        } else {
          unsigned long value = strtoul(token, NULL, 10);
          if (is_long) *va_arg(args, unsigned long *) = value;
          else *va_arg(args, unsigned int *) = (unsigned int)value;
        }
      }
    } else if (conversion == 's' || conversion == 'c' || conversion == '[') {
      unsigned char set[256] = {0};
      int negate = 0;
      char *dest = suppress ? NULL : va_arg(args, char *);
      if (is_long) { SDL_SetError("Unsupported wide stream conversion"); break; }
      if (conversion == 's') skip_space(io);
      if (conversion == '[') {
        if (*format == '^') { negate = 1; ++format; }
        if (*format == ']') set[(unsigned char)*format++] = 1;
        while (*format && *format != ']') {
          unsigned char first = (unsigned char)*format++;
          if (*format == '-' && format[1] && format[1] != ']') {
            unsigned char last = (unsigned char)format[1];
            unsigned int i;
            format += 2;
            for (i = first; i <= last; ++i) set[i] = 1;
          } else set[first] = 1;
        }
        if (*format != ']') { SDL_SetError("Unterminated stream scanset"); break; }
        ++format;
      }
      if (!width) width = conversion == 'c' ? 1 : INT_MAX;
      while (matched < width) {
        position = SDL_TellIO(io);
        c = shandalar_io_text_getc(io);
        if (c == EOF) { input_failure = 1; break; }
        if ((conversion == 's' && isspace((unsigned char)c)) ||
            (conversion == '[' && (set[(unsigned char)c] != 0) == negate)) {
          SDL_SeekIO(io, position, SDL_IO_SEEK_SET);
          break;
        }
        if (dest) dest[matched] = (char)c;
        ++matched;
      }
      if (dest && conversion != 'c') dest[matched] = 0;
      if (conversion == 'c' && matched != width) matched = 0;
    } else {
      SDL_SetError("Unsupported stream conversion: %c", conversion);
      break;
    }
    if (!matched) break;
    ++converted;
    if (!suppress) ++assigned;
  }
  va_end(args);
  return input_failure && !converted ? EOF : assigned;
}

int shandalar_file_exists(const char *path)
{
  SDL_IOStream *io = shandalar_open_file_io(path, "rb");
  if (!io) return 0;
  SDL_CloseIO(io);
  return 1;
}
