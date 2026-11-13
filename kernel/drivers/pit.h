#ifndef ARECHI_PIT_H
#define ARECHI_PIT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Configure the PIT channel 0 and register IRQ0.
 * The IRQ remains masked until pit_start() is called.
 */
bool pit_init(uint32_t frequency_hz);

/* Unmask IRQ0. CPU interrupts must still be enabled separately. */
void pit_start(void);
void pit_stop(void);

/* Number of timer interrupts received since initialisation. */
uint64_t pit_get_ticks(void);

/* Requested timer frequency, or zero before successful initialisation. */
uint32_t pit_get_frequency(void);

#endif
