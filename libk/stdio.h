#ifndef LIBK_STDIO_H
#define LIBK_STDIO_H

#include <stddef.h>

int kprintf(const char *fmt, ...);
int ksnprintf(char *buf, size_t n, const char *fmt, ...);

#endif
