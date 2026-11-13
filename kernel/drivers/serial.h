#ifndef ARECHI_SERIAL_H
#define ARECHI_SERIAL_H

#include <stdbool.h>

bool serial_init(void);
void serial_write_char(char character);
void serial_write(const char *text);

#endif
