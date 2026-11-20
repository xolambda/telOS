#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "timer.h"
#include "serial.h"
#include "stdio.h"
#include "string.h"
#include "panic.h"
#include "multiboot.h"
#include "paging.h"
#include "paging_asm.h"
#include "heap.h"
#include "syscall.h"
#include "usermode.h"
#include "task.h"
#include "ramfs.h"
#include "version.h"

#define TELOS_PROMPT "> "
#define LINE_MAX 256
#define BG_BUF_SIZE 1024

extern char __kernel_end;
extern char stack_top;

static struct multiboot_info *mb_info = 0;
static uint32_t mb_magic = 0;

static char history[KBD_HISTORY_SIZE][KBD_HISTORY_MAX];
static int  history_count = 0;

static char bg_buf[BG_BUF_SIZE];
static int  bg_head = 0;
static int  bg_tail = 0;

static void bg_push(char c) {
    int next = (bg_head + 1) % BG_BUF_SIZE;
    if (next != bg_tail) {
        bg_buf[bg_head] = c;
        bg_head = next;
    }
}

static char bg_pop(void) {
    if (bg_head == bg_tail) return 0;
    char c = bg_buf[bg_tail];
    bg_tail = (bg_tail + 1) % BG_BUF_SIZE;
    return c;
}

static void bg_flush(void) {
    char c;
    while ((c = bg_pop()) != 0) {
        vga_putchar(c);
        serial_putc(c);
    }
}

static void kernel_putchar(char c) {
    task_t *cur = task_current();
    if (cur && cur->is_detached && cur->state == TASK_STATE_RUNNING) {
        bg_push(c);
    } else {
        vga_putchar(c);
        serial_putc(c);
    }
}

static void shell_prompt(void) {
    vga_puts(TELOS_PROMPT);
}

static void history_push(const char *line) {
    if (line[0] == '\0') return;
    if (history_count > 0) {
        int last = (history_count - 1) % KBD_HISTORY_SIZE;
        if (strcmp(history[last], line) == 0) return;
    }
    int slot = history_count % KBD_HISTORY_SIZE;
    strncpy(history[slot], line, KBD_HISTORY_MAX - 1);
    history[slot][KBD_HISTORY_MAX - 1] = '\0';
    history_count++;
}

static void redraw_line(const char *buf, int len, int cursor) {
    int row = vga_get_cursor_row();
    int col_start = (int)strlen(TELOS_PROMPT);

    for (int i = 0; i < len; i++) {
        vga_putchar_at(row, col_start + i, buf[i]);
    }
    for (int i = len; i < LINE_MAX; i++) {
        int c = col_start + i;
        if (c >= VGA_WIDTH) break;
        vga_erase_at(row, c);
    }

    vga_set_cursor(row, col_start + cursor);
}

static void replace_line(const char *newbuf, int newlen, int *len, int *cursor, char *buf) {
    int row = vga_get_cursor_row();
    int col_start = (int)strlen(TELOS_PROMPT);
    for (int i = 0; i < *len; i++) {
        vga_erase_at(row, col_start + i);
    }
    for (int i = 0; i < newlen; i++) {
        buf[i] = newbuf[i];
        vga_putchar_at(row, col_start + i, newbuf[i]);
    }
    *len = newlen;
    *cursor = newlen;
    vga_set_cursor(row, col_start + *cursor);
}

static void do_help(void);
static void do_clear(void);
static void do_about(void);
static void do_uptime(void);
static void do_serialtest(void);
static void do_meminfo(void);
static void do_mmap(void);
static void do_pagetables(void);
static void do_pgstatus(void);
static void do_heapinfo(void);
static void do_heaptest(void);
static void do_heaptest2(void);
static void do_heapdump(void);
static void do_syscall(void);
static void do_usermode(void);
static void do_panic(void);
static void do_sysfetch(void);
static void do_spawn(void);
static void do_spawn_detached(void);
static void do_yield(void);
static void do_ps(void);
static void do_kill(const char *name);
static void do_hex(const char *arg);
static void do_sleep(const char *arg);
static void do_ls(void);
static void do_touch(const char *name);
static void do_rm(const char *name);
static void do_cat(const char *name);
static void do_write(const char *args);
static void do_spawn_file(const char *name);

