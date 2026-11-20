#include "task.h"
#include "timer.h"
#include "paging.h"
#include "heap.h"
#include "panic.h"
#include "stdio.h"
#include "string.h"
#include "serial.h"

extern void switch_context(task_t *old, task_t *new);

static task_t *current = 0;
static task_t kernel_task;
static task_t *ready_head = 0;
static task_t *ready_tail = 0;

static void ready_push(task_t *t) {
    t->next = 0;
    if (ready_tail) {
        ready_tail->next = t;
        ready_tail = t;
    } else {
        ready_head = ready_tail = t;
    }
}

static task_t *ready_pop(void) {
    if (!ready_head) return 0;
    task_t *t = ready_head;
    ready_head = t->next;
    if (!ready_head) ready_tail = 0;
    t->next = 0;
    return t;
}

static void task_trampoline(void) {
    void (*fn)(void);
    __asm__ volatile ("mov %%ebx, %0" : "=r"(fn));
    fn();
    task_exit();
}

void task_init(void) {
    memset(&kernel_task, 0, sizeof(kernel_task));
    strcpy(kernel_task.name, "kernel");
    kernel_task.state = TASK_STATE_RUNNING;
    current = &kernel_task;
    ready_head = ready_tail = 0;
    serial_puts("[task] kernel task initialized\n");
}

task_t *ktask_create(void (*fn)(void), const char *name) {
    task_t *t = (task_t *)kmalloc(sizeof(task_t));
    if (!t) return 0;
    memset(t, 0, sizeof(task_t));

    t->stack_size = PAGE_SIZE;
    t->stack = kmalloc_page();
    if (!t->stack) { kfree(t); return 0; }
    memset(t->stack, 0, PAGE_SIZE);

    uint32_t *sp = (uint32_t *)((uint8_t *)t->stack + PAGE_SIZE);

    *--sp = (uint32_t)task_trampoline;
    *--sp = 0;
    *--sp = (uint32_t)fn;
    *--sp = 0;
    *--sp = 0;

    t->esp = (uint32_t)sp;
    t->ebp = 0;
    t->ebx = 0;
    t->esi = 0;
    t->edi = 0;
    t->eip = (uint32_t)task_trampoline;

    strncpy(t->name, name, sizeof(t->name) - 1);
    t->state = TASK_STATE_READY;
    t->next = 0;

    ready_push(t);

    serial_puts("[task] created ");
    serial_puts(t->name);
    serial_puts(" stack=0x");
    serial_puthex((uint32_t)t->stack);
    serial_puts(" esp=0x");
    serial_puthex(t->esp);
    serial_puts("\n");

    return t;
}

task_t *ktask_spawn_detached(void (*fn)(void), const char *name) {
    return ktask_create(fn, name);
}

void schedule(void) {
    if (!current) return;

    timer_clear_resched();

    if (!ready_head) {
        if (current->state == TASK_STATE_DEAD) {
            serial_puts("[sched] no tasks left, halting\n");
            for (;;) __asm__ volatile ("cli; hlt");
        }
        return;
    }

    task_t *prev = current;

    if (prev->state != TASK_STATE_DEAD) {
        prev->state = TASK_STATE_READY;
        ready_push(prev);
    }

    task_t *next = ready_pop();
    if (next == prev) {
        prev->state = TASK_STATE_RUNNING;
        return;
    }

    current = next;
    next->state = TASK_STATE_RUNNING;
    switch_context(prev, next);
}

void task_yield(void) {
    if (ready_head || timer_should_resched()) {
        schedule();
    }
}

void task_wait(task_t *t) {
    while (t && t->state != TASK_STATE_DEAD) {
        task_yield();
    }
}

void task_switch(task_t *from, task_t *to) {
    if (from == to) return;
    to->state = TASK_STATE_RUNNING;
    if (from->state != TASK_STATE_DEAD) from->state = TASK_STATE_READY;
    current = to;
    switch_context(from, to);
}

task_t *task_current(void) {
    return current;
}

void task_exit(void) {
    kprintf("[task] %s exited\n", current->name);
    current->state = TASK_STATE_DEAD;
    schedule();
    for (;;) __asm__ volatile ("cli; hlt");
}

int task_count(void) {
    int n = (current && current->state != TASK_STATE_DEAD) ? 1 : 0;
    for (task_t *t = ready_head; t; t = t->next) n++;
    return n;
}
