#ifndef ARECHI_IRQ_H
#define ARECHI_IRQ_H

#include <stdint.h>

typedef void (*irq_handler_t)(void);

void irq_init(void);
int irq_register(uint8_t irq, irq_handler_t handler);
void irq_unregister(uint8_t irq);
void irq_enable(uint8_t irq);
void irq_disable(uint8_t irq);
void irq_dispatch(uint64_t vector);

#endif
