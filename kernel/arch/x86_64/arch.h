#ifndef ARECHI_X86_64_ARCH_H
#define ARECHI_X86_64_ARCH_H

#include <stdint.h>

static inline void x86_64_disable_interrupts(void)
{
    __asm__ volatile ("cli" ::: "memory");
}

static inline void x86_64_halt(void)
{
    __asm__ volatile ("hlt");
}

static inline void x86_64_halt_forever(void)
{
    x86_64_disable_interrupts();

    for (;;) {
        x86_64_halt();
    }
}

#endif
