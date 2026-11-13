#ifndef ARECHI_HEAP_H
#define ARECHI_HEAP_H

#include <stdbool.h>
#include <stddef.h>

/* Initialise the kernel heap after the HHDM and PMM are ready. */
bool heap_init(void);

/* Allocate aligned kernel memory. Returns NULL on failure. */
void *kmalloc(size_t size);

/* Allocate count elements and clear the returned memory. */
void *kcalloc(size_t count, size_t size);

/* Release a pointer returned by kmalloc() or kcalloc(). */
void kfree(void *pointer);

#endif
