#include "panic.h"
#include "stdio.h"
#include "serial.h"

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void panic_at(const char *msg, const char *file, int line) {
    __asm__ volatile ("cli");

    kprintf("\n*** telOS panic ***\n");
    kprintf("%s\n", msg);
    kprintf("at %s:%d\n", file, line);

    serial_puts("\n*** telOS panic ***\n");
    serial_puts(msg);
    serial_puts("\nat ");
    serial_puts(file);
    serial_puts(":");
    char buf[16];
    int n = 0;
    if (line == 0) {
        buf[n++] = '0';
    } else {
        char tmp[16];
        int m = 0;
        int l = line;
        if (l < 0) l = -l;
        while (l) { tmp[m++] = '0' + (l % 10); l /= 10; }
        while (m) buf[n++] = tmp[--m];
    }
    buf[n] = '\0';
    serial_puts(buf);
    serial_puts("\n");

    halt_forever();
}

void panic(const char *msg) {
    panic_at(msg, "?", 0);
}
