#ifndef ARECHI_PMM_H
#define ARECHI_PMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct limine_memmap_response;

/*
 * Initialise the early physical page allocator from Limine's memory map.
 * Only LIMINE_MEMMAP_USABLE ranges are added.
 */
bool pmm_init(const struct limine_memmap_response *memory_map);

/* Allocate contiguous 4 KiB pages. Returns a physical address, or zero. */
uint64_t pmm_alloc_pages(size_t page_count);

uint64_t pmm_total_bytes(void);
uint64_t pmm_free_bytes(void);

#endif
