#ifndef ARECHI_HHDM_H
#define ARECHI_HHDM_H

#include <stdbool.h>
#include <stdint.h>

struct limine_hhdm_response;

/* Store the direct-map offset supplied by Limine. */
bool hhdm_init(const struct limine_hhdm_response *response);
bool hhdm_is_ready(void);
uint64_t hhdm_get_offset(void);

/* Convert addresses only after HHDM initialisation. */
void *hhdm_phys_to_virt(uint64_t physical_address);
uint64_t hhdm_virt_to_phys(const void *virtual_address);

#endif
