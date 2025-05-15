#ifndef __LIB_KERNLE_STDIOKERNEL_H
#define __LIB_KERNEL_STDIOKERNEL_H
#include "stdint.h"

uint32_t printk(const char* format, ...);
void sys_putchar(const char chr);
#endif
