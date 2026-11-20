#include "usermode.h"
#include "user_prog.h"
#include "paging.h"
#include "heap.h"
#include "stdio.h"
#include "serial.h"
#include "panic.h"
#include "string.h"
#include "gdt.h"

extern void switch_to_user(uint32_t eip, uint32_t esp);
extern void switch_to_kernel(uint32_t eip, uint32_t esp);

uint32_t saved_kernel_eip;
uint32_t saved_kernel_esp;
uint32_t saved_kernel_ebp;

static uint8_t irq_stack[8192] __attribute__((aligned(16)));
static int usermode_active = 0;

void usermode_run(void) {
    if (usermode_active) {
        kprintf("usermode: already running\n");
        return;
    }

    serial_puts("[user] allocating pages\n");

    uint8_t *code_page = (uint8_t *)kmalloc_page();
    uint8_t *stack_page = (uint8_t *)kmalloc_page();
    if (!code_page || !stack_page) {
        panic("usermode: kmalloc_page failed");
    }
    memset(code_page, 0, PAGE_SIZE);
    memset(stack_page, 0, PAGE_SIZE);

    memcpy(code_page, user_code_blob, user_code_size);

    const char *msg = "hello from ring 3\n";
    if (strlen(msg) + 1 > 0x40) {
        panic("usermode: message too long for offset");
    }
    memcpy(code_page + USER_MSG_OFFSET, msg, strlen(msg) + 1);

    serial_puts("[user] mapping user pages\n");

    if (!paging_map_user(USER_CODE_BASE, (uint32_t)code_page, 0)) {
        panic("usermode: map code failed");
    }
    if (!paging_map_user(USER_STACK_BASE, (uint32_t)stack_page, 1)) {
        panic("usermode: map stack failed");
    }

    serial_puts("[user] jumping to ring 3\n");

    usermode_active = 1;

    gdt_set_kernel_stack((uint32_t)(irq_stack + sizeof(irq_stack)));

    switch_to_user(USER_CODE_BASE, USER_STACK_BASE + PAGE_SIZE - 16);

    __asm__ volatile ("sti");

    usermode_active = 0;
    kprintf("usermode: back in kernel, exit code received\n");
}

int usermode_is_active(void) {
    return usermode_active;
}

void usermode_return_from_exit(int code) {
    kprintf("usermode: SYS_EXIT(%d)\n", code);
    switch_to_kernel(saved_kernel_eip, saved_kernel_esp);
    __builtin_unreachable();
}
