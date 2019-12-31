#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "str.h"

// by Pavel Šimerda

int vasprintf(char** strp, const char* fmt, va_list ap) {
  va_list ap1;
  size_t size;
  char* buffer;

  va_copy(ap1, ap);
  size = vsnprintf(NULL, 0, fmt, ap1) + 1;
  va_end(ap1);
  buffer = calloc(1, size);

  if (!buffer)
	return -1;

  *strp = buffer;

  return vsnprintf(buffer, size, fmt, ap);
}

int asprintf(char** strp, const char* fmt, ...) {
  int error;
  va_list ap;

  va_start(ap, fmt);
  error = vasprintf(strp, fmt, ap);
  va_end(ap);

  return error;
}

/// asprintf but ignore errors and return string (or null if error)
char* isprintf(const char* fmt, ...) {
  char* strp = NULL;

  va_list ap;

  va_start(ap, fmt);
  vasprintf(&strp, fmt, ap);
  va_end(ap);

  return strp;
}