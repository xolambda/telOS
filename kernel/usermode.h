#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

void usermode_run(void);
void usermode_return_from_exit(int code) __attribute__((noreturn));
int  usermode_is_active(void);

extern uint32_t saved_kernel_eip;
extern uint32_t saved_kernel_esp;
extern uint32_t saved_kernel_ebp;

#endif
