#include "arch.h"

void kernel_main(void);

__attribute__((used, noreturn))
void _start(void)
{
    kernel_main();
}
