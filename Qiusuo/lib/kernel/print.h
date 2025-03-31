#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H
#include "stdint.h"
void put_char(uint8_t char_asci);       //ecx拿到参数
void put_str(char* message);            //ebx拿到参数
void put_int(uint32_t num);             //eax拿到参数，以十六进制打印 
void set_cursor(uint32_t cursor_pos);
#endif