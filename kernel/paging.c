#include "paging.h"
#include "paging_asm.h"
#include "serial.h"
#include "stdio.h"
#include "string.h"
#include "panic.h"
#include "heap.h"

static page_dir_entry_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static page_table_entry_t page_tables[PAGE_TABLES][PAGES_PER_TABLE]
    __attribute__((aligned(PAGE_SIZE)));

static inline uint32_t phys_of(const void *p) {
    return (uint32_t)(uintptr_t)p;
}

static void zero_table(page_table_entry_t *t) {
    memset(t, 0, PAGE_SIZE);
}

static page_dir_entry_t *page_directory_ptr(void) {
    return (page_dir_entry_t *)paging_get_cr3();
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

int paging_map_user(uint32_t virt, uint32_t phys, int writable) {
    if (virt & 0xFFF) {
        serial_puts("[paging_map_user] virt not aligned: ");
        serial_puthex(virt);
        serial_puts("\n");
        return 0;
    }
    if (phys & 0xFFF) {
        serial_puts("[paging_map_user] phys not aligned: ");
        serial_puthex(phys);
        serial_puts("\n");
        return 0;
    }

    uint32_t dir_index = virt >> 22;
    uint32_t tab_index = (virt >> 12) & 0x3FF;

    page_dir_entry_t *pdir = page_directory_ptr();
    page_table_entry_t *ptab;

    if (pdir[dir_index] & PAGE_PRESENT) {
        ptab = (page_table_entry_t *)(pdir[dir_index] & 0xFFFFF000);
    } else {
        ptab = (page_table_entry_t *)kmalloc_page();
        if (!ptab) {
            serial_puts("[paging_map_user] kmalloc_page failed\n");
            return 0;
        }
        for (int i = 0; i < PAGES_PER_TABLE; i++) ptab[i] = 0;
        pdir[dir_index] = (uint32_t)ptab | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
        serial_puts("[paging_map_user] allocated new page table at ");
        serial_puthex((uint32_t)ptab);
        serial_puts("\n");
    }

    uint32_t flags = PAGE_PRESENT | PAGE_USER;
    if (writable) flags |= PAGE_WRITE;

    pdir[dir_index] |= PAGE_USER;
    ptab[tab_index] = (phys & 0xFFFFF000) | flags;

    __asm__ volatile ("mov %%cr3, %%eax; mov %%eax, %%cr3" : : : "eax", "memory");

    serial_puts("[paging_map_user] virt=");
    serial_puthex(virt);
    serial_puts(" -> phys=");
    serial_puthex(phys);
    serial_puts(" dir_idx=");
    serial_puthex(dir_index);
    serial_puts(" tab_idx=");
    serial_puthex(tab_index);
    serial_puts("\n");

    return 1;
}
