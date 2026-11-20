#include "timer.h"
#include "io.h"

#define PIT_FREQ 1193182u

static volatile uint32_t ticks = 0;
static volatile uint32_t resched_counter = 0;
static volatile int need_resched = 0;

void timer_init(void) {
    uint32_t divisor = PIT_FREQ / TIMER_HZ;

    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

void timer_handle_tick(void) {
    ticks++;
    resched_counter++;
    if (resched_counter >= TIMER_RESCHED_TICKS) {
        resched_counter = 0;
        need_resched = 1;
    }
}

uint32_t timer_ticks(void) {
    return ticks;
}

uint32_t timer_uptime_ms(void) {
    return ticks * (1000u / TIMER_HZ);
}

void timer_sleep_ms(uint32_t ms) {
    uint32_t target = ticks + (ms * TIMER_HZ) / 1000u;
    while ((int32_t)(ticks - target) < 0) {
        __asm__ volatile ("hlt");
        extern void task_yield(void);
        task_yield();
    }
}

int timer_should_resched(void) {
    return need_resched;
}

void timer_clear_resched(void) {
    need_resched = 0;
}
