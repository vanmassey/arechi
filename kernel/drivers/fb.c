#include "fb.h"

#include <stdint.h>

#define FONT_WIDTH  5U
#define FONT_HEIGHT 7U
#define CELL_WIDTH  12U
#define CELL_HEIGHT 16U

static struct limine_framebuffer *active_framebuffer;
static uint64_t cursor_x;
static uint64_t cursor_y;

static const uint8_t font[36][FONT_HEIGHT] = {
    /* A-Z */
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    /* 0-9 */
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static const uint8_t *glyph_for(char character, uint8_t punctuation[FONT_HEIGHT])
{
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }

    if (character >= 'A' && character <= 'Z') {
        return font[(unsigned)(character - 'A')];
    }
    if (character >= '0' && character <= '9') {
        return font[26U + (unsigned)(character - '0')];
    }

    for (unsigned i = 0; i < FONT_HEIGHT; ++i) {
        punctuation[i] = 0;
    }

    switch (character) {
        case '-': punctuation[3] = 14; break;
        case '_': punctuation[6] = 31; break;
        case '.': punctuation[6] = 4; break;
        case ',': punctuation[5] = 4; punctuation[6] = 8; break;
        case ':': punctuation[2] = 4; punctuation[5] = 4; break;
        case ';': punctuation[2] = 4; punctuation[5] = 4; punctuation[6] = 8; break;
        case '!': punctuation[0] = 4; punctuation[1] = 4; punctuation[2] = 4;
                  punctuation[3] = 4; punctuation[4] = 4; punctuation[6] = 4; break;
        case '?': punctuation[0] = 14; punctuation[1] = 17; punctuation[2] = 1;
                  punctuation[3] = 2; punctuation[4] = 4; punctuation[6] = 4; break;
        case '/': punctuation[0] = 1; punctuation[1] = 2; punctuation[2] = 2;
                  punctuation[3] = 4; punctuation[4] = 8; punctuation[5] = 8;
                  punctuation[6] = 16; break;
        case '+': punctuation[2] = 4; punctuation[3] = 31; punctuation[4] = 4; break;
        case '=': punctuation[2] = 31; punctuation[4] = 31; break;
        case '(': punctuation[0] = 2; punctuation[1] = 4; punctuation[2] = 8;
                  punctuation[3] = 8; punctuation[4] = 8; punctuation[5] = 4;
                  punctuation[6] = 2; break;
        case ')': punctuation[0] = 8; punctuation[1] = 4; punctuation[2] = 2;
                  punctuation[3] = 2; punctuation[4] = 2; punctuation[5] = 4;
                  punctuation[6] = 8; break;
        case '\'': punctuation[0] = 4; punctuation[1] = 4; break;
        case ' ': break;
        default: punctuation[0] = 31; punctuation[1] = 17; punctuation[2] = 5;
                 punctuation[3] = 4; punctuation[4] = 4; punctuation[6] = 4; break;
    }

    return punctuation;
}

static uint32_t colour_channel(uint8_t value, uint8_t size, uint8_t shift)
{
    if (size == 0 || size > 8) {
        return 0;
    }

    uint32_t maximum = (1U << size) - 1U;
    return (((uint32_t)value * maximum) / 255U) << shift;
}

static void put_pixel(uint64_t x, uint64_t y, uint32_t colour)
{
    if (active_framebuffer == 0 || x >= active_framebuffer->width ||
        y >= active_framebuffer->height) {
        return;
    }

    uint32_t pixel =
        colour_channel((uint8_t)(colour >> 16),
                       active_framebuffer->red_mask_size,
                       active_framebuffer->red_mask_shift) |
        colour_channel((uint8_t)(colour >> 8),
                       active_framebuffer->green_mask_size,
                       active_framebuffer->green_mask_shift) |
        colour_channel((uint8_t)colour,
                       active_framebuffer->blue_mask_size,
                       active_framebuffer->blue_mask_shift);

    uint32_t bytes_per_pixel = active_framebuffer->bpp / 8U;
    volatile uint8_t *destination =
        (volatile uint8_t *)active_framebuffer->address +
        y * active_framebuffer->pitch + x * bytes_per_pixel;

    destination[0] = (uint8_t)pixel;
    destination[1] = (uint8_t)(pixel >> 8);
    destination[2] = (uint8_t)(pixel >> 16);
    if (bytes_per_pixel == 4) {
        destination[3] = (uint8_t)(pixel >> 24);
    }
}

