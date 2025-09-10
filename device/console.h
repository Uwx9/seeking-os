#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H
#include "stdint.h"

extern uint32_t str_color;
extern uint32_t char_color;

void console_init(void);
void console_acquire(void);
void console_realse(void);
void console_put_str(char* str, uint32_t char_attr);
void console_put_char(uint8_t char_asci, uint32_t char_attr);
void console_put_int(uint32_t num);
#endif