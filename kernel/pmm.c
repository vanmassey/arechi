#include "pmm.h"

#include "limine.h"
#include "hhdm.h"

#define PMM_PAGE_SIZE 4096ULL
#define PMM_MAX_REGIONS 256U

struct pmm_region {
    uint64_t base;
    uint64_t end;
    uint64_t cursor;
};

static struct pmm_region regions[PMM_MAX_REGIONS];
static size_t region_count;
static uint64_t total_bytes;
static uint64_t free_bytes;
static bool initialized;

static bool align_up_page(uint64_t value, uint64_t *aligned)
{
    if (value > UINT64_MAX - (PMM_PAGE_SIZE - 1)) {
        return false;
    }

    *aligned = (value + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1);
    return true;
}

bool pmm_init(const struct limine_memmap_response *memory_map)
{
    if (initialized || memory_map == NULL ||
        memory_map->entries == NULL || memory_map->entry_count == 0) {
        return false;
    }

    size_t usable_regions = 0;
    uint64_t usable_bytes = 0;

    for (uint64_t i = 0; i < memory_map->entry_count; ++i) {
        const struct limine_memmap_entry *entry = memory_map->entries[i];

        if (entry == NULL || entry->type != LIMINE_MEMMAP_USABLE ||
            entry->length == 0 || entry->base > UINT64_MAX - entry->length) {
            continue;
        }

        uint64_t start;
        if (!align_up_page(entry->base, &start)) {
            continue;
        }

        const uint64_t end =
            (entry->base + entry->length) & ~(PMM_PAGE_SIZE - 1);

        if (start >= end) {
            continue;
        }

        if (usable_regions >= PMM_MAX_REGIONS) {
            return false;
        }

        const uint64_t length = end - start;
        if (usable_bytes > UINT64_MAX - length) {
            return false;
        }

        regions[usable_regions++] = (struct pmm_region) {
            .base = start,
            .end = end,
            .cursor = start
        };
        usable_bytes += length;
    }

    if (usable_regions == 0 || usable_bytes == 0) {
        return false;
    }

    region_count = usable_regions;
    total_bytes = usable_bytes;
    free_bytes = usable_bytes;
    initialized = true;
    return true;
}

uint64_t pmm_alloc_pages(size_t page_count)
{
    if (!initialized || page_count == 0 ||
        page_count > UINT64_MAX / PMM_PAGE_SIZE) {
        return 0;
    }

    const uint64_t bytes = (uint64_t)page_count * PMM_PAGE_SIZE;

    for (size_t i = 0; i < region_count; ++i) {
        struct pmm_region *region = &regions[i];

        if (region->cursor > region->end ||
            bytes > region->end - region->cursor) {
            continue;
        }

        const uint64_t physical_address = region->cursor;
        region->cursor += bytes;
        free_bytes -= bytes;
        return physical_address;
    }

    return 0;
}

uint64_t pmm_total_bytes(void)
{
    return total_bytes;
}

uint64_t pmm_free_bytes(void)
{
    return free_bytes;
}

void *pmm_alloc_pages_virtual(size_t page_count)
{
    if (!hhdm_is_ready()) {
        return 0;
    }

    const uint64_t physical_address = pmm_alloc_pages(page_count);

    if (physical_address == 0) {
        return 0;
    }

    return hhdm_phys_to_virt(physical_address);
}
