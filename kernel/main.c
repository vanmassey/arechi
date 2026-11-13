#include <stdbool.h>
#include <stdint.h>

#include "arch.h"
#include "drivers/fb.h"
#include "drivers/serial.h"
#include "gdt.h"
#include "interrupts.h"
#include "limine.h"

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
    write_both("GDT, TSS and IDT initialised; CPU exceptions are now handled.\n");

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
    write_both("System is halted safely; interrupt and memory managers are not yet enabled.\n");

    x86_64_halt_forever();
}