static void handle_command(char *line) {
    int len = (int)strlen(line);

    if (len == 0) return;

    if (strcmp(line, "help") == 0) {
        do_help();
    } else if (strcmp(line, "clear") == 0) {
        do_clear();
    } else if (strcmp(line, "about") == 0) {
        do_about();
    } else if (strcmp(line, "uptime") == 0) {
        do_uptime();
    } else if (strcmp(line, "serialtest") == 0) {
        do_serialtest();
    } else if (strcmp(line, "meminfo") == 0) {
        do_meminfo();
    } else if (strcmp(line, "mmap") == 0) {
        do_mmap();
    } else if (strcmp(line, "pagetables") == 0) {
        do_pagetables();
    } else if (strcmp(line, "pgstatus") == 0) {
        do_pgstatus();
    } else if (strcmp(line, "heapinfo") == 0) {
        do_heapinfo();
    } else if (strcmp(line, "heaptest") == 0) {
        do_heaptest();
    } else if (strcmp(line, "heaptest2") == 0) {
        do_heaptest2();
    } else if (strcmp(line, "heapdump") == 0) {
        do_heapdump();
    } else if (strcmp(line, "syscall") == 0) {
        do_syscall();
    } else if (strcmp(line, "usermode") == 0) {
        do_usermode();
    } else if (strcmp(line, "spawn") == 0) {
        do_spawn();
    } else if (strcmp(line, "spawn_detached") == 0) {
        do_spawn_detached();
    } else if (strcmp(line, "yield") == 0) {
        do_yield();
    } else if (strcmp(line, "ps") == 0) {
        do_ps();
    } else if (strcmp(line, "panic") == 0) {
        do_panic();
    } else if (strcmp(line, "sysfetch") == 0) {
        do_sysfetch();
    } else if (strcmp(line, "ls") == 0) {
        do_ls();
    } else if (strncmp(line, "kill ", 5) == 0) {
        do_kill(line + 5);
    } else if (strncmp(line, "touch ", 6) == 0) {
        do_touch(line + 6);
    } else if (strncmp(line, "rm ", 3) == 0) {
        do_rm(line + 3);
    } else if (strncmp(line, "cat ", 4) == 0) {
        do_cat(line + 4);
    } else if (strncmp(line, "write ", 6) == 0) {
        do_write(line + 6);
    } else if (strncmp(line, "spawn_file ", 11) == 0) {
        do_spawn_file(line + 11);
    } else if (strncmp(line, "echo ", 5) == 0) {
        kprintf("%s\n", line + 5);
    } else if (strncmp(line, "hex ", 4) == 0) {
        do_hex(line + 4);
    } else if (strncmp(line, "sleep ", 6) == 0) {
        do_sleep(line + 6);
    } else {
        kprintf("unknown command: %s\n", line);
    }
}

