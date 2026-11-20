.set MAGIC,    0x1BADB002
.set FLAGS,    0x0
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    cli
    mov $stack_top, %esp

    push %ebx
    push %eax
    call kernel_main

    cli
1:  hlt
    jmp 1b
.size _start, . - _start

.global gdt_flush
gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ljmp $0x08, $.flush
.flush:
    ret

.global idt_load
idt_load:
    mov 4(%esp), %eax
    lidt (%eax)
    sti
    ret

.extern irq0_c_handler
.extern irq1_c_handler

.global irq0_handler
irq0_handler:
    pusha
    call irq0_c_handler
    popa
    iret

.global irq1_handler
irq1_handler:
    pusha
    call irq1_c_handler
    popa
    iret
