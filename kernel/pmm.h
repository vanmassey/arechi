#ifndef ARECHI_PMM_H
#define ARECHI_PMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct limine_memmap_response;

bool pmm_init(const struct limine_memmap_response *memory_map);

/* Allocate contiguous 4 KiB pages. Returns a physical address, or zero. */
uint64_t pmm_alloc_pages(size_t page_count);

/* Free an exact allocation returned by pmm_alloc_pages(). */
bool pmm_free_pages(uint64_t physical_address, size_t page_count);

/* Free an exact allocation returned as an HHDM virtual address. */
bool pmm_free_pages_virtual(void *virtual_address, size_t page_count);

/* Allocate pages and return their HHDM virtual address, or NULL. */
void *pmm_alloc_pages_virtual(size_t page_count);

/* Allocate contiguous pages, clear them, and return their HHDM address. */
void *pmm_alloc_pages_zeroed(size_t page_count);

uint64_t pmm_total_bytes(void);
uint64_t pmm_free_bytes(void);

#endif
