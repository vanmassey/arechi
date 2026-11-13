#include "arch.h"

/*
 * Limine transfers control to the ELF entry point after setting up the
 * initial execution environment. Keep this entry point freestanding and
 * independent of any runtime or global constructors.
 */
__attribute__((used, noreturn))
void _start(void)
{
    x86_64_halt_forever();
}
