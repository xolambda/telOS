#ifndef LIBK_STDIO_H
#define LIBK_STDIO_H

#include <stddef.h>

int kprintf(const char *fmt, ...);
int ksnprintf(char *buf, size_t n, const char *fmt, ...);
int kvsnprintf(char *buf, size_t n, const char *fmt, __builtin_va_list ap);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);

#endif