static void fill_screen(uint32_t colour)
{
    for (uint64_t y = 0; y < active_framebuffer->height; ++y) {
        for (uint64_t x = 0; x < active_framebuffer->width; ++x) {
            put_pixel(x, y, colour);
        }
    }
}

static void scroll_console(void)
{
    uint64_t bytes_per_pixel = active_framebuffer->bpp / 8U;
    uint64_t row_bytes = active_framebuffer->pitch;
    volatile uint8_t *pixels = (volatile uint8_t *)active_framebuffer->address;

    if (active_framebuffer->height <= CELL_HEIGHT) {
        fill_screen(0x101820);
        cursor_y = 0;
        return;
    }

    for (uint64_t y = 0; y + CELL_HEIGHT < active_framebuffer->height; ++y) {
        for (uint64_t byte = 0; byte < row_bytes; ++byte) {
            pixels[y * row_bytes + byte] =
                pixels[(y + CELL_HEIGHT) * row_bytes + byte];
        }
    }

    for (uint64_t y = active_framebuffer->height - CELL_HEIGHT;
         y < active_framebuffer->height; ++y) {
        for (uint64_t x = 0; x < active_framebuffer->width; ++x) {
            put_pixel(x, y, 0x101820);
        }
        for (uint64_t byte = active_framebuffer->width * bytes_per_pixel;
             byte < row_bytes; ++byte) {
            pixels[y * row_bytes + byte] = 0;
        }
    }

    if (cursor_y >= CELL_HEIGHT) {
        cursor_y -= CELL_HEIGHT;
    } else {
        cursor_y = 0;
    }
}

static void newline(void)
{
    cursor_x = 0;
    cursor_y += CELL_HEIGHT;
    if (cursor_y + CELL_HEIGHT > active_framebuffer->height) {
        scroll_console();
    }
}

static void draw_character(char character)
{
    uint8_t punctuation[FONT_HEIGHT];
    const uint8_t *glyph = glyph_for(character, punctuation);

    if (cursor_x + CELL_WIDTH > active_framebuffer->width) {
        newline();
    }

    for (uint64_t row = 0; row < FONT_HEIGHT; ++row) {
        for (uint64_t column = 0; column < FONT_WIDTH; ++column) {
            if ((glyph[row] & (1U << (FONT_WIDTH - 1U - column))) != 0) {
                for (uint64_t dy = 0; dy < 2; ++dy) {
                    for (uint64_t dx = 0; dx < 2; ++dx) {
                        put_pixel(cursor_x + 1 + column * 2 + dx,
                                  cursor_y + 1 + row * 2 + dy,
                                  0xE6EDF3);
                    }
                }
            }
        }
    }

    cursor_x += CELL_WIDTH;
}

bool fb_init(struct limine_framebuffer_response *framebuffer_response)
{
    if (framebuffer_response == 0 || framebuffer_response->framebuffer_count == 0 ||
        framebuffer_response->framebuffers == 0 ||
        framebuffer_response->framebuffers[0] == 0) {
        return false;
    }

    struct limine_framebuffer *framebuffer = framebuffer_response->framebuffers[0];
    if (framebuffer->address == 0 || framebuffer->width < CELL_WIDTH ||
        framebuffer->height < CELL_HEIGHT || framebuffer->pitch == 0 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        (framebuffer->bpp != 24 && framebuffer->bpp != 32) ||
        framebuffer->pitch < framebuffer->width * (framebuffer->bpp / 8U) ||
        framebuffer->red_mask_size == 0 || framebuffer->green_mask_size == 0 ||
        framebuffer->blue_mask_size == 0) {
        return false;
    }

    active_framebuffer = framebuffer;
    cursor_x = 0;
    cursor_y = 0;
    fill_screen(0x101820);
    return true;
}

void fb_write(const char *text)
{
    if (text == 0 || active_framebuffer == 0) {
        return;
    }

    while (*text != '\0') {
        char character = *text++;

        if (character == '\n') {
            newline();
        } else if (character == '\r') {
            cursor_x = 0;
        } else if (character == '\t') {
            for (unsigned i = 0; i < 4; ++i) {
                draw_character(' ');
            }
        } else {
            draw_character(character);
        }
    }
}
