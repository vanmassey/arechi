#ifndef ARECHI_FB_H
#define ARECHI_FB_H

#include <stdbool.h>
#include "limine.h"

bool fb_init(struct limine_framebuffer_response *framebuffer_response);
void fb_write(const char *text);

#endif