static void shell_run(void) __attribute__((noreturn));
static void shell_run(void) {
    char line[LINE_MAX];
    int len = 0;
    int cursor = 0;
    int browse_index = -1;

    shell_prompt();

    for (;;) {
        bg_flush();

        struct kbd_event ev = keyboard_get_event();
        if (ev.type == KBD_EV_NONE) {
            __asm__ volatile ("hlt");
            task_yield();
            continue;
        }

        if (ev.type == KBD_EV_CHAR) {
            char c = ev.ch;

            if (c == '\n') {
                vga_putchar('\n');
                line[len] = '\0';
                history_push(line);
                handle_command(line);
                len = 0;
                cursor = 0;
                browse_index = -1;
                shell_prompt();
            } else if (c == '\b') {
                if (cursor > 0) {
                    for (int i = cursor - 1; i < len - 1; i++) {
                        line[i] = line[i + 1];
                    }
                    len--;
                    cursor--;
                    line[len] = '\0';
                    redraw_line(line, len, cursor);
                }
            } else if (c >= 32 && c < 127 && len < LINE_MAX - 1) {
                for (int i = len; i > cursor; i--) {
                    line[i] = line[i - 1];
                }
                line[cursor] = c;
                len++;
                cursor++;
                line[len] = '\0';
                redraw_line(line, len, cursor);
            }
        } else if (ev.type == KBD_EV_UP) {
            if (history_count > 0) {
                if (browse_index < 0) browse_index = history_count - 1;
                else if (browse_index > 0) browse_index--;
                int slot = browse_index % KBD_HISTORY_SIZE;
                replace_line(history[slot], (int)strlen(history[slot]),
                             &len, &cursor, line);
            }
        } else if (ev.type == KBD_EV_DOWN) {
            if (browse_index >= 0) {
                browse_index++;
                if (browse_index >= history_count) {
                    browse_index = -1;
                    replace_line("", 0, &len, &cursor, line);
                } else {
                    int slot = browse_index % KBD_HISTORY_SIZE;
                    replace_line(history[slot], (int)strlen(history[slot]),
                                 &len, &cursor, line);
                }
            }
        } else if (ev.type == KBD_EV_LEFT) {
            if (cursor > 0) {
                cursor--;
                vga_set_cursor(vga_get_cursor_row(),
                               (int)strlen(TELOS_PROMPT) + cursor);
            }
        } else if (ev.type == KBD_EV_RIGHT) {
            if (cursor < len) {
                cursor++;
                vga_set_cursor(vga_get_cursor_row(),
                               (int)strlen(TELOS_PROMPT) + cursor);
            }
        } else if (ev.type == KBD_EV_HOME) {
            cursor = 0;
            vga_set_cursor(vga_get_cursor_row(),
                           (int)strlen(TELOS_PROMPT));
        } else if (ev.type == KBD_EV_END) {
            cursor = len;
            vga_set_cursor(vga_get_cursor_row(),
                           (int)strlen(TELOS_PROMPT) + cursor);
        } else if (ev.type == KBD_EV_DELETE) {
            if (cursor < len) {
                for (int i = cursor; i < len - 1; i++) {
                    line[i] = line[i + 1];
                }
                len--;
                line[len] = '\0';
                redraw_line(line, len, cursor);
            }
        } else if (ev.type == KBD_EV_ESC) {
            vga_putchar('\n');
            len = 0;
            cursor = 0;
            browse_index = -1;
            shell_prompt();
        }

        task_yield();
    }
}

static void do_help(void) {
    kprintf("commands:\n");
    kprintf("  help           show this message\n");
    kprintf("  clear          clear the screen\n");
    kprintf("  echo <text>    print text back\n");
    kprintf("  hex  <number>  print decimal as hex\n");
    kprintf("  sleep <ms>     sleep for N milliseconds\n");
    kprintf("  uptime         print time since boot\n");
    kprintf("  serialtest     write test data to COM1\n");
    kprintf("  meminfo        multiboot memory summary\n");
    kprintf("  mmap           dump the memory map\n");
    kprintf("  pagetables     page directory + table info\n");
    kprintf("  pgstatus       paging status (CR0/CR3)\n");
    kprintf("  heapinfo       heap base / current / capacity\n");
    kprintf("  heaptest       allocate + verify a block\n");
    kprintf("  heaptest2      allocate, free, reuse, coalesce\n");
    kprintf("  heapdump       walk every heap block\n");
    kprintf("  syscall        trigger int 0x80 test\n");
    kprintf("  usermode       jump to ring 3 and back\n");
    kprintf("  spawn          create + wait for a kernel task\n");
    kprintf("  spawn_detached create a task, return to shell\n");
    kprintf("  spawn_file <f> run a file as a detached task\n");
    kprintf("  yield          yield to next ready task\n");
    kprintf("  ps             list running tasks\n");
    kprintf("  kill <name>    terminate a task by name\n");
    kprintf("  sysfetch       system info + ASCII art\n");
    kprintf("  panic          trigger a test panic\n");
    kprintf("  about          show version info\n");
    kprintf("\n");
    kprintf("filesystem:\n");
    kprintf("  ls             list files\n");
    kprintf("  touch <file>   create an empty file\n");
    kprintf("  rm <file>      delete a file\n");
    kprintf("  cat <file>     print file contents\n");
    kprintf("  write <f> <s>  write string to file\n");
    kprintf("\n  line editing: arrows, home/end, delete, up/down history\n");
}

