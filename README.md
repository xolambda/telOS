<div align="center">

# telOS

**A tiny 32-bit operating system, written from scratch in C and assembly.**

It's bare metal.

[![Platform](https://img.shields.io/badge/platform-x86--32-informational?style=flat-square)](#)
[![Language](https://img.shields.io/badge/language-C%20%2B%20assembly-blue?style=flat-square)](#)
[![Boot](https://img.shields.io/badge/boot-Multiboot%20%2F%20GRUB-green?style=flat-square)](#)
[![Memory](https://img.shields.io/badge/memory-paging%20enabled-purple?style=flat-square)](#)
[![Status](https://img.shields.io/badge/status-v0.5c%20paging-brightgreen?style=flat-square)](#)

</div>

---

## What is this?

`telOS` is a hobby operating system. It's small enough to read end-to-end in an afternoon, but complete enough to boot on real hardware (or QEMU), print to the screen, handle keyboard input, keep time, and run with virtual memory turned on.

It is **not** a Unix clone, **not** a teaching OS with a framework, and **not** a Linux distro. Every line of code in `kernel/` and `libk/` was written for this project. There is no standard library, no runtime, and no host operating system underneath.

## Features

### Boot & CPU
- **Multiboot** compliant — loads via GRUB 2
- 32-bit protected mode, flat memory model
- **GDT** with kernel code + data segments
- **IDT** with 256 gates
- **PIC** remapped to `0x20..0x2F`

### Interrupts & I/O
- **PIT** at 100 Hz (IRQ0) — real uptime, `sleep`, benchmarks
- **Keyboard** (IRQ1) — scancode decoding, ring-buffered
- **Serial** (COM1, 38400 8N1) — every `kprintf` mirrored to UART

### Memory
- **Paging enabled** (`CR0.PG = 1`)
- 16 MiB identity-mapped (`0x00000000`–`0x00FFFFFF`)
- 4 page tables, 4096 pages, all present + writable
- `kernel_end` symbol exported from linker for the future heap

### Runtime
- `kprintf` — `%d %i %u %x %X %s %c %p %%`, field width, `0`-pad, `-` left-align
- `ksnprintf` — same formatter, into a caller-provided buffer
- `libk/string` — `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `memcpy`, `memset`, `memcmp`
- `libk/ctype` — `isdigit`, `isalpha`, `isspace`, `isupper`, `islower`, `toupper`, `tolower`

### Shell
Twelve commands. Line editing with backspace and ESC-to-cancel. Hardware cursor tracking. Full ring buffer between IRQ handler and shell loop.

### Safety
- `panic(msg)` — prints to VGA **and** serial, disables interrupts, halts
- `ASSERT(cond)` — one-line assertions with file and line
- `P` canary — a single byte sent over serial immediately after `CR0.PG` is set, proving the CPU survived the MMU flip

---

## Requirements

**Debian / Ubuntu**

```bash
sudo apt install build-essential gcc-multilib grub-pc-bin xorriso qemu-system-x86
```

**Arch**

```bash
sudo pacman -S base-devel gcc-multilib grub xorriso qemu-system-x86 mtools
```

**macOS** — install `i386-elf-gcc` and `qemu` via Homebrew, then set `CC = i386-elf-gcc` and `AS = i386-elf-gcc --target=i386` in the Makefile.

---

## Build & Run

```bash
make run
```

That builds the kernel, wraps it in a bootable ISO, and boots it in QEMU with serial attached to your terminal.

Other targets:

```bash
make            # build telos.bin only
make iso        # build telos.iso
make clean      # remove all artifacts
```

To boot on real hardware, write `telos.iso` to a USB stick:

```bash
sudo dd if=telos.iso of=/dev/sdX bs=4M status=progress && sync
```

(Replace `sdX` with your USB device — *check with `lsblk` before running this.*)

---

## Project layout

```
.
├── boot/
│   └── boot.S          Multiboot header, _start, GDT/IDT/IRQ stubs
├── kernel/
│   ├── kernel.c        kernel_main + shell + commands
│   ├── gdt.c           global descriptor table
│   ├── idt.c           interrupt descriptor table + PIC remap
│   ├── keyboard.c      scancode → ring buffer
│   ├── timer.c         PIT, ticks, sleep
│   ├── serial.c        COM1 driver
│   ├── vga.c           text mode 3 driver + hardware cursor
│   ├── paging.c        page directory, page tables, self test
│   ├── paging_asm.h    CR0/CR3 inline-asm helpers
│   ├── panic.c         panic + ASSERT
│   ├── multiboot.h     multiboot info structures
│   └── io.h            port I/O (inb / outb)
├── libk/
│   ├── string.c/.h     freestanding string functions
│   ├── ctype.h         character classification
│   └── stdio.c/.h      kprintf / ksnprintf
├── linker.ld           links kernel at 1 MiB, exports __kernel_end
└── Makefile
```

---

## How the shell talks to the hardware

Pressing a key travels this path:

```
keyboard → IRQ1 → irq1_c_handler → keyboard_handle_scancode
        → ring buffer → keyboard_getchar → shell_run → handle_command
        → kprintf → vga_putchar + serial_putc
```

The IRQ handler does the *minimum*: read the scancode, translate it, push it into a ring buffer, send EOI. All display work happens in the shell loop, at process (well — kernel thread) context, with interrupts handled safely. This is the right structure, and it's the one you'll want to preserve when you add scheduling.

---

## Versioning

Each version is a milestone.

| Version | Milestone |
|---------|-----------|
| **v0.1** | Boots, prints to VGA, keyboard echoes |
| **v0.2** | Hardware cursor, line buffer, shell loop, backspace |
| **v0.3** | `libk` strings, `kprintf`, cleaner shell |
| **v0.4** | Serial mirror, PIT timer, `sleep` / `uptime` |
| **v0.5a** | Multiboot info passed to kernel, `panic`, `meminfo`, `mmap` |
| **v0.5b** | Page directory + tables built (PG off), self test |
| **v0.5c** | **Paging enabled**, 16 MiB identity-mapped |
| **v0.6** | Heap: `kmalloc` / `kfree` |
| **v0.7** | TSS, ring 3, syscall gate |
| **v0.8** | Round-robin scheduler, `fork`-like threads |
| **v0.9** | RAMFS |
| **v1.0** | Something that runs a program |

---

## Roadmap

```
[x] Boot to protected mode
[x] GDT, IDT, PIC remap
[x] Keyboard interrupts
[x] VGA text with cursor tracking
[x] libk + kprintf
[x] Serial debug output
[x] PIT timer + sleep
[x] Multiboot info
[x] Paging
[ ] Heap allocator
[ ] TSS + ring 3
[ ] System calls
[ ] Multitasking
[ ] Filesystem
[ ] Userspace shell
```

---

## Design notes

Deliberate choices. I won't change this.

- **Serial mirror is default-on.** `kprintf` writes to VGA *and* COM1. When the MMU breaks and VGA dies, serial is the only way to see what happened. This is why it's wired into the formatter rather than bolted on later.

- **The IRQ handlers are tiny.** They do the minimum work and defer everything else. `irq1_c_handler` pushes a scancode and returns; the shell does the echoing. This is the correct design and gets more important once scheduling arrives.

- **Paging is identity-mapped.** `0x00000000`–`0x00FFFFFF` map to themselves. This means a virtual address is the same as a physical address for everything the kernel currently touches, which is the simplest possible setup that still proves the MMU works. Non-identity mappings will come with userspace.

- **The `P` canary.** When `CR0.PG` is flipped, the very next asm block does `out 'P', 0x3F8`. If you see `P` on serial, the CPU survived the flip. If you don't, it triple-faulted. One byte of diagnostic can save hours of guessing.

- **No libc, no stdarg.h.** `kprintf` uses `__builtin_va_list` and `libk` provides everything else. The entire toolchain dependency is `gcc -ffreestanding -nostdlib -lgcc`.

---

## License

MIT. See `LICENSE`.

---

<div align="center">

*stop scrolling man*

</div>
