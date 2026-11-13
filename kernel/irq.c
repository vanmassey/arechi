#include "irq.h"

#include <stddef.h>

#include "pic.h"

#define IRQ_COUNT 16
#define IRQ_VECTOR_BASE 32

static irq_handler_t handlers[IRQ_COUNT];

void irq_init(void)
{
    for (size_t i = 0; i < IRQ_COUNT; ++i) {
        handlers[i] = NULL;
    }

    pic_init();
}

int irq_register(uint8_t irq, irq_handler_t handler)
{
    if (irq >= IRQ_COUNT || handler == NULL || handlers[irq] != NULL) {
        return -1;
    }

    handlers[irq] = handler;
    return 0;
}

void irq_unregister(uint8_t irq)
{
    if (irq >= IRQ_COUNT) {
        return;
    }

    pic_set_mask(irq);
    handlers[irq] = NULL;
}

void irq_enable(uint8_t irq)
{
    if (irq < IRQ_COUNT && handlers[irq] != NULL) {
        pic_clear_mask(irq);
    }
}

void irq_disable(uint8_t irq)
{
    if (irq < IRQ_COUNT) {
        pic_set_mask(irq);
    }
}

void irq_dispatch(uint64_t vector)
{
    if (vector < IRQ_VECTOR_BASE || vector >= IRQ_VECTOR_BASE + IRQ_COUNT) {
        return;
    }

    const uint8_t irq = (uint8_t)(vector - IRQ_VECTOR_BASE);

    if (pic_is_spurious(irq)) {
        return;
    }

    if (handlers[irq] != NULL) {
        handlers[irq]();
    }

    pic_send_eoi(irq);
}
