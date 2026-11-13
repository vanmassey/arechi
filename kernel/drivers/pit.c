#include "pit.h"

#include "../irq.h"

#define PIT_INPUT_HZ 1193182U
#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43
#define PIT_MODE_RATE     0x36
#define PIT_MIN_FREQUENCY 19U
#define PIT_MAX_FREQUENCY PIT_INPUT_HZ

static volatile uint64_t ticks;
static uint32_t configured_frequency;
static bool initialized;
static bool running;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void pit_irq_handler(void)
{
    ++ticks;
}

bool pit_init(uint32_t frequency_hz)
{
    if (initialized ||
        frequency_hz < PIT_MIN_FREQUENCY ||
        frequency_hz > PIT_MAX_FREQUENCY) {
        return false;
    }

    uint32_t divisor = PIT_INPUT_HZ / frequency_hz;
    if (divisor == 0 || divisor > 0xFFFFU) {
        return false;
    }

    /* Keep IRQ0 masked until the caller explicitly starts the timer. */
    irq_disable(0);
    if (irq_register(0, pit_irq_handler) != 0) {
        return false;
    }

    outb(PIT_COMMAND, PIT_MODE_RATE);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFFU));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFFU));

    ticks = 0;
    configured_frequency = frequency_hz;
    initialized = true;
    running = false;
    return true;
}

void pit_start(void)
{
    if (initialized && !running) {
        irq_enable(0);
        running = true;
    }
}

void pit_stop(void)
{
    if (initialized && running) {
        irq_disable(0);
        running = false;
    }
}

uint64_t pit_get_ticks(void)
{
    return ticks;
}

uint32_t pit_get_frequency(void)
{
    return configured_frequency;
}
