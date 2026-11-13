#include "pic.h"

#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define PIC_ICW1_INIT 0x10
#define PIC_ICW1_ICW4 0x01
#define PIC_ICW4_8086 0x01
#define PIC_READ_ISR  0x0B

static uint8_t master_mask = 0xFF;
static uint8_t slave_mask = 0xFF;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(void)
{
    outb(PIC1_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086);
    io_wait();

    master_mask = 0xFF;
    slave_mask = 0xFF;
    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

void pic_set_mask(uint8_t irq)
{
    if (irq >= 16) {
        return;
    }

    if (irq < 8) {
        master_mask |= (uint8_t)(1U << irq);
        outb(PIC1_DATA, master_mask);
    } else {
        slave_mask |= (uint8_t)(1U << (irq - 8));
        outb(PIC2_DATA, slave_mask);
    }
}

void pic_clear_mask(uint8_t irq)
{
    if (irq >= 16) {
        return;
    }

    if (irq < 8) {
        master_mask &= (uint8_t)~(1U << irq);
        outb(PIC1_DATA, master_mask);
    } else {
        slave_mask &= (uint8_t)~(1U << (irq - 8));
        outb(PIC2_DATA, slave_mask);
    }
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

int pic_is_spurious(uint8_t irq)
{
    if (irq == 7) {
        outb(PIC1_COMMAND, PIC_READ_ISR);
        return (inb(PIC1_COMMAND) & 0x80) == 0;
    }

    if (irq == 15) {
        outb(PIC2_COMMAND, PIC_READ_ISR);
        if ((inb(PIC2_COMMAND) & 0x80) == 0) {
            outb(PIC1_COMMAND, PIC_EOI);
            return 1;
        }
    }

    return 0;
}
