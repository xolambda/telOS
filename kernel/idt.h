#ifndef IDT_H
#define IDT_H

void idt_init(void);
void irq0_c_handler(void);
void irq1_c_handler(void);
void gp_c_handler(void);
void pf_c_handler(void);
void ud_c_handler(void);

#endif
