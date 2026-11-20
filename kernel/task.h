#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_STATE_READY   0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_DEAD    2

typedef struct task {
    uint32_t esp;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t eip;

    void    *stack;
    uint32_t stack_size;
    char     name[32];

    int state;
    struct task *next;
} task_t;

void task_init(void);
task_t *ktask_create(void (*fn)(void), const char *name);
void task_switch(task_t *from, task_t *to);
task_t *task_current(void);
void task_yield(void);
void schedule(void);
void task_exit(void) __attribute__((noreturn));
int task_count(void);

#endif
