#include "gdt.h"

#include <stddef.h>
#include <stdint.h>

#define GDT_KERNEL_CODE 0x00AF9A000000FFFFULL
#define GDT_KERNEL_DATA 0x00CF92000000FFFFULL
#define GDT_TSS_SELECTOR 0x18
#define STACK_SIZE 16384

struct task_state_segment {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed));

struct gdt_register {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static uint64_t gdt[5] __attribute__((aligned(16)));
static struct task_state_segment tss __attribute__((aligned(16)));
static uint8_t privilege_stack[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t double_fault_stack[STACK_SIZE] __attribute__((aligned(16)));

extern void gdt_load(const struct gdt_register *gdtr);

void gdt_init(void)
{
    const uintptr_t tss_base = (uintptr_t)&tss;
    const uint64_t tss_limit = sizeof(tss) - 1;

    gdt[0] = 0;
    gdt[1] = GDT_KERNEL_CODE;
    gdt[2] = GDT_KERNEL_DATA;
    gdt[3] = (tss_limit & 0xFFFF)
           | ((uint64_t)(tss_base & 0xFFFFFF) << 16)
           | (0x89ULL << 40)
           | ((tss_limit & 0xF0000) << 32)
           | ((uint64_t)(tss_base & 0xFF000000) << 32);
    gdt[4] = (uint64_t)(tss_base >> 32);

    tss.rsp0 = (uint64_t)(uintptr_t)(privilege_stack + sizeof(privilege_stack));
    tss.ist1 = (uint64_t)(uintptr_t)(double_fault_stack + sizeof(double_fault_stack));
    tss.io_map_base = sizeof(tss);

    const struct gdt_register gdtr = {
        .limit = (uint16_t)(sizeof(gdt) - 1),
        .base = (uint64_t)(uintptr_t)gdt
    };

    gdt_load(&gdtr);

    const uint16_t tss_selector = GDT_TSS_SELECTOR;
    __asm__ volatile ("ltr %0" : : "r"(tss_selector) : "memory");
}
