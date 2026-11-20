#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KBD_BUF_SIZE 256

void keyboard_init(void);
void keyboard_handle_scancode(unsigned char sc);
int keyboard_getchar(void);

#endif
