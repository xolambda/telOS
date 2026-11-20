<div align="center">

# telOS

**A tiny 32-bit operating system, written from scratch in C and assembly.**

It's bare metal.

[![Platform](https://img.shields.io/badge/platform-x86--32-informational?style=flat-square)](#)
[![Language](https://img.shields.io/badge/language-C%20%2B%20assembly-blue?style=flat-square)](#)
[![Boot](https://img.shields.io/badge/boot-Multiboot%20%2F%20GRUB-green?style=flat-square)](#)
[![Memory](https://img.shields.io/badge/memory-paging%20%2B%20heap-purple?style=flat-square)](#)
[![Privilege](https://img.shields.io/badge/privilege-ring%200%20%2F%20ring%203-orange?style=flat-square)](#)
[![Status](https://img.shields.io/badge/status-v0.7b%20ring%203-brightgreen?style=flat-square)](#)

</div>

---

## What is this?

`telOS` is a hobby operating system. It's small enough to read end-to-end in an afternoon, but complete enough to boot on real hardware (or QEMU), print to the screen, handle keyboard input, keep time, manage memory with paging and a heap allocator, and run code in **ring 3** — real user mode, separated from the kernel by hardware privilege.

It is **not** a Unix clone, **not** a teaching OS with a framework, and **not** a Linux distro. Every line of code in `kernel/` and `libk/` was written for this project. There is no standard library, no runtime, and no host operating system underneath.

## Features

### Boot & CPU
- **Multiboot** compliant — loads via GRUB 2
- 32-bit protected mode, flat memory model
- **GDT** with kernel code + data, user code + data, and a **TSS**
- **IDT** with 256 gates (exceptions, IRQs, `int 0x80`)
- **PIC** remapped to `0x20..0x2F`

### Interrupts & I/O
- **PIT** at 100 Hz (IRQ0) — real uptime, `sleep`, benchmarks
- **Keyboard** (IRQ1) — scancode decoding, ring-buffered
- **Serial** (COM1, 38400 8N1) — every `kprintf` mirrored to UART
- **Exception handlers** for `#UD` (6), `#GP` (13), `#PF` (14) — print `cr2`/`err`/`cs:eip` to serial instead of triple-faulting

### Memory
- **Paging enabled** (`CR0.PG = 1`), 16 MiB identity-mapped
- 4 static page tables for the identity region, dynamically allocated page tables for user mappings
- **`kmalloc` / `kfree`** — free-list allocator with block headers, splitting, and coalescing
- **`kmalloc_page`** — page-aligned variant for allocations that must land on 4 KiB boundaries
- Kernel exports `__kernel_end` from the linker for the heap base

### Privilege & Syscalls
- **Ring 3 execution** — user code runs at CPL=3, kernel at CPL=0
- **TSS** loaded via `ltr 0x28`, `esp0` points to a dedicated IRQ stack during user mode
- **`int 0x80`** — DPL=3 interrupt gate with a register-based ABI
  - `eax` = syscall number, `ebx`/`ecx`/`edx` = args, return in `eax`
  - `SYS_WRITE` (1) — write a NUL-terminated string to console
  - `SYS_EXIT` (2) — return to kernel
- **Clean ring transitions** — `switch_to_user` / `switch_to_kernel` in assembly, with `saved_kernel_eip`/`esp`/`ebp` captured from the `call` frame
- **`IF` restored** on return from user mode (interrupt gate clears it, `sti` after)

### Runtime
- `kprintf` — `%d %i %u %x %X %s %c %p %%`, field width, `0`-pad, `-` left-align
- `ksnprintf` — same formatter, into a caller-provided buffer
- `libk/string` — `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `memcpy`, `memset`, `memcmp`
- `libk/ctype` — `isdigit`, `isalpha`, `isspace`, `isupper`, `islower`, `toupper`, `tolower`

### Shell
17 commands. Line editing with backspace and ESC-to-cancel. Hardware cursor tracking. Full ring buffer between IRQ handler and shell loop. Heap and paging diagnostics (`heapinfo`, `heapdump`, `heaptest`, `heaptest2`, `pagetables`, `pgstatus`, `meminfo`, `mmap`). User-mode trigger (`usermode`).

### Safety
- `panic(msg)` — prints to VGA **and** serial, disables interrupts, halts
- `ASSERT(cond)` — one-line assertions with file and line
- **Exception handlers** for `#UD`, `#GP`, `#PF` that dump register state and halt cleanly
- **`P` canary** — a single byte sent over serial immediately after `CR0.PG` is set, proving the CPU survived the MMU flip

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
│   └── boot.S          Multiboot header, _start, GDT/IDT/IRQ/syscall stubs
├── kernel/
│   ├── kernel.c        kernel_main + shell + commands
│   ├── gdt.c           global descriptor table + TSS
│   ├── idt.c           interrupt descriptor table + PIC remap + exception handlers
│   ├── keyboard.c      scancode → ring buffer
│   ├── timer.c         PIT, ticks, sleep
│   ├── serial.c        COM1 driver
│   ├── vga.c           text mode 3 driver + hardware cursor
│   ├── paging.c        page directory, page tables, self test, user mapping
│   ├── paging_asm.h    CR0/CR3 inline-asm helpers
│   ├── heap.c          free-list kmalloc/kfree, kmalloc_page
│   ├── syscall.c       int 0x80 dispatcher
│   ├── syscall.h       pusha_regs struct + syscall numbers
│   ├── usermode.c      ring 3 entry / exit
│   ├── usermode.h      usermode API + saved kernel continuation
│   ├── user_prog.c     hand-assembled user program blob
│   ├── user_prog.h     user code base + message offset
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

The IRQ handler does the *minimum*: read the scancode, translate it, push it into a ring buffer, send EOI. All display work happens in the shell loop, at kernel-thread context, with interrupts handled safely. This is the right structure, and it's the one you'll want to preserve when you add scheduling.

## How `usermode` crosses the privilege boundary

```
shell → cmd_usermode → usermode_run
      → kmalloc_page × 2, copy user code blob into one, zero the other
      → paging_map_user(0x40000000, code_phys, RO)
      → paging_map_user(0x40100000, stack_phys, RW)
      → gdt_set_kernel_stack(irq_stack_top)
      → switch_to_user(eip, esp)
          → save kernel eip/esp/ebp from the call frame
          → load user ds/es/fs/gs
          → iret (ring 0 → ring 3, 5-value frame)
      → user code runs at CPL=3
          → int 0x80 (SYS_WRITE) → syscall_entry → syscall_dispatch → kprintf
          → int 0x80 (SYS_EXIT)  → syscall_entry → syscall_dispatch
                                 → usermode_return_from_exit
                                 → switch_to_kernel(saved_eip, saved_esp)
                                 → mov esp; jmp eip  (same-ring, no iret)
      → sti (restore IF cleared by the interrupt gate)
      → return to shell
```

The user program is a hand-assembled blob — 22 bytes of x86 machine code — that calls `write("hello from ring 3\n")` then `exit(0)`. Absolute addresses are baked in at the user code base (`0x40000000`) so the `mov $0x40000040, %ebx` for the message pointer resolves without relocation.

---

## Versioning

Each version is a milestone and a git tag.

| Version | Milestone |
|---------|-----------|
| **v0.1** | Boots, prints to VGA, keyboard echoes |
| **v0.2** | Hardware cursor, line buffer, shell loop, backspace |
| **v0.3** | `libk` strings, `kprintf`, cleaner shell |
| **v0.4** | Serial mirror, PIT timer, `sleep` / `uptime` |
| **v0.5a** | Multiboot info passed to kernel, `panic`, `meminfo`, `mmap` |
| **v0.5b** | Page directory + tables built (PG off), self test |
| **v0.5c** | Paging enabled, 16 MiB identity-mapped |
| **v0.6a** | Bump allocator, `heaptest` |
| **v0.6b** | Free-list allocator with splitting + coalescing |
| **v0.7a** | TSS, user segments, `int 0x80` gate (ring 0 only) |
| **v0.7b** | **Ring 3 execution, real syscalls, clean ring transitions** |
| **v0.8** | Round-robin scheduler, kernel threads |
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
[x] Heap allocator (bump, then free-list)
[x] TSS + ring 3
[x] System calls
[x] Exception handlers with diagnostic output
[ ] More syscalls (read, mmap, yield, getpid)
[ ] User program loader from disk
[ ] Multitasking (kernel threads, then user processes)
[ ] Filesystem
[ ] Userspace shell
```

---

## Design notes

Deliberate choices. Future-me should not undo these.

- **Serial mirror is default-on.** `kprintf` writes to VGA *and* COM1. When the MMU breaks and VGA dies, serial is the only way to see what happened. This is why it's wired into the formatter rather than bolted on later.

- **The IRQ handlers are tiny.** They do the minimum work and defer everything else. `irq1_c_handler` pushes a scancode and returns; the shell does the echoing. This is the correct design and gets more important once scheduling arrives.

- **Paging is identity-mapped for kernel space.** `0x00000000`–`0x00FFFFFF` map to themselves. Non-identity mappings start at `0x40000000` for user code and `0x40100000` for user stack. The identity map means a kernel virtual address is also its physical address, which is the simplest possible setup that still proves the MMU works.

- **User pages are mapped with `PAGE_USER` on both PDE and PTE.** If either is missing, ring 3 page-faults on the first instruction fetch. The `paging_map_user` debug output prints `dir_idx`/`tab_idx` so you can verify the mapping.

- **The `P` canary.** When `CR0.PG` is flipped, the very next asm block does `out 'P', 0x3F8`. If you see `P` on serial, the CPU survived the flip. If you don't, it triple-faulted. One byte of diagnostic can save hours of guessing.

- **`switch_to_kernel` uses `mov esp; jmp eip`, not `iret`.** Same-ring transitions should not use `iret` — `iret` pops 5 values for cross-ring but only 3 for same-ring, and getting this wrong leaves the stack 8 bytes off. `mov esp; jmp eip` is unambiguous.

- **Interrupt gates clear `IF`.** `int 0x80` uses `0xEE` (interrupt gate), which clears `IF` on entry. `switch_to_kernel` doesn't restore it, so `usermode_run` does `sti` after `switch_to_user` returns. Without this, the keyboard is dead after `usermode` — the IRQs never fire.

- **Alignment guarantees must be explicit.** `kmalloc` returns 16-byte aligned; `kmalloc_page` returns 4 KiB aligned. `paging_map_user` requires 4 KiB alignment on both virt and phys. Using `kmalloc` where `kmalloc_page` is required silently corrupts page tables. When you add an alignment-aware helper, grep for every call site that should use it.

- **No libc, no stdarg.h.** `kprintf` uses `__builtin_va_list` and `libk` provides everything else. The entire toolchain dependency is `gcc -ffreestanding -nostdlib -lgcc`.

---

## License

MIT. See `LICENSE`.

---

<div align="center">

*stop scrolling man*

</div>
