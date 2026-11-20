#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_ALIGN 16

void   heap_init(void);
void  *kmalloc(size_t size);
void  *kmalloc_page(void);
void   kfree(void *ptr);
size_t heap_used(void);
size_t heap_capacity(void);
void  *heap_start(void);
void  *heap_current(void);
void   heap_dump(void);

#endif
