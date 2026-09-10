#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void assert(int condition, const char *file, int line, const char *format, ...)
{
  va_list args;
  if (condition)
    return;
  fprintf(stderr, "PIC decoder assertion at %s:%d: ", file ? file : "?", line);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputc('\n', stderr);
  abort();
}

void assert_noabort(int condition, const char *file, int line, const char *format, ...)
{
  va_list args;
  if (condition)
    return;
  fprintf(stderr, "PIC decoder warning at %s:%d: ", file ? file : "?", line);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputc('\n', stderr);
}
