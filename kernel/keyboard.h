#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KBD_BUF_SIZE 256
#define KBD_HISTORY_SIZE 16
#define KBD_HISTORY_MAX 256

enum kbd_event_type {
    KBD_EV_NONE = 0,
    KBD_EV_CHAR,
    KBD_EV_UP,
    KBD_EV_DOWN,
    KBD_EV_LEFT,
    KBD_EV_RIGHT,
    KBD_EV_HOME,
    KBD_EV_END,
    KBD_EV_DELETE,
    KBD_EV_ESC,
};

struct kbd_event {
    enum kbd_event_type type;
    char ch;
};

void keyboard_init(void);
struct kbd_event keyboard_get_event(void);
int keyboard_getchar(void);
void keyboard_handle_scancode(unsigned char sc);

#endif
