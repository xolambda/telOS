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
.global stack_top
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

.global tss_flush
tss_flush:
    mov $0x28, %ax
    ltr %ax
    ret

.global idt_load
idt_load:
    mov 4(%esp), %eax
    lidt (%eax)
    sti
    ret

.extern irq0_c_handler
.extern irq1_c_handler
.extern syscall_dispatch
.extern gp_c_handler
.extern pf_c_handler
.extern ud_c_handler
.extern saved_kernel_eip
.extern saved_kernel_esp
.extern saved_kernel_ebp

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

.global syscall_entry
syscall_entry:
    cld
    pusha
    mov %esp, %eax
    push %eax
    call syscall_dispatch
    add $4, %esp
    popa
    iret

.global exc_gp_handler
exc_gp_handler:
    pusha
    call gp_c_handler
    popa
    iret

.global exc_pf_handler
exc_pf_handler:
    pusha
    call pf_c_handler
    popa
    iret

.global exc_ud_handler
exc_ud_handler:
    pusha
    call ud_c_handler
    popa
    iret

.global switch_to_user
switch_to_user:
    mov (%esp), %eax
    mov %eax, saved_kernel_eip
    lea 4(%esp), %eax
    mov %eax, saved_kernel_esp
    mov %ebp, saved_kernel_ebp

    mov 4(%esp), %ecx
    mov 8(%esp), %edx

    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    pushl $0x23
    pushl %edx
    pushfl
    pushl $0x1B
    pushl %ecx
    iret

.global switch_to_kernel
switch_to_kernel:
    mov 4(%esp), %ecx
    mov 8(%esp), %edx

    mov saved_kernel_ebp, %ebp

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    mov %edx, %esp
    cld
    jmp *%ecx

.global switch_context
switch_context:
    mov 4(%esp), %eax
    mov 8(%esp), %edx

    push %ebp
    push %ebx
    push %esi
    push %edi
    mov %esp, (%eax)

    mov (%edx), %esp
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret
