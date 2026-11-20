#include "heap.h"
#include "paging.h"
#include "panic.h"
#include "stdio.h"
#include "string.h"
#include "serial.h"

extern char __kernel_end;

#define HEAP_END_ADDR  (IDENTITY_MAP_SIZE)
#define BLOCK_OVERHEAD 32

typedef struct block {
    uint32_t      size;
    uint32_t      prev_size;
    uint32_t      free;
    struct block *next;
    struct block *prev;
} __attribute__((packed)) block_t;

static block_t *free_head = 0;
static uintptr_t heap_base = 0;
static uintptr_t heap_cur  = 0;
static uintptr_t heap_end  = 0;
static size_t    used_bytes = 0;

static inline uintptr_t align_up(uintptr_t v, uintptr_t a) {
    return (v + a - 1) & ~(a - 1);
}

static inline block_t *block_at(uintptr_t addr) {
    return (block_t *)addr;
}

static inline uintptr_t next_block_addr(block_t *b) {
    return (uintptr_t)b + b->size;
}

static inline block_t *next_block(block_t *b) {
    uintptr_t n = next_block_addr(b);
    if (n >= heap_end) return 0;
    return block_at(n);
}

static inline block_t *prev_block(block_t *b) {
    if (b->prev_size == 0) return 0;
    return block_at((uintptr_t)b - b->prev_size);
}

static void write_header(block_t *b, uint32_t size, uint32_t prev_size, int is_free) {
    b->size      = size;
    b->prev_size = prev_size;
    b->free      = is_free ? 1u : 0u;
    b->next      = 0;
    b->prev      = 0;
}

static void free_list_push(block_t *b) {
    b->free = 1;
    b->prev = 0;
    b->next = free_head;
    if (free_head) free_head->prev = b;
    free_head = b;
}

static void free_list_remove(block_t *b) {
    if (b->prev) b->prev->next = b->next;
    else         free_head = b->next;
    if (b->next) b->next->prev = b->prev;
    b->prev = 0;
    b->next = 0;
}

static void split_block(block_t *b, uint32_t want) {
    ASSERT(want >= BLOCK_OVERHEAD);
    ASSERT(b->size >= want);
    ASSERT(b->free == 0);

    uint32_t remain = b->size - want;
    if (remain < BLOCK_OVERHEAD + HEAP_ALIGN) return;

    block_t *tail = block_at((uintptr_t)b + want);
    write_header(tail, remain, want, 1);

    block_t *after = next_block(tail);
    if (after) after->prev_size = remain;

    b->size = want;
    free_list_push(tail);
}

static block_t *coalesce(block_t *b) {
    ASSERT(b->free == 1);

    block_t *after = next_block(b);
    if (after && after->free) {
        free_list_remove(after);
        b->size += after->size;
        block_t *after2 = next_block(b);
        if (after2) after2->prev_size = b->size;
    }

    block_t *before = prev_block(b);
    if (before && before->free) {
        free_list_remove(before);
        before->size += b->size;
        block_t *after3 = next_block(before);
        if (after3) after3->prev_size = before->size;
        return before;
    }

    return b;
}

void heap_init(void) {
    heap_base = align_up((uintptr_t)&__kernel_end, HEAP_ALIGN);
    heap_end  = HEAP_END_ADDR;

    if (heap_end <= heap_base + BLOCK_OVERHEAD + HEAP_ALIGN) {
        panic("heap: region too small");
    }

    block_t *first = block_at(heap_base);
    uint32_t size  = (uint32_t)(heap_end - heap_base);
    write_header(first, size, 0, 1);

    free_head  = first;
    heap_cur   = heap_base;
    used_bytes = 0;
}

static void *kmalloc_internal(size_t size, int require_page_align) {
    ASSERT(size > 0);

    uint32_t align = require_page_align ? PAGE_SIZE : HEAP_ALIGN;
    uint32_t need  = (uint32_t)align_up(size, align);
    if (need < size) panic("kmalloc: size overflow");
    if (need < align) need = align;

    block_t *b = free_head;
    while (b) {
        uintptr_t hdr = (uintptr_t)b;
        uintptr_t content = hdr + BLOCK_OVERHEAD;
        uintptr_t aligned_content = content;

        if (require_page_align) {
            aligned_content = align_up(content, PAGE_SIZE);
        }

        uintptr_t pad = aligned_content - content;
        uint32_t total = need + BLOCK_OVERHEAD + (uint32_t)pad;

        if (b->size >= total) {
            if (pad > 0) {
                ASSERT(pad >= BLOCK_OVERHEAD);
                ASSERT(b->size >= pad + BLOCK_OVERHEAD);

                block_t *slack = b;
                block_t *real = block_at(hdr + pad);
                uint32_t remaining = slack->size - (uint32_t)pad;
                write_header(real, remaining, (uint32_t)pad, 1);
                slack->size = (uint32_t)pad;
                slack->free = 1;
                free_list_push(slack);
                b = real;
            } else {
                free_list_remove(b);
            }

            b->free = 0;
            heap_cur = (uintptr_t)b + BLOCK_OVERHEAD;

            uint32_t want = need + BLOCK_OVERHEAD;
            if (b->size >= want + BLOCK_OVERHEAD + HEAP_ALIGN) {
                split_block(b, want);
            }

            used_bytes += b->size;

            void *result = (void *)((uintptr_t)b + BLOCK_OVERHEAD);
            ASSERT(((uintptr_t)result & (align - 1)) == 0);
            ASSERT((uintptr_t)result >= heap_base + BLOCK_OVERHEAD);
            ASSERT((uintptr_t)result < heap_end);

            return result;
        }
        b = b->next;
    }

    panic("kmalloc: out of heap");
    return 0;
}

void *kmalloc(size_t size) {
    return kmalloc_internal(size, 0);
}

void *kmalloc_page(void) {
    return kmalloc_internal(PAGE_SIZE, 1);
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_t *b = block_at((uintptr_t)ptr - BLOCK_OVERHEAD);

    ASSERT(b >= (block_t *)heap_base);
    ASSERT((uintptr_t)b < heap_end);
    ASSERT(((uintptr_t)b & (HEAP_ALIGN - 1)) == 0);
    ASSERT(b->free == 0);
    ASSERT(b->size >= BLOCK_OVERHEAD);
    ASSERT(used_bytes >= b->size);

    used_bytes -= b->size;
    b->free = 1;
    b = coalesce(b);
    free_list_push(b);
}

size_t heap_used(void) {
    return used_bytes;
}

size_t heap_capacity(void) {
    return (size_t)(heap_end - heap_base);
}

void *heap_start(void) {
    return (void *)heap_base;
}

void *heap_current(void) {
    return (void *)heap_cur;
}

void heap_dump(void) {
    kprintf("heap dump:\n");
    kprintf("  base=0x%x end=0x%x used=%u cap=%u\n",
            (uint32_t)heap_base, (uint32_t)heap_end,
            (uint32_t)used_bytes, (uint32_t)heap_capacity());

    uintptr_t addr = heap_base;
    int i = 0;
    while (addr < heap_end && i < 64) {
        block_t *b = block_at(addr);
        if (b->size == 0) { kprintf("  [%2d] zero-size block, stop\n", i); break; }
        kprintf("  [%2d] addr=0x%x size=%u prev=%u free=%u\n",
                i, (uint32_t)addr, b->size, b->prev_size, b->free);
        addr += b->size;
        i++;
    }
    if (addr < heap_end) kprintf("  ...\n");
}
