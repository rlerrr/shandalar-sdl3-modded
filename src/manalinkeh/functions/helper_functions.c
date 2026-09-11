#include "manalink.h"
#include <stddef.h>

int vscnprintf(char *buf, size_t size, const char *fmt, va_list args) {
	int i;
	ptrdiff_t ssize = (ptrdiff_t)size;

	i = vsnprintf(buf, size, fmt, args);

	return (i >= ssize) ? (ssize - 1) : i;
}

int scnprintf(char * buf, size_t size, const char * fmt, ...) {
	va_list args;
	ptrdiff_t ssize = (ptrdiff_t)size;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return (i >= ssize) ? (ssize - 1) : i;
}
