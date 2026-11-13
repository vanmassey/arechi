#include "heap.h"

#include <stdint.h>

#include "hhdm.h"
#include "pmm.h"

#define HEAP_ALIGNMENT 16U
#define HEAP_PAGE_SIZE 4096U
#define HEAP_MIN_ARENA_PAGES 4U
#define HEAP_BLOCK_MAGIC UINT64_C(0x4152454348494842)

struct __attribute__((aligned(HEAP_ALIGNMENT))) heap_block {
    size_t size;
    struct heap_block *previous;
    struct heap_block *next;
    void *arena_base;
    size_t arena_pages;
    uint64_t magic;
    bool free;
};

static struct heap_block *block_list;
static bool initialized;

static bool align_size(size_t size, size_t *aligned)
{
    if (size > SIZE_MAX - (HEAP_ALIGNMENT - 1U)) {
        return false;
    }

    *aligned = (size + HEAP_ALIGNMENT - 1U) &
               ~(size_t)(HEAP_ALIGNMENT - 1U);
    return true;
}

static bool calculate_arena_pages(size_t required, size_t *page_count)
{
    if (required > SIZE_MAX - sizeof(struct heap_block)) {
        return false;
    }

    size_t total = required + sizeof(struct heap_block);
    size_t pages = total / HEAP_PAGE_SIZE;

    if (total % HEAP_PAGE_SIZE != 0) {
        ++pages;
    }

    if (pages < HEAP_MIN_ARENA_PAGES) {
        pages = HEAP_MIN_ARENA_PAGES;
    }

    if (pages > SIZE_MAX / HEAP_PAGE_SIZE) {
        return false;
    }

    *page_count = pages;
    return true;
}

static void append_block(struct heap_block *block)
{
    block->previous = NULL;
    block->next = block_list;

    if (block_list != NULL) {
        block_list->previous = block;
    }

    block_list = block;
}

static void remove_block(struct heap_block *block)
{
    if (block->previous != NULL) {
        block->previous->next = block->next;
    } else {
        block_list = block->next;
    }

    if (block->next != NULL) {
        block->next->previous = block->previous;
    }

    block->previous = NULL;
    block->next = NULL;
}

static bool add_arena(size_t required)
{
    size_t page_count;
    if (!calculate_arena_pages(required, &page_count)) {
        return false;
    }

    void *memory = pmm_alloc_pages_zeroed(page_count);
    if (memory == NULL) {
        return false;
    }

    struct heap_block *block = (struct heap_block *)memory;
    block->size = page_count * HEAP_PAGE_SIZE - sizeof(*block);
    block->arena_base = memory;
    block->arena_pages = page_count;
    block->magic = HEAP_BLOCK_MAGIC;
    block->free = true;
    append_block(block);
    return true;
}

bool heap_init(void)
{
    if (initialized || !hhdm_is_ready() || pmm_free_bytes() == 0) {
        return false;
    }

    block_list = NULL;
    initialized = true;
    return true;
}

void *kmalloc(size_t size)
{
    if (!initialized || size == 0) {
        return NULL;
    }

    size_t aligned_size;
    if (!align_size(size, &aligned_size)) {
        return NULL;
    }

    for (;;) {
        struct heap_block *block = block_list;

        while (block != NULL) {
            if (block->magic == HEAP_BLOCK_MAGIC && block->free &&
                block->size >= aligned_size) {
                const size_t remaining = block->size - aligned_size;

                if (remaining >= sizeof(struct heap_block) +
                                 HEAP_ALIGNMENT) {
                    struct heap_block *split =
                        (struct heap_block *)((uint8_t *)(block + 1) +
                                              aligned_size);

                    split->size = remaining - sizeof(*split);
                    split->arena_base = block->arena_base;
                    split->arena_pages = block->arena_pages;
                    split->magic = HEAP_BLOCK_MAGIC;
                    split->free = true;
                    split->previous = block;
                    split->next = block->next;

                    if (split->next != NULL) {
                        split->next->previous = split;
                    }

                    block->next = split;
                    block->size = aligned_size;
                }

                block->free = false;
                return (void *)(block + 1);
            }

            block = block->next;
        }

        if (!add_arena(aligned_size)) {
            return NULL;
        }
    }
}

void *kcalloc(size_t count, size_t size)
{
    if (count == 0 || size == 0 || count > SIZE_MAX / size) {
        return NULL;
    }

    const size_t total = count * size;
    void *memory = kmalloc(total);
    if (memory == NULL) {
        return NULL;
    }

    uint8_t *bytes = (uint8_t *)memory;
    for (size_t i = 0; i < total; ++i) {
        bytes[i] = 0;
    }

    return memory;
}

void kfree(void *pointer)
{
    if (!initialized || pointer == NULL) {
        return;
    }

    /* Find the exact payload pointer before reading any candidate header. */
    struct heap_block *block = block_list;
    while (block != NULL && (void *)(block + 1) != pointer) {
        block = block->next;
    }

    if (block == NULL || block->magic != HEAP_BLOCK_MAGIC || block->free) {
        return;
    }

    block->free = true;

    if (block->previous != NULL &&
        block->previous->arena_base == block->arena_base &&
        block->previous->free) {
        struct heap_block *previous = block->previous;
        previous->size += sizeof(*block) + block->size;
        previous->next = block->next;

        if (block->next != NULL) {
            block->next->previous = previous;
        }

        block->magic = 0;
        block = previous;
    }

    if (block->next != NULL &&
        block->next->arena_base == block->arena_base &&
        block->next->free) {
        struct heap_block *next = block->next;
        block->size += sizeof(*next) + next->size;
        block->next = next->next;

        if (next->next != NULL) {
            next->next->previous = block;
        }

        next->magic = 0;
    }

    /* Return a completely coalesced arena to the physical page allocator. */
    for (struct heap_block *other = block_list;
         other != NULL; other = other->next) {
        if (other != block && other->arena_base == block->arena_base) {
            return;
        }
    }

    void *arena_base = block->arena_base;
    const size_t arena_pages = block->arena_pages;

    if (pmm_free_pages_virtual(arena_base, arena_pages)) {
        remove_block(block);
    }
}

bool heap_self_test(void)
{
    if (!initialized) {
        return false;
    }

    const uint64_t free_before = pmm_free_bytes();
    uint8_t *small = (uint8_t *)kmalloc(37);
    uint8_t *large = (uint8_t *)kmalloc(4097);
    uint64_t *zeroed = (uint64_t *)kcalloc(32, sizeof(uint64_t));

    bool passed = small != NULL && large != NULL && zeroed != NULL;

    if (passed) {
        passed = ((uintptr_t)small % HEAP_ALIGNMENT) == 0 &&
                 ((uintptr_t)large % HEAP_ALIGNMENT) == 0 &&
                 ((uintptr_t)zeroed % HEAP_ALIGNMENT) == 0;
    }

    if (passed) {
        for (size_t i = 0; i < 37; ++i) {
            small[i] = 0xA5U;
        }

        large[0] = 0x5AU;
        large[4096] = 0xC3U;

        for (size_t i = 0; i < 32; ++i) {
            if (zeroed[i] != 0) {
                passed = false;
                break;
            }
        }

        if (small[0] != 0xA5U || small[36] != 0xA5U ||
            large[0] != 0x5AU || large[4096] != 0xC3U) {
            passed = false;
        }
    }

    kfree(large);
    kfree(small);
    kfree(zeroed);

    return passed && pmm_free_bytes() == free_before;
}
