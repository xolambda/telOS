#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_HZ 100
#define TIMER_RESCHED_TICKS 5

void     timer_init(void);
uint32_t timer_ticks(void);
uint32_t timer_uptime_ms(void);
void     timer_sleep_ms(uint32_t ms);
void     timer_handle_tick(void);
int      timer_should_resched(void);
void     timer_clear_resched(void);

#endif
