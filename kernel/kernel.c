#include "usermode.h"
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

#define TELOS_PROMPT "> "
#define LINE_MAX 256

extern char __kernel_end;
extern char stack_top;

static struct multiboot_info *mb_info = 0;
static uint32_t mb_magic = 0;

static void shell_prompt(void) {
    vga_puts(TELOS_PROMPT);
}

static void cmd_help(void) {
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
    kprintf("  panic          trigger a test panic\n");
    kprintf("  about          show version info\n");
    kprintf("  usermode       jump to ring 3 and back\n");
}

static void cmd_hex(const char *arg) {
    unsigned int v = 0;
    int any = 0;
    while (*arg == ' ') arg++;
    while (*arg >= '0' && *arg <= '9') {
        v = v * 10 + (unsigned)(*arg - '0');
        arg++;
        any = 1;
    }
    if (!any) {
        kprintf("usage: hex <decimal>\n");
        return;
    }
    kprintf("0x%x\n", v);
}

static void cmd_about(void) {
    kprintf("telOS v0.7a\n");
    kprintf("kernel:  telos.bin\n");
    kprintf("build:   -m32 -ffreestanding\n");
}

static void cmd_sleep(const char *arg) {
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

static void cmd_uptime(void) {
    uint32_t ms = timer_uptime_ms();
    kprintf("uptime: %u ms (%u ticks @ %u Hz)\n",
            ms, timer_ticks(), TIMER_HZ);
}

static void cmd_serialtest(void) {
    serial_puts("[serial test] hello from telOS\n");
    serial_puts("[serial test] hex: ");
    serial_puthex(0xdeadbeef);
    serial_puts("\n");
    kprintf("wrote to serial\n");
}

static void cmd_meminfo(void) {
    if (!mb_info) {
        kprintf("no multiboot info\n");
        return;
    }

    kprintf("multiboot magic: 0x%x\n", mb_magic);

    if (mb_info->flags & MULTIBOOT_INFO_MEMORY) {
        uint32_t lower_kb = mb_info->mem_lower;
        uint32_t upper_kb = mb_info->mem_upper;
        kprintf("lower memory:   %u KB (%u MB)\n",
                lower_kb, lower_kb / 1024);
        kprintf("upper memory:   %u KB (%u MB)\n",
                upper_kb, upper_kb / 1024);
        kprintf("total usable:   %u KB\n", lower_kb + upper_kb);
    } else {
        kprintf("mem_lower/mem_upper not provided\n");
    }

    if (mb_info->flags & MULTIBOOT_INFO_MMAP) {
        kprintf("mmap entries:   ~%u (at 0x%x)\n",
                mb_info->mmap_length / 24, mb_info->mmap_addr);
    } else {
        kprintf("no memory map provided\n");
    }

    kprintf("kernel ends at: 0x%x\n", (unsigned int)(size_t)&__kernel_end);
}

static void cmd_mmap(void) {
    if (!mb_info || !(mb_info->flags & MULTIBOOT_INFO_MMAP)) {
        kprintf("no memory map\n");
        return;
    }

    uint32_t addr = mb_info->mmap_addr;
    uint32_t end = addr + mb_info->mmap_length;
    int i = 0;

    kprintf("idx  base             length           type\n");

    while (addr < end) {
        struct multiboot_mmap_entry *e =
            (struct multiboot_mmap_entry *)addr;

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
        i++;

        if (i > 64) {
            kprintf("... (truncated)\n");
            break;
        }
    }
}

static void cmd_pagetables(void) {
    uint32_t dir_phys = paging_directory_phys();
    uint32_t self_test = (uint32_t)paging_self_test();

    kprintf("identity map size: 0x%x bytes (%u MiB)\n",
            IDENTITY_MAP_SIZE, IDENTITY_MAP_SIZE / (1024 * 1024));
    kprintf("page directory:    0x%x\n", dir_phys);
    kprintf("kernel ends at:    0x%x\n",
            (unsigned int)(size_t)&__kernel_end);
    kprintf("self test:         %s\n", self_test ? "PASS" : "FAIL");

    kprintf("\npage dir entries 0..%u:\n", PAGE_TABLES - 1);

    for (uint32_t i = 0; i < PAGE_TABLES; i++) {
        uint32_t entry = ((page_dir_entry_t *)dir_phys)[i];
        kprintf("  [%u] = 0x%x\n", i, entry);
    }
}

static void cmd_pgstatus(void) {
    uint32_t cr0 = paging_get_cr0();

    kprintf("CR0:  0x%x\n", cr0);
    kprintf("PG:   %s\n", (cr0 & 0x80000000u) ? "on" : "off");
    kprintf("PE:   %s\n", (cr0 & 0x1u) ? "on" : "off");
    kprintf("CR3:  0x%x\n", paging_get_cr3());
    kprintf("dir phys: 0x%x\n", paging_directory_phys());
    kprintf("self test: %s\n", paging_self_test() ? "PASS" : "FAIL");
}

static void cmd_heapinfo(void) {
    kprintf("heap base:     0x%x\n", (uint32_t)(uintptr_t)heap_start());
    kprintf("heap current:  0x%x\n", (uint32_t)(uintptr_t)heap_current());
    kprintf("used:          %u bytes\n", (uint32_t)heap_used());
    kprintf("capacity:      %u bytes\n", (uint32_t)heap_capacity());
}

static void cmd_heaptest(void) {
    const unsigned int sizes[] = { 16, 32, 64, 128, 1000, 4096 };
    const unsigned int count = sizeof(sizes) / sizeof(sizes[0]);

    unsigned char *blocks[6];
    unsigned int i;

    kprintf("allocating %u blocks...\n", count);

    for (i = 0; i < count; i++) {
        blocks[i] = (unsigned char *)kmalloc(sizes[i]);
        if (!blocks[i]) {
            kprintf("  FAIL: kmalloc(%u) returned NULL\n", sizes[i]);
            return;
        }
        if ((uintptr_t)blocks[i] & (HEAP_ALIGN - 1)) {
            kprintf("  FAIL: misaligned pointer 0x%x\n",
                    (uint32_t)(uintptr_t)blocks[i]);
            return;
        }
        kprintf("  [%u] %5u bytes -> 0x%x\n",
                i, sizes[i], (uint32_t)(uintptr_t)blocks[i]);
    }

    kprintf("writing + verifying pattern...\n");

    for (i = 0; i < count; i++) {
        unsigned int j;
        for (j = 0; j < sizes[i]; j++) {
            blocks[i][j] = (unsigned char)(i * 31 + j);
        }
    }

    for (i = 0; i < count; i++) {
        unsigned int j;
        for (j = 0; j < sizes[i]; j++) {
            if (blocks[i][j] != (unsigned char)(i * 31 + j)) {
                kprintf("  FAIL: corruption in block %u at byte %u\n", i, j);
                return;
            }
        }
    }

    kprintf("all %u blocks verified. used: %u bytes\n",
            count, (uint32_t)heap_used());
}

static void cmd_heaptest2(void) {
    kprintf("step 1: allocate A,B,C\n");
    unsigned char *a = (unsigned char *)kmalloc(256);
    unsigned char *b = (unsigned char *)kmalloc(512);
    unsigned char *c = (unsigned char *)kmalloc(256);
    if (!a || !b || !c) { kprintf("  FAIL: allocation\n"); return; }
    kprintf("  A=0x%x B=0x%x C=0x%x\n",
            (uint32_t)(uintptr_t)a,
            (uint32_t)(uintptr_t)b,
            (uint32_t)(uintptr_t)c);

    kprintf("step 2: write patterns\n");
    for (unsigned int i = 0; i < 256; i++) a[i] = (unsigned char)i;
    for (unsigned int i = 0; i < 512; i++) b[i] = (unsigned char)(i * 3);
    for (unsigned int i = 0; i < 256; i++) c[i] = (unsigned char)(i * 7);

    kprintf("step 3: free B (middle)\n");
    kfree(b);

    kprintf("step 4: allocate D of same size\n");
    unsigned char *d = (unsigned char *)kmalloc(512);
    if (!d) { kprintf("  FAIL: D allocation\n"); return; }
    kprintf("  D=0x%x (expect reuse of B's slot)\n", (uint32_t)(uintptr_t)d);

    for (unsigned int i = 0; i < 512; i++) d[i] = (unsigned char)(i * 5);

    kprintf("step 5: verify A and C untouched\n");
    for (unsigned int i = 0; i < 256; i++) {
        if (a[i] != (unsigned char)i) { kprintf("  FAIL: A corrupted at %u\n", i); return; }
    }
    for (unsigned int i = 0; i < 256; i++) {
        if (c[i] != (unsigned char)(i * 7)) { kprintf("  FAIL: C corrupted at %u\n", i); return; }
    }
    kprintf("  A and C intact\n");

    kprintf("step 6: free A, C, D\n");
    kfree(a);
    kfree(c);
    kfree(d);

    kprintf("step 7: allocate large block (coalescing test)\n");
    unsigned char *big = (unsigned char *)kmalloc(1024);
    if (!big) { kprintf("  FAIL: big allocation\n"); return; }
    kprintf("  big=0x%x (expect coalesced region)\n", (uint32_t)(uintptr_t)big);

    for (unsigned int i = 0; i < 1024; i++) big[i] = (unsigned char)(i ^ 0xAA);
    for (unsigned int i = 0; i < 1024; i++) {
        if (big[i] != (unsigned char)(i ^ 0xAA)) { kprintf("  FAIL: big corrupted at %u\n", i); return; }
    }

    kfree(big);

    kprintf("heaptest2: PASS\n");
    kprintf("used after test: %u bytes\n", (uint32_t)heap_used());
}

static void cmd_heapdump(void) {
    heap_dump();
}

static void cmd_syscall(void) {
    kprintf("triggering int 0x80 with SYS_WRITE...\n");
    uint32_t r = syscall_test();
    kprintf("returned: %u\n", r);
}

static void cmd_panic(void) {
    panic("panic triggered by shell");
}

static void cmd_usermode(void) {
    kprintf("entering user mode...\n");
    usermode_run();
    kprintf("returned from user mode\n");
}

static void handle_command(char *line) {
    int len = (int)strlen(line);

    if (len == 0) return;

    if (strcmp(line, "help") == 0) {
        cmd_help();
    } else if (strcmp(line, "clear") == 0) {
        vga_clear();
    } else if (strcmp(line, "about") == 0) {
        cmd_about();
    } else if (strcmp(line, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(line, "serialtest") == 0) {
        cmd_serialtest();
    } else if (strcmp(line, "meminfo") == 0) {
        cmd_meminfo();
    } else if (strcmp(line, "mmap") == 0) {
        cmd_mmap();
    } else if (strcmp(line, "pagetables") == 0) {
        cmd_pagetables();
    } else if (strcmp(line, "pgstatus") == 0) {
        cmd_pgstatus();
    } else if (strcmp(line, "heapinfo") == 0) {
        cmd_heapinfo();
    } else if (strcmp(line, "heaptest") == 0) {
        cmd_heaptest();
    } else if (strcmp(line, "heaptest2") == 0) {
        cmd_heaptest2();
    } else if (strcmp(line, "heapdump") == 0) {
        cmd_heapdump();
    } else if (strcmp(line, "syscall") == 0) {
        cmd_syscall();
    } else if (strcmp(line, "panic") == 0) {
        cmd_panic();
    } else if (strncmp(line, "echo ", 5) == 0) {
        kprintf("%s\n", line + 5);
    } else if (strncmp(line, "hex ", 4) == 0) {
        cmd_hex(line + 4);
    } else if (strncmp(line, "sleep ", 6) == 0) {
        cmd_sleep(line + 6);
    } else if (strcmp(line, "usermode") == 0) {
        cmd_usermode();
    } else {
        kprintf("unknown command: %s\n", line);
    }
}

static void shell_run(void) __attribute__((noreturn));
static void shell_run(void) {
    char line[LINE_MAX];
    int len = 0;

    shell_prompt();

    for (;;) {
        int c = keyboard_getchar();
        if (c < 0) {
            __asm__ volatile ("hlt");
            continue;
        }

        if (c == '\n') {
            vga_putchar('\n');
            line[len] = '\0';
            handle_command(line);
            len = 0;
            shell_prompt();
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                vga_backspace();
            }
        } else if (c == 27) {
            vga_putchar('\n');
            len = 0;
            shell_prompt();
        } else if (c >= 32 && c < 127 && len < LINE_MAX - 1) {
            line[len++] = (char)c;
            vga_putchar((char)c);
        }
    }
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

    kprintf("telOS v0.7a\n");
    kprintf("==========\n\n");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        kprintf("warning: bad multiboot magic 0x%x\n\n", magic);
    } else {
        kprintf("multiboot OK, info at 0x%x\n", info_addr);
    }

    serial_puts("[boot] about to enable paging\n");
    paging_enable_now();
    serial_puts("[boot] survived paging flip\n");

    if (!paging_enabled()) {
        panic("paging_enable_now returned but PG is clear");
    }

    if (!paging_self_test()) {
        panic("paging self test failed after enable");
    }

    kprintf("paging ON, dir at 0x%x\n", paging_directory_phys());

    heap_init();

    syscall_init();
    gdt_set_kernel_stack((uint32_t)&stack_top);

    kprintf("heap: base=0x%x cap=%u bytes\n\n",
            (uint32_t)(uintptr_t)heap_start(),
            (uint32_t)heap_capacity());

    kprintf("Welcome to telOS. Type 'help'.\n\n");

    shell_run();
}
