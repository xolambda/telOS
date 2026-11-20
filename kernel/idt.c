#include "idt.h"
#include "io.h"
#include "vga.h"
#include "timer.h"
#include "keyboard.h"

struct idt_entry {
    unsigned short base_lo;
    unsigned short sel;
    unsigned char always0;
    unsigned char flags;
    unsigned short base_hi;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void idt_load(unsigned int);
extern void irq0_handler(void);
extern void irq1_handler(void);

static void idt_set_gate(unsigned char n, unsigned int base,
                        unsigned short sel, unsigned char flags) {
    idt[n].base_lo = base & 0xFFFF;
    idt[n].base_hi = (base >> 16) & 0xFFFF;
    idt[n].sel = sel;
    idt[n].always0 = 0;
    idt[n].flags = flags;
}

void irq0_c_handler(void) {
    timer_handle_tick();
    outb(0x20, 0x20);
}

void irq1_c_handler(void) {
    unsigned char sc = inb(0x60);
    keyboard_handle_scancode(sc);
    outb(0x20, 0x20);
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned int)&idt;

    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC); outb(0xA1, 0xFF);

    for (int i = 0; i < 256; i++)
        idt_set_gate(i, 0, 0x08, 0x8E);

    idt_set_gate(0x20, (unsigned int)irq0_handler, 0x08, 0x8E);
    idt_set_gate(0x21, (unsigned int)irq1_handler, 0x08, 0x8E);

    idt_load((unsigned int)&idtp);
}
