#include "pmm.h"

#include "limine.h"
#include "hhdm.h"

#define PMM_PAGE_SIZE 4096ULL
#define PMM_MAX_REGIONS 256U
#define PMM_MAX_BLOCKS 4096U

struct pmm_region {
    uint64_t base;
    uint64_t end;
    uint64_t cursor;
};

struct pmm_block {
    uint64_t base;
    uint64_t page_count;
    bool used;
    bool free;
};

static struct pmm_region regions[PMM_MAX_REGIONS];
static struct pmm_block blocks[PMM_MAX_BLOCKS];
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

static size_t find_unused_block(void)
{
    for (size_t i = 0; i < PMM_MAX_BLOCKS; ++i) {
        if (!blocks[i].used) {
            return i;
        }
    }

    return PMM_MAX_BLOCKS;
}

static void merge_free_blocks(void)
{
    bool merged;

    do {
        merged = false;

        for (size_t i = 0; i < PMM_MAX_BLOCKS && !merged; ++i) {
            if (!blocks[i].used || !blocks[i].free) {
                continue;
            }

            const uint64_t i_bytes = blocks[i].page_count * PMM_PAGE_SIZE;
            if (blocks[i].base > UINT64_MAX - i_bytes) {
                continue;
            }

            const uint64_t i_end = blocks[i].base + i_bytes;

            for (size_t j = i + 1; j < PMM_MAX_BLOCKS; ++j) {
                if (!blocks[j].used || !blocks[j].free) {
                    continue;
                }

                const uint64_t j_bytes =
                    blocks[j].page_count * PMM_PAGE_SIZE;

                if (blocks[j].base <= UINT64_MAX - j_bytes &&
                    i_end == blocks[j].base) {
                    blocks[i].page_count += blocks[j].page_count;
                    blocks[j].used = false;
                    merged = true;
                    break;
                }

                if (blocks[j].base <= UINT64_MAX - j_bytes &&
                    blocks[j].base + j_bytes == blocks[i].base) {
                    blocks[j].page_count += blocks[i].page_count;
                    blocks[i].used = false;
                    merged = true;
                    break;
                }
            }
        }
    } while (merged);
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

        /* Physical address zero is reserved as the allocation-failure value. */
        if (start == 0) {
            start = PMM_PAGE_SIZE;
        }

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

    /* Reuse a previously freed extent before consuming new memory. */
    for (size_t i = 0; i < PMM_MAX_BLOCKS; ++i) {
        struct pmm_block *block = &blocks[i];

        if (!block->used || !block->free ||
            block->page_count < (uint64_t)page_count) {
            continue;
        }

        if (block->page_count == (uint64_t)page_count) {
            block->free = false;
            free_bytes -= bytes;
            return block->base;
        }

        const size_t slot = find_unused_block();
        if (slot == PMM_MAX_BLOCKS) {
            return 0;
        }

        const uint64_t physical_address = block->base;
        block->base += bytes;
        block->page_count -= (uint64_t)page_count;

        blocks[slot] = (struct pmm_block) {
            .base = physical_address,
            .page_count = (uint64_t)page_count,
            .used = true,
            .free = false
        };

        free_bytes -= bytes;
        return physical_address;
    }

    /* Allocate from the unused tail of a usable memory region. */
    for (size_t i = 0; i < region_count; ++i) {
        struct pmm_region *region = &regions[i];

        if (region->cursor > region->end ||
            bytes > region->end - region->cursor) {
            continue;
        }

        const size_t slot = find_unused_block();
        if (slot == PMM_MAX_BLOCKS) {
            return 0;
        }

        const uint64_t physical_address = region->cursor;
        region->cursor += bytes;

        blocks[slot] = (struct pmm_block) {
            .base = physical_address,
            .page_count = (uint64_t)page_count,
            .used = true,
            .free = false
        };

        free_bytes -= bytes;
        return physical_address;
    }

    return 0;
}

bool pmm_free_pages(uint64_t physical_address, size_t page_count)
{
    if (!initialized || physical_address == 0 || page_count == 0 ||
        page_count > UINT64_MAX / PMM_PAGE_SIZE) {
        return false;
    }

    const uint64_t bytes = (uint64_t)page_count * PMM_PAGE_SIZE;

    for (size_t i = 0; i < PMM_MAX_BLOCKS; ++i) {
        struct pmm_block *block = &blocks[i];

        if (!block->used || block->free ||
            block->base != physical_address ||
            block->page_count != (uint64_t)page_count) {
            continue;
        }

        if (bytes > total_bytes || free_bytes > total_bytes - bytes) {
            return false;
        }

        block->free = true;
        free_bytes += bytes;
        merge_free_blocks();
        return true;
    }

    return false;
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

    void *virtual_address = hhdm_phys_to_virt(physical_address);
    if (virtual_address == 0) {
        (void)pmm_free_pages(physical_address, page_count);
        return 0;
    }

    return virtual_address;
}

void *pmm_alloc_pages_zeroed(size_t page_count)
{
    if (page_count == 0 ||
        page_count > UINT64_MAX / PMM_PAGE_SIZE) {
        return 0;
    }

    void *memory = pmm_alloc_pages_virtual(page_count);
    if (memory == 0) {
        return 0;
    }

    uint8_t *bytes = (uint8_t *)memory;
    const uint64_t byte_count = (uint64_t)page_count * PMM_PAGE_SIZE;

    for (uint64_t i = 0; i < byte_count; ++i) {
        bytes[i] = 0;
    }

    return memory;
}
