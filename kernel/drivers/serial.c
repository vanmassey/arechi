#include "serial.h"

#include <stdint.h>

#define COM1_BASE 0x3F8

static bool serial_available;

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

bool serial_init(void)
{
    outb(COM1_BASE + 1, 0x00); /* Disable UART interrupts. */
    outb(COM1_BASE + 3, 0x80); /* Enable divisor latch. */
    outb(COM1_BASE + 0, 0x03); /* 38400 baud, divisor 3. */
    outb(COM1_BASE + 1, 0x00);
    outb(COM1_BASE + 3, 0x03); /* 8 data bits, no parity, one stop bit. */
    outb(COM1_BASE + 2, 0xC7); /* Enable and clear FIFO. */
    outb(COM1_BASE + 4, 0x0B); /* Set DTR, RTS and OUT2. */

    serial_available = inb(COM1_BASE + 5) != 0xFF;
    return serial_available;
}

void serial_write_char(char character)
{
    if (!serial_available) {
        return;
    }

    while ((inb(COM1_BASE + 5) & 0x20) == 0) {
        __asm__ volatile ("pause");
    }

    outb(COM1_BASE, (uint8_t)character);
}

void serial_write(const char *text)
{
    if (text == 0 || !serial_available) {
        return;
    }

    while (*text != '\0') {
        if (*text == '\n') {
            serial_write_char('\r');
        }

        serial_write_char(*text);
        ++text;
    }
}
