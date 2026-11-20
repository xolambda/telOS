#ifndef IDT_H
#define IDT_H

void idt_init(void);
void irq0_c_handler(void);
void irq1_c_handler(void);

#endif
