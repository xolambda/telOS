#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT  2

struct pusha_regs {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
};

void syscall_init(void);
void syscall_dispatch(struct pusha_regs *regs);
uint32_t syscall_test(void);

#endif