static void do_clear(void) { vga_clear(); }

static void do_about(void) {
    char buf[64];
    sprintf(buf, "%s %s", TELOS_NAME, TELOS_VERSION);
    kprintf("%s\n", buf);
    kprintf("kernel:  telos.bin\n");
    kprintf("build:   -m32 -ffreestanding\n");
}

static void do_uptime(void) {
    kprintf("uptime: %u ms (%u ticks @ %u Hz)\n",
            timer_uptime_ms(), timer_ticks(), TIMER_HZ);
}

static void do_serialtest(void) {
    serial_puts("[serial test] hello from telOS\n");
    serial_puts("[serial test] hex: ");
    serial_puthex(0xdeadbeef);
    serial_puts("\n");
    kprintf("wrote to serial\n");
}

static void do_meminfo(void) {
    if (!mb_info) { kprintf("no multiboot info\n"); return; }

    kprintf("multiboot magic: 0x%x\n", mb_magic);

    if (mb_info->flags & MULTIBOOT_INFO_MEMORY) {
        kprintf("lower memory:   %u KB\n", mb_info->mem_lower);
        kprintf("upper memory:   %u KB\n", mb_info->mem_upper);
        kprintf("total usable:   %u KB\n", mb_info->mem_lower + mb_info->mem_upper);
    }

    if (mb_info->flags & MULTIBOOT_INFO_MMAP) {
        kprintf("mmap entries:   ~%u (at 0x%x)\n",
                mb_info->mmap_length / 24, mb_info->mmap_addr);
    }

    kprintf("kernel ends at: 0x%x\n", (unsigned int)(size_t)&__kernel_end);
}

static void do_mmap(void) {
    if (!mb_info || !(mb_info->flags & MULTIBOOT_INFO_MMAP)) {
        kprintf("no memory map\n"); return;
    }

    uint32_t addr = mb_info->mmap_addr;
    uint32_t end = addr + mb_info->mmap_length;
    int i = 0;
    kprintf("idx  base             length           type\n");

    while (addr < end) {
        struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)addr;
        const char *kind;
        switch (e->type) {
        case MULTIBOOT_MEMORY_AVAILABLE:        kind = "usable";     break;
        case MULTIBOOT_MEMORY_RESERVED:         kind = "reserved";   break;
        case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: kind = "acpi-recl";  break;
        case MULTIBOOT_MEMORY_NVS:              kind = "nvs";        break;
        case MULTIBOOT_MEMORY_BADRAM:           kind = "badram";     break;
        default:                                kind = "unknown";    break;
        }
        kprintf("%3d  0x%08x%08x  0x%08x%08x  %s\n",
                i,
                (uint32_t)(e->addr >> 32), (uint32_t)(e->addr & 0xFFFFFFFFu),
                (uint32_t)(e->len >> 32),  (uint32_t)(e->len & 0xFFFFFFFFu),
                kind);
        addr += e->size + 4;
        if (++i > 64) { kprintf("... (truncated)\n"); break; }
    }
}

