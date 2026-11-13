#include <stdbool.h>
#include <stdint.h>

#include "arch.h"
#include "drivers/fb.h"
#include "drivers/serial.h"
#include "gdt.h"
#include "interrupts.h"
#include "drivers/pit.h"
#include "limine.h"
#include "pmm.h"
#include "hhdm.h"
#include "heap.h"

__attribute__((used, section(".limine_requests_start")))
LIMINE_REQUESTS_START_MARKER

__attribute__((used, section(".limine_requests")))
LIMINE_BASE_REVISION(0)

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
LIMINE_REQUESTS_END_MARKER

static void write_both(const char *message)
{
    serial_write(message);
    fb_write(message);
}

__attribute__((used, noreturn))
void kernel_main(void)
{
    x86_64_disable_interrupts();

    bool serial_ready = serial_init();
    bool framebuffer_ready =
        fb_init(framebuffer_request.response, terminal_request.response);

    gdt_init();
    interrupts_init();
    write_both("GDT, TSS, IDT and masked PIC initialised; CPU exceptions are handled.\n");

    if (hhdm_init(hhdm_request.response)) {
        write_both("Higher Half Direct Map initialised.\n");
    } else {
        write_both("Higher Half Direct Map initialisation failed.\n");
    }

    if (pmm_init(memory_map_request.response)) {
        write_both("Physical memory map accepted; early page allocator ready.\n");
    } else {
        write_both("Physical memory allocator initialisation failed.\n");
    }

    if (heap_init()) {
        write_both("Kernel heap initialised; byte-sized allocations are available.\\n");

        if (heap_self_test()) {
            write_both("Kernel heap self-test passed: alignment, zeroing, data integrity and page reclamation.\\n");
        } else {
            write_both("Kernel heap self-test failed.\\n");
        }
    } else {
        write_both("Kernel heap initialisation failed; dynamic allocation is unavailable.\\n");
    }

    if (pit_init(100)) {
        write_both("PIT configured at 100 Hz; IRQ0 remains masked.\n");
    } else {
        write_both("PIT initialisation failed; timer remains unavailable.\n");
    }

    if (serial_ready) {
        serial_write("\nArechi OS - Milestone 1\n");
        serial_write("------------------------\n");
        serial_write("Kernel entered through Limine.\n");
    }

    if (framebuffer_ready) {
        fb_write("Arechi OS - Milestone 1\n");
        fb_write("------------------------\n");
        fb_write("Kernel entered through Limine.\n");
    }

    if (!serial_ready && !framebuffer_ready) {
        x86_64_halt_forever();
    }

    write_both("Console initialisation complete.\n");
    write_both("System is halted safely; timer IRQs remain masked.\n");

    x86_64_halt_forever();
}
