CC      = gcc
AS      = gcc
CFLAGS  = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pic -fno-pie \
          -Ilibk -Ikernel
LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 -nostdlib -no-pie -lgcc

OBJS = boot/boot.o \
       kernel/kernel.o \
       kernel/vga.o \
       kernel/gdt.o \
       kernel/idt.o \
       kernel/keyboard.o \
       kernel/timer.o \
       kernel/serial.o \
       kernel/panic.o \
       kernel/paging.o \
       kernel/heap.o \
       kernel/syscall.o \
       kernel/usermode.o \
       kernel/user_prog.o \
       libk/string.o \
       libk/stdio.o

all: telos.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(AS) -m32 -c $< -o $@

telos.bin: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

iso: telos.bin
	mkdir -p isodir/boot/grub
	cp telos.bin isodir/boot/telos.bin
	printf 'menuentry "telOS" {\n  multiboot /boot/telos.bin\n}\n' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o telos.iso isodir

run: iso
	qemu-system-i386 -cdrom telos.iso -serial stdio

clean:
	rm -f $(OBJS) telos.bin telos.iso
	rm -rf isodir
