#include "stdio.h"
#include "string.h"
#include "ctype.h"
#include "vga.h"
#include "serial.h"

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_arg(v, t)   __builtin_va_arg(v, t)
#define va_end(v)      __builtin_va_end(v)

typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
} sink_t;

static void (*putchar_hook)(char c) = 0;

void stdio_set_putchar_hook(void (*fn)(char c)) {
    putchar_hook = fn;
}

static void sink_putc(sink_t *s, char c) {
    if (s->buf) {
        if (s->len + 1 < s->cap) s->buf[s->len] = c;
    } else {
        if (putchar_hook) {
            putchar_hook(c);
        } else {
            vga_putchar(c);
            serial_putc(c);
        }
    }
    s->len++;
}

static void emit_uint(sink_t *s, unsigned int v, unsigned int base,
                     int width, char pad, int upper) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[32];
    int n = 0;

    if (v == 0) {
        tmp[n++] = '0';
    } else {
        while (v) {
            tmp[n++] = digits[v % base];
            v /= base;
        }
    }

    while (n < width) tmp[n++] = pad;
    while (n--) sink_putc(s, tmp[n]);
}

static void emit_int(sink_t *s, int v, int width, char pad) {
    if (v < 0) {
        sink_putc(s, '-');
        emit_uint(s, (unsigned int)(-(long)v), 10, width ? width - 1 : 0, '0', 0);
    } else {
        emit_uint(s, (unsigned int)v, 10, width, pad, 0);
    }
}

static int do_format(sink_t *s, const char *fmt, va_list ap) {
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            sink_putc(s, *p);
            continue;
        }
        p++;

        int left = 0, zero = 0;
        while (*p == '-' || *p == '0') {
            if (*p == '-') left = 1;
            else zero = 1;
            p++;
        }

        int width = 0;
        while (isdigit(*p)) {
            width = width * 10 + (*p - '0');
            p++;
        }

        char pad = zero ? '0' : ' ';

        switch (*p) {
        case 'd': case 'i':
            emit_int(s, va_arg(ap, int), width, pad);
            break;
        case 'u':
            emit_uint(s, va_arg(ap, unsigned int), 10, width, pad, 0);
            break;
        case 'x':
            emit_uint(s, va_arg(ap, unsigned int), 16, width, pad, 0);
            break;
        case 'X':
            emit_uint(s, va_arg(ap, unsigned int), 16, width, pad, 1);
            break;
        case 'p': {
            unsigned int v = (unsigned int)(size_t)va_arg(ap, void *);
            sink_putc(s, '0');
            sink_putc(s, 'x');
            emit_uint(s, v, 16, 8, '0', 0);
            break;
        }
        case 'c':
            sink_putc(s, (char)va_arg(ap, int));
            break;
        case 's': {
            const char *str = va_arg(ap, const char *);
            if (!str) str = "(null)";
            if (left) {
                int n = 0;
                for (; *str; str++, n++) sink_putc(s, *str);
                for (; n < width; n++) sink_putc(s, ' ');
            } else {
                int n = (int)strlen(str);
                for (int i = n; i < width; i++) sink_putc(s, ' ');
                for (; *str; str++) sink_putc(s, *str);
            }
            break;
        }
        case '%':
            sink_putc(s, '%');
            break;
        case '\0':
            sink_putc(s, '%');
            return (int)s->len;
        default:
            sink_putc(s, '%');
            sink_putc(s, *p);
            break;
        }
    }
    return (int)s->len;
}

int kprintf(const char *fmt, ...) {
    sink_t s = { .buf = 0, .cap = 0, .len = 0 };
    va_list ap;
    va_start(ap, fmt);
    do_format(&s, fmt, ap);
    va_end(ap);
    return (int)s.len;
}

int kvsnprintf(char *buf, size_t n, const char *fmt, __builtin_va_list ap) {
    sink_t s = { .buf = buf, .cap = n, .len = 0 };
    do_format(&s, fmt, ap);
    if (n > 0) buf[(s.len < n) ? s.len : n - 1] = '\0';
    return (int)s.len;
}

int ksnprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf(buf, (size_t)-1, fmt, ap);
    va_end(ap);
    return r;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}
