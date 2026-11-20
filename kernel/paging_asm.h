#ifndef PAGING_ASM_H
#define PAGING_ASM_H

#include <stdint.h>

static inline void paging_load_cr3(uint32_t phys) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(phys) : "memory");
}

static inline void paging_enable(void) {
    __asm__ volatile (
        "mov %cr0, %eax\n"
        "or  $0x80000000, %eax\n"
        "mov %eax, %cr0\n"
        : : : "eax", "memory"
    );
}

static inline int paging_is_enabled(void) {
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x80000000u) != 0;
}

static inline uint32_t paging_get_cr3(void) {
    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline uint32_t paging_get_cr0(void) {
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

#endif
