#include "fb.h"

static struct limine_terminal_response *terminal_service;
static struct limine_terminal *active_terminal;

bool fb_init(struct limine_framebuffer_response *framebuffer_response,
             struct limine_terminal_response *terminal_response)
{
    if (framebuffer_response == 0 || framebuffer_response->framebuffer_count == 0 ||
        framebuffer_response->framebuffers == 0 ||
        framebuffer_response->framebuffers[0] == 0) {
        return false;
    }

    struct limine_framebuffer *framebuffer = framebuffer_response->framebuffers[0];
    if (framebuffer->address == 0 || framebuffer->width == 0 ||
        framebuffer->height == 0 || framebuffer->pitch == 0 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB) {
        return false;
    }

    if (terminal_response == 0 || terminal_response->terminal_count == 0 ||
        terminal_response->terminals == 0 || terminal_response->terminals[0] == 0 ||
        terminal_response->write == 0) {
        return false;
    }

    active_terminal = terminal_response->terminals[0];
    if (active_terminal->framebuffer == 0) {
        active_terminal = 0;
        return false;
    }

    terminal_service = terminal_response;
    return true;
}

void fb_write(const char *text)
{
    if (text == 0 || terminal_service == 0 || active_terminal == 0) {
        return;
    }

    uint64_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }

    if (length != 0) {
        terminal_service->write(active_terminal, text, length);
    }
}
