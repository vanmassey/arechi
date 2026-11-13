#include "interrupts.h"

#include <stddef.h>
#include <stdint.h>

#include "drivers/fb.h"
#include "drivers/serial.h"

#define IDT_ENTRIES 256
#define EXCEPTION_COUNT 32
#define IDT_INTERRUPT_GATE 0x8E

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_register {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct interrupt_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t instruction_pointer;
    uint64_t code_segment;
    uint64_t flags;
};

static struct idt_entry idt[IDT_ENTRIES];

extern void (*isr_stub_table[EXCEPTION_COUNT])(void);

static void idt_set_gate(uint8_t vector, uintptr_t handler, uint16_t selector)
{
    struct idt_entry *entry = &idt[vector];

    entry->offset_low = (uint16_t)(handler & 0xFFFF);
    entry->selector = selector;
    entry->ist = vector == 8 ? 1 : 0;
    entry->attributes = IDT_INTERRUPT_GATE;
    entry->offset_middle = (uint16_t)((handler >> 16) & 0xFFFF);
    entry->offset_high = (uint32_t)(handler >> 32);
    entry->reserved = 0;
}

void interrupts_init(void)
{
    uint16_t code_selector;

    __asm__ volatile ("mov %%cs, %0" : "=r"(code_selector));

    for (size_t i = 0; i < IDT_ENTRIES; ++i) {
        idt[i] = (struct idt_entry){0};
    }

    for (uint8_t vector = 0; vector < EXCEPTION_COUNT; ++vector) {
        idt_set_gate(vector, (uintptr_t)isr_stub_table[vector], code_selector);
    }

    struct idt_register idtr = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uint64_t)(uintptr_t)idt
    };

    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");
}

static void write_hex(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char output[19] = "0x0000000000000000";

    for (unsigned int i = 0; i < 16; ++i) {
        unsigned int shift = (15U - i) * 4U;
        output[2 + i] = digits[(value >> shift) & 0xF];
    }

    serial_write(output);
    fb_write(output);
}

__attribute__((noreturn))
void interrupt_exception_handler(const struct interrupt_frame *frame)
{
    serial_write("\nARECHI: CPU EXCEPTION\nVector: ");
    fb_write("\nARECHI: CPU EXCEPTION\nVector: ");
    write_hex(frame->vector);

    serial_write("\nError code: ");
    fb_write("\nError code: ");
    write_hex(frame->error_code);

    serial_write("\nInstruction pointer: ");
    fb_write("\nInstruction pointer: ");
    write_hex(frame->instruction_pointer);

    serial_write("\nSystem halted.\n");
    fb_write("\nSystem halted.\n");

    __asm__ volatile ("cli" : : : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
