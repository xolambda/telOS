
#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "paging_asm.h"

#define PAGE_SIZE 4096
#define PAGES_PER_TABLE 1024
#define PAGE_TABLES 4
#define IDENTITY_MAP_SIZE (PAGE_TABLES * PAGES_PER_TABLE * PAGE_SIZE)

#define PAGE_PRESENT  0x1
#define PAGE_WRITE    0x2
#define PAGE_USER     0x4

typedef uint32_t page_dir_entry_t;
typedef uint32_t page_table_entry_t;

void paging_enable_now(void);
int  paging_enabled(void);

void paging_init(void);
uint32_t paging_directory_phys(void);
int paging_self_test(void);

#endif
