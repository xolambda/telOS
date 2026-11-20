#include "keyboard.h"

static volatile char kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

static volatile struct kbd_event evt_head[KBD_BUF_SIZE];
static volatile int evt_head_idx = 0;
static volatile int evt_tail_idx = 0;

static int ext_prefix = 0;

static const char kbd_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

static void evt_push(struct kbd_event ev) {
    int next = (evt_head_idx + 1) % KBD_BUF_SIZE;
    if (next != evt_tail_idx) {
        evt_head[evt_head_idx] = ev;
        evt_head_idx = next;
    }
}

void keyboard_init(void) {
    kbd_head = kbd_tail = 0;
    evt_head_idx = evt_tail_idx = 0;
    ext_prefix = 0;
}

void keyboard_handle_scancode(unsigned char sc) {
    if (sc == 0xE0) {
        ext_prefix = 1;
        return;
    }
    if (sc == 0xE1) {
        ext_prefix = 0;
        return;
    }

    int release = (sc & 0x80) != 0;
    unsigned char key = sc & 0x7F;

    if (ext_prefix) {
        ext_prefix = 0;
        if (release) return;
        struct kbd_event ev = { KBD_EV_NONE, 0 };
        switch (key) {
        case 0x48: ev.type = KBD_EV_UP; break;
        case 0x50: ev.type = KBD_EV_DOWN; break;
        case 0x4B: ev.type = KBD_EV_LEFT; break;
        case 0x4D: ev.type = KBD_EV_RIGHT; break;
        case 0x47: ev.type = KBD_EV_HOME; break;
        case 0x4F: ev.type = KBD_EV_END; break;
        case 0x53: ev.type = KBD_EV_DELETE; break;
        default: return;
        }
        evt_push(ev);
        return;
    }

    if (release) return;

    char c = kbd_map[key];
    if (c == 27) {
        struct kbd_event ev = { KBD_EV_ESC, 0 };
        evt_push(ev);
        return;
    }
    if (!c) return;

    struct kbd_event ev = { KBD_EV_CHAR, c };
    evt_push(ev);

    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
}

struct kbd_event keyboard_get_event(void) {
    struct kbd_event ev = { KBD_EV_NONE, 0 };
    if (evt_head_idx == evt_tail_idx) return ev;
    ev = evt_head[evt_tail_idx];
    evt_tail_idx = (evt_tail_idx + 1) % KBD_BUF_SIZE;
    return ev;
}

int keyboard_getchar(void) {
    struct kbd_event ev = keyboard_get_event();
    if (ev.type == KBD_EV_CHAR) return (unsigned char)ev.ch;
    return -1;
}
