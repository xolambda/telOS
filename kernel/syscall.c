#include "syscall.h"
#include "stdio.h"
#include "serial.h"
#include "string.h"
#include "panic.h"
#include "usermode.h"

static int sys_write(const char *msg) {
    if (!msg) return -1;
    kprintf("%s", msg);
    return (int)strlen(msg);
}

void syscall_dispatch(struct pusha_regs *regs) {
    ASSERT(regs != 0);
    ASSERT(regs->eax >= 1);
    ASSERT(regs->eax <= 2);

    switch (regs->eax) {
    case SYS_WRITE: {
        const char *msg = (const char *)regs->ebx;
        regs->eax = (uint32_t)sys_write(msg);
        break;
    }
    case SYS_EXIT: {
        int code = (int)regs->ebx;
        usermode_return_from_exit(code);
        break;
    }
    default:
        kprintf("syscall: unknown number %u\n", regs->eax);
        regs->eax = (uint32_t)-1;
        break;
    }
}

uint32_t syscall_test(void) {
    const char *msg = "hello from int 0x80\n";
    uint32_t result;

    __asm__ volatile (
        "int $0x80"
        : "=a"(result)
        : "a"((uint32_t)SYS_WRITE), "b"(msg)
        : "memory"
    );

    return result;
}

void syscall_init(void) {
    serial_puts("[syscall] int 0x80 gate installed\n");
}
