#include "vga.h"
#include "io.h"

static unsigned short *vga_buffer = (unsigned short *)0xB8000;
static size_t cursor_row = 0, cursor_col = 0;
static unsigned char attribute;

static inline unsigned char make_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline unsigned short make_entry(char c, unsigned char color) {
    return (unsigned short)c | (unsigned short)color << 8;
}

void vga_update_cursor(void) {
    unsigned short pos = (unsigned short)(cursor_row * VGA_WIDTH + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void vga_clear(void) {
  for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buffer[i] = make_entry(' ', attribute);
    cursor_row = cursor_col = 0;
}

void vga_init(void) {
    attribute = make_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

static void scroll(void) {
    if (cursor_row < VGA_HEIGHT) return;
    for (size_t i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
        vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
    for (size_t i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
        vga_buffer[i] = make_entry(' ', attribute);
    cursor_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else {
        vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = make_entry(c, attribute);
        if (++cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    scroll();
    vga_update_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}

void vga_backspace(void) {
    if (cursor_col > 0) {
        cursor_col--;
    } else if (cursor_row > 0) {
        cursor_row--;
        cursor_col = VGA_WIDTH - 1;
    } else {
        return;
    }
    vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = make_entry(' ', attribute);
    vga_update_cursor();
}

int vga_get_cursor_row(void) {
    return (int)cursor_row;
}

int vga_get_cursor_col(void) {
    return (int)cursor_col;
}

void vga_set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col < 0) col = 0;
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    cursor_row = (size_t)row;
    cursor_col = (size_t)col;
    vga_update_cursor();
}

void vga_erase_at(int row, int col) {
    if (row < 0 || row >= VGA_HEIGHT) return;
    if (col < 0 || col >= VGA_WIDTH) return;
    vga_buffer[row * VGA_WIDTH + col] = make_entry(' ', attribute);
}

void vga_putchar_at(int row, int col, char c) {
    if (row < 0 || row >= VGA_HEIGHT) return;
    if (col < 0 || col >= VGA_WIDTH) return;
    vga_buffer[row * VGA_WIDTH + col] = make_entry(c, attribute);
}