static void do_pagetables(void) {
    kprintf("identity map size: 0x%x bytes (%u MiB)\n",
            IDENTITY_MAP_SIZE, IDENTITY_MAP_SIZE / (1024 * 1024));
    kprintf("page directory:    0x%x\n", paging_directory_phys());
    kprintf("kernel ends at:    0x%x\n", (unsigned int)(size_t)&__kernel_end);
    kprintf("self test:         %s\n", paging_self_test() ? "PASS" : "FAIL");
    for (uint32_t i = 0; i < PAGE_TABLES; i++) {
        uint32_t entry = ((page_dir_entry_t *)paging_directory_phys())[i];
        kprintf("  [%u] = 0x%x\n", i, entry);
    }
}

static void do_pgstatus(void) {
    uint32_t cr0 = paging_get_cr0();
    kprintf("CR0:  0x%x\n", cr0);
    kprintf("PG:   %s\n", (cr0 & 0x80000000u) ? "on" : "off");
    kprintf("PE:   %s\n", (cr0 & 0x1u) ? "on" : "off");
    kprintf("CR3:  0x%x\n", paging_get_cr3());
}

static void do_heapinfo(void) {
    kprintf("heap base:     0x%x\n", (uint32_t)(uintptr_t)heap_start());
    kprintf("heap current:  0x%x\n", (uint32_t)(uintptr_t)heap_current());
    kprintf("used:          %u bytes\n", (uint32_t)heap_used());
    kprintf("capacity:      %u bytes\n", (uint32_t)heap_capacity());
}

static void do_heaptest(void) {
    const unsigned int sizes[] = { 16, 32, 64, 128, 1000, 4096 };
    const unsigned int count = sizeof(sizes) / sizeof(sizes[0]);
    unsigned char *blocks[6];
    unsigned int i;

    kprintf("allocating %u blocks...\n", count);
    for (i = 0; i < count; i++) {
        blocks[i] = (unsigned char *)kmalloc(sizes[i]);
        if (!blocks[i]) { kprintf("  FAIL: kmalloc(%u) returned NULL\n", sizes[i]); return; }
        if ((uintptr_t)blocks[i] & (HEAP_ALIGN - 1)) {
            kprintf("  FAIL: misaligned pointer 0x%x\n", (uint32_t)(uintptr_t)blocks[i]);
            return;
        }
        kprintf("  [%u] %5u bytes -> 0x%x\n", i, sizes[i], (uint32_t)(uintptr_t)blocks[i]);
    }
    for (i = 0; i < count; i++) {
        for (unsigned int j = 0; j < sizes[i]; j++)
            blocks[i][j] = (unsigned char)(i * 31 + j);
    }
    for (i = 0; i < count; i++) {
        for (unsigned int j = 0; j < sizes[i]; j++) {
            if (blocks[i][j] != (unsigned char)(i * 31 + j)) {
                kprintf("  FAIL: corruption in block %u at byte %u\n", i, j);
                return;
            }
        }
    }
    kprintf("all %u blocks verified. used: %u bytes\n",
            count, (uint32_t)heap_used());
}

