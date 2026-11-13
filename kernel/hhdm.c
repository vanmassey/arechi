#include "hhdm.h"

#include "limine.h"

static uint64_t direct_map_offset;
static bool initialized;

bool hhdm_init(const struct limine_hhdm_response *response)
{
    if (initialized || response == 0 || response->offset == 0) {
        return false;
    }

    direct_map_offset = response->offset;
    initialized = true;
    return true;
}

bool hhdm_is_ready(void)
{
    return initialized;
}

uint64_t hhdm_get_offset(void)
{
    return initialized ? direct_map_offset : 0;
}

void *hhdm_phys_to_virt(uint64_t physical_address)
{
    if (!initialized || physical_address > UINT64_MAX - direct_map_offset) {
        return 0;
    }

    return (void *)(uintptr_t)(direct_map_offset + physical_address);
}

uint64_t hhdm_virt_to_phys(const void *virtual_address)
{
    const uint64_t address = (uint64_t)(uintptr_t)virtual_address;

    if (!initialized || address < direct_map_offset) {
        return UINT64_MAX;
    }

    return address - direct_map_offset;
}
