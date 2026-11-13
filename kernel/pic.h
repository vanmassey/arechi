#ifndef ARECHI_PIC_H
#define ARECHI_PIC_H

#include <stdint.h>

void pic_init(void);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
void pic_send_eoi(uint8_t irq);
int pic_is_spurious(uint8_t irq);

#endif