static void do_heaptest2(void) {
    unsigned char *a = (unsigned char *)kmalloc(256);
    unsigned char *b = (unsigned char *)kmalloc(512);
    unsigned char *c = (unsigned char *)kmalloc(256);
    if (!a || !b || !c) { kprintf("  FAIL: allocation\n"); return; }
    kprintf("  A=0x%x B=0x%x C=0x%x\n",
            (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b, (uint32_t)(uintptr_t)c);
    for (unsigned int i = 0; i < 256; i++) a[i] = (unsigned char)i;
    for (unsigned int i = 0; i < 512; i++) b[i] = (unsigned char)(i * 3);
    for (unsigned int i = 0; i < 256; i++) c[i] = (unsigned char)(i * 7);
    kfree(b);
    unsigned char *d = (unsigned char *)kmalloc(512);
    if (!d) { kprintf("  FAIL: D allocation\n"); return; }
    kprintf("  D=0x%x\n", (uint32_t)(uintptr_t)d);
    for (unsigned int i = 0; i < 512; i++) d[i] = (unsigned char)(i * 5);
    for (unsigned int i = 0; i < 256; i++)
        if (a[i] != (unsigned char)i) { kprintf("  FAIL: A corrupted\n"); return; }
    for (unsigned int i = 0; i < 256; i++)
        if (c[i] != (unsigned char)(i * 7)) { kprintf("  FAIL: C corrupted\n"); return; }
    kfree(a); kfree(c); kfree(d);
    unsigned char *big = (unsigned char *)kmalloc(1024);
    if (!big) { kprintf("  FAIL: big allocation\n"); return; }
    kprintf("  big=0x%x\n", (uint32_t)(uintptr_t)big);
    for (unsigned int i = 0; i < 1024; i++) big[i] = (unsigned char)(i ^ 0xAA);
    for (unsigned int i = 0; i < 1024; i++)
        if (big[i] != (unsigned char)(i ^ 0xAA)) { kprintf("  FAIL: big corrupted\n"); return; }
    kfree(big);
    kprintf("heaptest2: PASS\nused after test: %u bytes\n", (uint32_t)heap_used());
}

static void do_heapdump(void) { heap_dump(); }

static void do_syscall(void) {
    kprintf("triggering int 0x80 with SYS_WRITE...\n");
    uint32_t r = syscall_test();
    kprintf("returned: %u\n", r);
}

static void do_usermode(void) {
    kprintf("entering user mode...\n");
    usermode_run();
    kprintf("returned from user mode\n");
}

static void spawned_task(void) {
    for (int i = 0; i < 3; i++) {
        kprintf("[spawned] iteration %d\n", i);
        timer_sleep_ms(500);
    }
    kprintf("[spawned] done\n");
}

static void detached_task(void) {
    for (int i = 0; i < 5; i++) {
        kprintf("[detached] iteration %d\n", i);
        timer_sleep_ms(300);
    }
    kprintf("[detached] done\n");
}

static void do_spawn(void) {
    kprintf("creating task...\n");
    task_t *t = ktask_create(spawned_task, "spawned");
    if (!t) { kprintf("failed to create task\n"); return; }
    kprintf("yielding to new task...\n");
    task_yield();
    task_wait(t);
    kprintf("returned to shell\n");
}

static void do_spawn_detached(void) {
    kprintf("creating detached task...\n");
    task_t *t = ktask_spawn_detached(detached_task, "detached");
    if (!t) { kprintf("failed to create task\n"); return; }
    kprintf("task created, running in background\n");
    task_yield();
}

static void do_yield(void) {
    kprintf("yielding...\n");
    task_yield();
    kprintf("resumed\n");
}

static void ps_print_one(task_t *t) {
    const char *state;
    switch (t->state) {
    case TASK_STATE_RUNNING: state = "running"; break;
    case TASK_STATE_READY:   state = "ready";   break;
    case TASK_STATE_DEAD:    state = "dead";    break;
    default:                 state = "?";       break;
    }
    kprintf("  %2d  %-16s %-8s esp=0x%x\n", t->id, t->name, state, t->esp);
}

static void do_ps(void) {
    kprintf("id  name             state    esp\n");
    task_iterate(ps_print_one);
}

static void do_kill(const char *name) {
    task_t *t = task_find_by_name(name);
    if (!t) {
        kprintf("no such task: %s\n", name);
        return;
    }
    if (task_kill(t)) {
        kprintf("killed %s\n", name);
    } else {
        kprintf("failed to kill %s\n", name);
    }
}

static void do_ls(void) {
    int n = ramfs_count();
    if (n == 0) {
        kprintf("(empty)\n");
        return;
    }
    kprintf("name                             size\n");
    for (int i = 0; i < n; i++) {
        ramfs_file_t *f = ramfs_get(i);
        if (f) {
            kprintf("  %-30s %u\n", f->name, (uint32_t)f->size);
        }
    }
}

static void do_touch(const char *name) {
    if (ramfs_create(name) != 0) {
        kprintf("failed to create %s\n", name);
        return;
    }
    kprintf("created %s\n", name);
}

static void do_rm(const char *name) {
    if (ramfs_remove(name) != 0) {
        kprintf("no such file: %s\n", name);
        return;
    }
    kprintf("removed %s\n", name);
}

static void do_cat(const char *name) {
    int size = ramfs_size(name);
    if (size < 0) {
        kprintf("no such file: %s\n", name);
        return;
    }
    if (size == 0) {
        kprintf("(empty)\n");
        return;
    }
    char *buf = (char *)kmalloc(size + 1);
    if (!buf) { kprintf("oom\n"); return; }
    ramfs_read(name, buf, size);
    buf[size] = '\0';
    kprintf("%s", buf);
    if (buf[size - 1] != '\n') kprintf("\n");
    kfree(buf);
}

static void do_write(const char *args) {
    const char *name = args;
    while (*args && *args != ' ') args++;
    if (*args != ' ') {
        kprintf("usage: write <file> <text>\n");
        return;
    }
    int name_len = (int)(args - name);
    char file[RAMFS_MAX_NAME];
    if (name_len >= RAMFS_MAX_NAME) name_len = RAMFS_MAX_NAME - 1;
    memcpy(file, name, name_len);
    file[name_len] = '\0';

    const char *text = args + 1;

    if (!ramfs_exists(file)) {
        if (ramfs_create(file) != 0) {
            kprintf("failed to create %s\n", file);
            return;
        }
    }

    int n = ramfs_write(file, text, strlen(text));
    if (n < 0) {
        kprintf("write failed\n");
        return;
    }
    kprintf("wrote %d bytes to %s\n", n, file);
}

static void do_spawn_file(const char *name) {
    int size = ramfs_size(name);
    if (size < 0) {
        kprintf("no such file: %s\n", name);
        return;
    }
    kprintf("loading %s (%d bytes) as task\n", name, size);

    char *buf = (char *)kmalloc(size + 1);
    if (!buf) { kprintf("oom\n"); return; }
    ramfs_read(name, buf, size);
    buf[size] = '\0';

    kprintf("contents:\n---\n");
    kprintf("%s", buf);
    if (size > 0 && buf[size - 1] != '\n') kprintf("\n");
    kprintf("---\n");

    kfree(buf);

    task_t *t = ktask_spawn_detached(detached_task, name);
    if (!t) {
        kprintf("failed to spawn task\n");
        return;
    }
    kprintf("task %s created (runs the demo task)\n", name);
    task_yield();
}

static void do_panic(void) { panic("panic triggered by shell"); }

static void do_sysfetch(void) {
    static const char *art[] = {
        "._____. ._____.",
        "| ._. | | ._. |",
        "| !_| |_|_|_! |",
        "!___| |_______!",
        ".___|_|_| |___.",
        "| ._____| |_. |",
        "| !_! | | !_! |",
        "!_____! !_____!",
    };
    int art_lines = 8;

    uint32_t ms = timer_uptime_ms();
    uint32_t secs = ms / 1000;
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;

    char uptime_buf[32];
    snprintf(uptime_buf, sizeof(uptime_buf), "%uh %um %us",
             hours, mins % 60, secs % 60);

    uint32_t heap_cap = (uint32_t)heap_capacity();
    uint32_t heap_used_b = (uint32_t)heap_used();
    uint32_t heap_pct = heap_cap ? (heap_used_b * 100) / heap_cap : 0;

    char heap_buf[64];
    snprintf(heap_buf, sizeof(heap_buf), "%u KiB / %u KiB (%u%%)",
             heap_used_b / 1024, heap_cap / 1024, heap_pct);

    char mem_buf[64];
    if (mb_info && (mb_info->flags & MULTIBOOT_INFO_MEMORY)) {
        snprintf(mem_buf, sizeof(mem_buf), "%u MiB",
                 (mb_info->mem_lower + mb_info->mem_upper) / 1024);
    } else {
        snprintf(mem_buf, sizeof(mem_buf), "unknown");
    }

    char fs_buf[64];
    snprintf(fs_buf, sizeof(fs_buf), "%d files", ramfs_count());

    const char *labels[7] = {
        "OS", "Uptime", "Memory", "Heap", "Tasks", "FS", "Privilege"
    };

    char values[7][80];
    snprintf(values[0], sizeof(values[0]), "%s %s", TELOS_NAME, TELOS_VERSION);
    snprintf(values[1], sizeof(values[1]), "%s", uptime_buf);
    snprintf(values[2], sizeof(values[2]), "%s", mem_buf);
    snprintf(values[3], sizeof(values[3]), "%s", heap_buf);
    snprintf(values[4], sizeof(values[4]), "%d", task_count());
    snprintf(values[5], sizeof(values[5]), "%s", fs_buf);
    snprintf(values[6], sizeof(values[6]), "ring 0 / ring 3");

    int info_lines = 7;
    int info_offset = (art_lines - info_lines) / 2;

    for (int i = 0; i < art_lines; i++) {
        vga_puts(art[i]);
        vga_puts("  ");

        int info_i = i - info_offset;
        if (info_i >= 0 && info_i < info_lines) {
            vga_puts(labels[info_i]);
            vga_puts(": ");
            vga_puts(values[info_i]);
        }

        if (i < art_lines - 1) vga_putchar('\n');
    }

    vga_putchar('\n');
}

static void do_hex(const char *arg) {
    unsigned int v = 0;
    int any = 0;
    while (*arg == ' ') arg++;
    while (*arg >= '0' && *arg <= '9') {
        v = v * 10 + (unsigned)(*arg - '0');
        arg++;
        any = 1;
    }
    if (!any) { kprintf("usage: hex <decimal>\n"); return; }
    kprintf("0x%x\n", v);
}

static void do_sleep(const char *arg) {
    unsigned int ms = 0;
    int any = 0;
    while (*arg == ' ') arg++;
    while (*arg >= '0' && *arg <= '9') {
        ms = ms * 10 + (unsigned)(*arg - '0');
        arg++;
        any = 1;
    }
    if (!any || ms == 0 || ms > 60000) {
        kprintf("usage: sleep <1..60000 ms>\n");
        return;
    }
    kprintf("sleeping %u ms...\n", ms);
    timer_sleep_ms(ms);
    kprintf("done. uptime: %u ms\n", timer_uptime_ms());
}

void kernel_main(uint32_t magic, uint32_t info_addr) {
    mb_magic = magic;
    mb_info = (struct multiboot_info *)info_addr;

    serial_init();
    vga_init();
    gdt_init();
    idt_init();
    keyboard_init();
    timer_init();
    paging_init();

    kprintf("%s %s\n", TELOS_NAME, TELOS_VERSION);
    kprintf("._____. ._____.\n");
    kprintf("| ._. | | ._. |\n");
    kprintf("| !_| |_|_|_! |\n");
    kprintf("!___| |_______!\n");
    kprintf(".___|_|_| |___.\n");
    kprintf("| ._____| |_. |\n");
    kprintf("| !_! | | !_! |\n");
    kprintf("!_____! !_____!\n\n");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        kprintf("warning: bad multiboot magic 0x%x\n\n", magic);
    } else {
        kprintf("multiboot OK, info at 0x%x\n", info_addr);
    }

    paging_enable_now();
    if (!paging_enabled()) panic("PG is clear");

    kprintf("paging ON, dir at 0x%x\n", paging_directory_phys());
    heap_init();
    syscall_init();
    gdt_set_kernel_stack((uint32_t)&stack_top);
    task_init();
    ramfs_init();

    stdio_set_putchar_hook(kernel_putchar);

    kprintf("heap: base=0x%x cap=%u bytes\n\n",
            (uint32_t)(uintptr_t)heap_start(),
            (uint32_t)heap_capacity());

    kprintf("Welcome to %s. Type 'help'.\n\n", TELOS_NAME);

    shell_run();
}
