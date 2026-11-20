#include "paging_asm.h"
#include "serial.h"
#include "paging.h"
#include "stdio.h"
#include "string.h"
#include "panic.h"

static page_dir_entry_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static page_table_entry_t page_tables[PAGE_TABLES][PAGES_PER_TABLE]
    __attribute__((aligned(PAGE_SIZE)));

static inline uint32_t phys_of(const void *p) {
    return (uint32_t)(uintptr_t)p;
}

static void zero_table(page_table_entry_t *t) {
    memset(t, 0, PAGE_SIZE);
}

void paging_init(void) {
    memset(page_directory, 0, sizeof(page_directory));

    for (int t = 0; t < PAGE_TABLES; t++) {
        zero_table(page_tables[t]);

        for (int p = 0; p < PAGES_PER_TABLE; p++) {
            uint32_t addr = (uint32_t)t * PAGES_PER_TABLE * PAGE_SIZE
                          + (uint32_t)p * PAGE_SIZE;
            page_tables[t][p] = addr | PAGE_PRESENT | PAGE_WRITE;
        }

        page_directory[t] = phys_of(page_tables[t]) | PAGE_PRESENT | PAGE_WRITE;
    }

    for (int i = PAGE_TABLES; i < 1024; i++) {
        page_directory[i] = 0;
    }
}

uint32_t paging_directory_phys(void) {
    return phys_of(page_directory);
}

int paging_self_test(void) {
    uint32_t dir_phys = phys_of(page_directory);
    uint32_t first_table_phys = phys_of(page_tables[0]);
    uint32_t fourth_table_phys = phys_of(page_tables[PAGE_TABLES - 1]);

    if (dir_phys & 0xFFF) return 0;
    if (first_table_phys & 0xFFF) return 0;
    if (fourth_table_phys & 0xFFF) return 0;

    if ((page_directory[0] & 0xFFFFF000) != first_table_phys) return 0;
    if ((page_directory[PAGE_TABLES - 1] & 0xFFFFF000) != fourth_table_phys) return 0;

    if ((page_tables[0][0] & 0xFFFFF000) != 0x00000000) return 0;
    if ((page_tables[0][1] & 0xFFFFF000) != 0x00001000) return 0;
    if ((page_tables[3][1023] & 0xFFFFF000) != (IDENTITY_MAP_SIZE - PAGE_SIZE))
        return 0;

    if (!(page_tables[0][0] & PAGE_PRESENT)) return 0;
    if (!(page_tables[0][0] & PAGE_WRITE)) return 0;

    return 1;
}

void paging_enable_now(void) {
    uint32_t dir_phys = phys_of(page_directory);

    serial_puts("paging: CR3 <- ");
    serial_puthex(dir_phys);
    serial_puts("\n");

    paging_load_cr3(dir_phys);

    serial_puts("paging: flipping CR0.PG ... ");

    __asm__ volatile (
        "mov %%cr0, %%eax\n"
        "or  $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        "mov $0x3F8, %%dx\n"
        "mov $'P', %%al\n"
        "out %%al, %%dx\n"
        : : : "eax", "edx", "memory"
    );

    serial_puts("\npaging: enabled\n");
}

int paging_enabled(void) {
    return paging_is_enabled();
}
